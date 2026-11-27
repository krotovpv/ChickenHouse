#include <Arduino.h>
#include <esp_task_wdt.h> // Для контроля ватчдога на Ядре 0
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h> // Библиотека для работы с Flash
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include "ModbusServerRTU.h"
#include "registers.h"
#include "gateway_state.h"
#include "mqtt_command_input.h"
#include "reset_button.h"
#include "telegram.h"
#include "web_pages.h"
#include "sunny_logo.h"
#include "table_assets.h"
#include <mbedtls/sha256.h>
#include <esp_ota_ops.h>
#include <Update.h> // Системная библиотека для работы с OTA (шить Flash/FS)

Preferences prefs;
ResponsiveWebServer webServer(80);
ModbusServerRTU MBserver(2000);

DNSServer dnsServer;
const byte DNS_PORT = 53;
static const char* MDNS_HOSTNAME = "chickenhouse";

// Динамические страницы и API нельзя сохранять в кэше браузера.
#define NO_STORE_ROUTE(handler) []() { webServer.sendHeader("Cache-Control", "no-store"); handler(); }

static String resourceHash(const unsigned char* data, size_t length) {
  unsigned char digest[32];
  if (mbedtls_sha256_ret(data, length, digest, 0) != 0) return String();
  char hex[65];
  static const char digits[] = "0123456789abcdef";
  for (size_t i = 0; i < 32; ++i) { hex[i * 2] = digits[digest[i] >> 4]; hex[i * 2 + 1] = digits[digest[i] & 15]; }
  hex[64] = '\0';
  return String(hex);
}
static String logoHash() { static const String v = resourceHash(sunnyLogo, sunnyLogoSize); return v; }
static String styleHash() { static const String v = resourceHash((const unsigned char*)tableStyle, sizeof(tableStyle) - 1); return v; }
static String scriptHash() { static const String v = resourceHash((const unsigned char*)tableScript, sizeof(tableScript) - 1); return v; }
static String resourceUrl(const char* path, const String& hash) { return hash.length() ? String(path) + "?v=" + hash : String(path); }
String sunnyLogoUrl() { return resourceUrl("/sunny-logo.png", logoHash()); }
String tableStyleUrl() { return resourceUrl("/assets/table.css", styleHash()); }
String tableScriptUrl() { return resourceUrl("/assets/table.js", scriptHash()); }
String uiBuildId() {
  static bool initialized = false;
  static String id;
  if (!initialized) {
    initialized = true;
    char buffer[65] = {};
    if (esp_ota_get_app_elf_sha256(buffer, sizeof(buffer)) > 0 && strlen(buffer) == 64 && strspn(buffer, "0123456789abcdef") == 64 && strspn(buffer, "0") != 64) id = buffer;
  }
  return id;
}
static void serveStaticAsset(const unsigned char* data, size_t length, const char* type, const String& hash) {
  if (!hash.length() && webServer.hasArg("v")) {
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(503, "text/plain", "Resource version unavailable; reload page");
    return;
  }
  if (hash.length() && webServer.hasArg("v") && webServer.arg("v") != hash) {
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(410, "text/plain", "Resource version changed; reload page");
    return;
  }
  webServer.sendHeader("Cache-Control", hash.length() && webServer.hasArg("v") ? "public, max-age=86400" : "no-store");
  webServer.send_P(200, type, reinterpret_cast<PGM_P>(data), length);
}

void handleCaptivePortalRequest() {
  // Only redirect requests received via our AP, never clients of the router.
  if (webServer.client().localIP() != WiFi.softAPIP()) {
    webServer.send(404, "text/plain", "Not found");
    return;
  }
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  webServer.send(302, "text/plain", "");
}

// Настройки собственной точки доступа (AP)
String ap_ssid = "ChickenHous";
String ap_password = "12345678";
static const uint8_t FACTORY_RESET_PIN = 27;
static ResetButton factoryResetButton;
static volatile bool factoryResetRequested = false;

void factoryResetButtonTask(void*) {
  for (;;) {
    if (factoryResetButton.update(digitalRead(FACTORY_RESET_PIN) == LOW, millis()))
      factoryResetRequested = true;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Настройки для подключения к роутеру (STA)
String sta_ssid;
String sta_password;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
String mqtt_host, mqtt_portStr, mqtt_user, mqtt_pass, mqtt_topic;
int mqtt_port;

// Настройки для Telegram (значения по умолчанию)
String BOT_TOKEN = "";
String CHAT_ID = "";
uint8_t humidityLimit = 95;
bool humidityEnabled = true;
bool humidityRepeat = false;
uint16_t humidityRepeatMinutes = 30;

bool parseUnsignedArg(const String& text, uint32_t maxValue, uint32_t& value) {
  if (text.length() == 0) return false;
  uint32_t parsed = 0;
  for (size_t i = 0; i < text.length(); ++i) {
    char c = text[i];
    if (c < '0' || c > '9') return false;
    uint32_t digit = c - '0';
    if (digit > maxValue || parsed > (maxValue - digit) / 10) return false;
    parsed = parsed * 10 + digit;
  }
  value = parsed;
  return true;
}

void loadApPassword() {
  if (prefs.begin("configAP", true)) {
    ap_password = prefs.getString("password", "12345678");
    prefs.end();
  }
}

void handleSavePasswords() {
  String kind = webServer.arg("kind");
  String value = webServer.arg("password");
  String confirm = webServer.arg("confirm");
  if (kind != "ap" || value != confirm) {
    webServer.send(400, "text/plain; charset=utf-8", "Пароли не совпадают или неверный тип"); return;
  }
  bool valid = value.length() >= 8 && value.length() <= 63;
  for (size_t i = 0; i < value.length(); ++i) valid &= value[i] >= 32 && value[i] <= 126;
  if (!valid) { webServer.send(400, "text/plain; charset=utf-8", "Пароль точки: от 8 до 63 печатных символов ASCII"); return; }
  if (!prefs.begin("configAP", false)) { webServer.send(500, "text/plain", "Storage error"); return; }
  size_t stored = prefs.putString("password", value);
  prefs.end();
  if (!stored) { webServer.send(500, "text/plain", "Storage error"); return; }
  webServer.send(200, "text/plain; charset=utf-8", "Пароль сохранён. Устройство перезагрузится; переподключитесь к ChickenHous с новым паролем.");
  delay(1000); ESP.restart();
}

void factoryReset() {
  const char* spaces[] = {"configAP", "webAdmin", "configWiFi", "configMQTT2", "configTG", "configAlerts"};
  for (const char* name : spaces) {
    if (!prefs.begin(name, false)) { Serial.println("Factory reset: storage error"); return; }
    bool cleared = prefs.clear();
    prefs.end();
    if (!cleared) { Serial.println("Factory reset: clear failed"); return; }
  }
  ESP.restart();
}

// Хэндлы объектов FreeRTOS для асинхронного Modbus
SemaphoreHandle_t modbusMutex = NULL;
GatewayState gatewayState;

const char* masterStatusName(MasterStatus status) {
  switch (status) {
    case MasterStatus::waiting: return "waiting";
    case MasterStatus::fresh: return "fresh";
    default: return "stale";
  }
}

const char* commandStatusName(CommandStatus status) {
  switch (status) {
    case CommandStatus::pending: return "pending";
    case CommandStatus::completed: return "completed";
    case CommandStatus::unknown: return "unknown";
    default: return "idle";
  }
}

const char* writeResultReason(WriteResult result) {
  switch (result) {
    case WriteResult::readOnly: return "read_only";
    case WriteResult::invalidValue: return "invalid_value";
    case WriteResult::busy: return "register_busy";
    default: return "";
  }
}

// The register check, command state and register update form one transaction.
WriteResult submitCommand(uint16_t address, uint16_t value, bool mqtt, const char* requestId, bool& locked) {
  locked = modbusMutex && xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(100)) == pdTRUE;
  if (!locked) return WriteResult::busy;
  auto it = memo.find(address);
  uint16_t unused = 0;
  WriteResult result = gatewayState.submit(address, value, it == memo.end() ? unused : it->second,
                                           mqtt, requestId, millis());
  xSemaphoreGive(modbusMutex);
  return result;
}

void publishCommandResult(int address, int value, const char* status, const char* reason, const char* requestId) {
  if (!mqttClient.connected()) return;
  JsonDocument doc;
  if (address >= 0) doc["address"] = address; else doc["address"] = nullptr;
  if (value >= 0) doc["value"] = value; else doc["value"] = nullptr;
  doc["status"] = status;
  if (reason && *reason) doc["reason"] = reason;
  if (requestId && *requestId) doc["request_id"] = requestId;
  String payload;
  serializeJson(doc, payload);
  if (!mqttClient.publish((mqtt_topic + "/result").c_str(), payload.c_str()))
    Serial.println("MQTT: не удалось отправить результат команды");
}

void publishMasterStatus() {
  if (!mqttClient.connected() || !modbusMutex || xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
  MasterStatus status = gatewayState.masterStatus(millis());
  xSemaphoreGive(modbusMutex);
  JsonDocument doc;
  doc["master_status"] = masterStatusName(status);
  String payload;
  serializeJson(doc, payload);
  if (!mqttClient.publish((mqtt_topic + "/status").c_str(), payload.c_str()))
    Serial.println("MQTT: не удалось отправить статус Master");
}

bool snapshotRegisters(std::map<uint16_t, uint16_t>& snapshot) {
  if (modbusMutex == NULL || xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
  snapshot = memo;
  xSemaphoreGive(modbusMutex);
  return true;
}

bool snapshotHumidity(uint16_t& humidity, bool& fresh, uint8_t& limit,
                      bool& enabled, bool& repeat, uint16_t& minutes) {
  if (!modbusMutex || xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
  humidity = memo.at(6);
  fresh = gatewayState.masterStatus(millis()) == MasterStatus::fresh;
  limit = humidityLimit; enabled = humidityEnabled;
  repeat = humidityRepeat; minutes = humidityRepeatMinutes;
  xSemaphoreGive(modbusMutex);
  return true;
}

void loadAlertSettings() {
  if (!prefs.begin("configAlerts", true)) return;
  humidityLimit = prefs.getUChar("limit", 95);
  if (humidityLimit > 100) humidityLimit = 95;
  humidityEnabled = prefs.getBool("enabled", true);
  humidityRepeat = prefs.getBool("repeat", false);
  humidityRepeatMinutes = prefs.getUShort("minutes", 30);
  if (!humidityRepeatMinutes || humidityRepeatMinutes > 10080) humidityRepeatMinutes = 30;
  prefs.end();
}

void handleSaveAlerts() {
  uint32_t limit, minutes;
  if (!parseUnsignedArg(webServer.arg("limit"), 100, limit) ||
      !parseUnsignedArg(webServer.arg("minutes"), 10080, minutes) || !minutes) {
    webServer.send(400, "text/plain; charset=utf-8", "Проверьте предел влажности (0–100) и период (1–10080 минут)"); return;
  }
  bool enabled = webServer.hasArg("enabled"), repeat = webServer.hasArg("repeat");
  if (!prefs.begin("configAlerts", false)) { webServer.send(500, "text/plain", "Storage error"); return; }
  bool ok = prefs.putUChar("limit", limit) && prefs.putBool("enabled", enabled) &&
            prefs.putBool("repeat", repeat) && prefs.putUShort("minutes", minutes);
  prefs.end();
  if (!ok) { webServer.send(500, "text/plain", "Storage error"); return; }
  if (!modbusMutex || xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    webServer.send(503, "text/plain", "Settings saved; restart needed"); return;
  }
  humidityLimit = limit; humidityEnabled = enabled; humidityRepeat = repeat; humidityRepeatMinutes = minutes;
  xSemaphoreGive(modbusMutex);
  webServer.sendHeader("Location", "/alerts", true);
  webServer.send(303, "text/plain; charset=utf-8", "Настройки сохранены");
}

// --- Инициализация настроек Telegram из Flash ---
void loadSettingsTelegram() {
  prefs.begin("configTG", true); // Открываем пространство "configTG" в режиме чтения
  BOT_TOKEN = prefs.getString("bot_token", BOT_TOKEN); // Если в памяти пусто, останется дефолт
  CHAT_ID = prefs.getString("chat_id", CHAT_ID);
  prefs.end();
}

// --- Сохранение настроек Telegram в Flash ---
void saveSettingsTelegram(String token, String chatid) {
  prefs.begin("configTG", false); // Открываем для записи
  prefs.putString("bot_token", token);
  prefs.putString("chat_id", chatid);
  prefs.end();
}

// --- Инициализация настроек из Flash ---
void loadSettingsWiFi() {
  prefs.begin("configWiFi", true); // Открываем в режиме чтения (true)
  sta_ssid = prefs.getString("ssid", ""); // Дефолт, если пусто
  sta_password = prefs.getString("pass", "");
  prefs.end();
}
// --- Инициализация настроек из Flash ---
void loadSettingsMQTT() {
  prefs.begin("configMQTT2", true); // Открываем в режиме чтения (true)
  mqtt_user  = prefs.getString("mqtt_user", "");
  mqtt_pass  = prefs.getString("mqtt_pass", "");
  mqtt_host  = prefs.getString("mqtt_host", "mqtt-dashboard.com");
  mqtt_portStr  = prefs.getString("mqtt_portStr", "1883");
  mqtt_port  = mqtt_portStr.toInt();
  mqtt_topic = prefs.getString("mqtt_topic", "esp32/chickenhouse");
  prefs.end();
}
// --- Сохранение в Flash ---
void saveSettingsWiFi(String s, String p) {
  //prefs.begin("config", ...) Параметр false означает доступ на чтение и запись, true — только чтение.
  prefs.begin("configWiFi", false); // Открываем для записи (false)
  prefs.putString("ssid", s);
  prefs.putString("pass", p);
  prefs.end();
}
// --- Сохранение в Flash ---
void saveSettingsMQTT(String mq_u, String mq_pass, String mq_h, String mq_p, String mq_t) {
  //prefs.begin("config", ...) Параметр false означает доступ на чтение и запись, true — только чтение.
  prefs.begin("configMQTT2", false); // Открываем для записи (false)
  prefs.putString("mqtt_user", mq_u);
  prefs.putString("mqtt_pass", mq_pass);
  prefs.putString("mqtt_host", mq_h);
  prefs.putString("mqtt_portStr", mq_p);
  prefs.putString("mqtt_topic", mq_t);
  prefs.end();
}

void publishModbusData() {
  if (mqttClient.connected()) {
    JsonDocument doc;
    std::map<uint16_t, uint16_t> snapshot;
    if (!snapshotRegisters(snapshot)) return;
    for (const auto& item : snapshot) doc[String(item.first)] = item.second;

    String output;
    serializeJson(doc, output);
    if (mqttClient.publish(mqtt_topic.c_str(), output.c_str())) {
      Serial.println("Данные отправлены в MQTT");
    } else {
      Serial.println("Ошибка отправки в MQTT");
    }
  }
}

void reconnectMQTT() {
  // Проверяем WiFi и состояние подключения
  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    Serial.print("Попытка MQTT подключения к ");
    Serial.println(mqtt_host);

    // Создаем уникальный ID на основе MAC-адреса устройства
    // --- НЕОБХОДИМО ГАРАНТИРОВАТЬ УНИКАЛЬНОСТЬ ---
    String clientId = "ESP32_CH_" + WiFi.macAddress();
    clientId.replace(":", ""); // Убираем двоеточия из MAC

    // Пытаемся подключиться (с логином/паролем или без)
    bool connected = false;
    if (mqtt_user.length() > 0) {
      connected = mqttClient.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_pass.c_str());
    } else {
      connected = mqttClient.connect(clientId.c_str());
    }

    if (connected) {
      Serial.println("MQTT подключен!");
      String setTopic = mqtt_topic + "/set";
      mqttClient.subscribe(setTopic.c_str());
      publishMasterStatus();
    } else {
      Serial.print("Ошибка, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) {
    publishCommandResult(-1, -1, "rejected", "invalid_json", nullptr);
    return;
  }
  const char* requestId = nullptr;
  int address = doc["address"].is<uint16_t>() ? doc["address"].as<uint16_t>() : -1;
  int value = doc["value"].is<uint16_t>() ? doc["value"].as<uint16_t>() : -1;
  if (!readRequestId(doc["request_id"], requestId)) {
    publishCommandResult(address, value, "rejected", "invalid_request_id", nullptr);
    return;
  }
  if (!doc["bit"].isNull() || !doc["state"].isNull()) {
    publishCommandResult(address, value, "rejected", "bit_write_forbidden", requestId);
    return;
  }
  if (address < 0 || value < 0) {
    publishCommandResult(address, value, "rejected", "invalid_arguments", requestId);
    return;
  }
  bool locked;
  WriteResult result = submitCommand(address, value, true, requestId, locked);
  publishCommandResult(address, value, result == WriteResult::accepted ? "accepted" : "rejected",
                       locked ? writeResultReason(result) : "lock_busy", requestId);
  if (result == WriteResult::accepted && locked) publishModbusData();
}

// ОБРАБОТЧИК: Чтение (FC 03), Запись (FC 06), Запись массива (FC 16)
ModbusMessage handleModbus(ModbusMessage request) {
  uint8_t  fc = request.getFunctionCode();
  ModbusMessage response;
  if (request.size() < 6) return ModbusMessage(request.getServerID(), fc | 0x80, ILLEGAL_DATA_VALUE);

  uint16_t address = 0, countOrValue = 0;
  if (request.get(2, address, countOrValue) != 6) {
    return ModbusMessage(request.getServerID(), fc | 0x80, ILLEGAL_DATA_VALUE);
  }
  if (modbusMutex == NULL || xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return ModbusMessage(request.getServerID(), fc | 0x80, SERVER_DEVICE_BUSY);
  }

  auto rangeIsDefined = [](uint16_t start, uint16_t count) {
    if (count == 0 || (uint32_t)start + count > 65536UL) return false;
    for (uint32_t i = 0; i < count; ++i) {
      if (memo.find((uint16_t)(start + i)) == memo.end()) return false;
    }
    return true;
  };

  switch (fc) {
    case READ_HOLD_REGISTER: // FC 03
      if (request.size() != 6 || countOrValue > 125 || !rangeIsDefined(address, countOrValue)) {
        response = ModbusMessage(request.getServerID(), fc | 0x80, ILLEGAL_DATA_ADDRESS);
      } else {
        response.add(request.getServerID(), fc, (uint8_t)(countOrValue * 2));
        for (uint16_t i = 0; i < countOrValue; ++i) response.add(memo.find(address + i)->second);
      }
      break;

    case WRITE_HOLD_REGISTER: // FC 06
      if (request.size() != 6 || memo.find(address) == memo.end()) {
        response = ModbusMessage(request.getServerID(), fc | 0x80, ILLEGAL_DATA_ADDRESS);
      } else {
        auto& reg = memo.find(address)->second;
        gatewayState.masterWrite(address, reg, countOrValue, millis());
        reg = countOrValue;
        response = request;
      }
      break;

    case WRITE_MULT_REGISTERS: // FC 16
      if (request.size() < 7 || countOrValue == 0 || countOrValue > 123 || request[6] != countOrValue * 2 || request.size() != 7 + countOrValue * 2 || !rangeIsDefined(address, countOrValue)) {
        response = ModbusMessage(request.getServerID(), fc | 0x80, ILLEGAL_DATA_VALUE);
      } else {
        for (uint16_t i = 0; i < countOrValue; ++i) {
          uint16_t value = 0;
          request.get(7 + i * 2, value);
          auto& reg = memo.find(address + i)->second;
          gatewayState.masterWrite(address + i, reg, value, millis());
          reg = value;
        }
        response.add(request.getServerID(), fc, address, countOrValue);
      }
      break;
    default:
      response = ModbusMessage(request.getServerID(), fc | 0x80, ILLEGAL_FUNCTION);
      break;
  }
  xSemaphoreGive(modbusMutex);
  return response;
}

// API для сканирования сетей
void handleApiScan() {
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray networks = doc.to<JsonArray>();

  for (int i = 0; i < n; ++i) {
    JsonObject net = networks.add<JsonObject>();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
    net["enc"]  = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured";
  }
  
  String json;
  serializeJson(doc, json);
  webServer.send(200, "application/json", json);
}

// API для проверки статуса Wi-Fi и MQTT
void handleApiStatus() {
  webServer.sendHeader("X-UI-Build", uiBuildId());
  JsonDocument doc;
  bool isWifiConnected = (WiFi.status() == WL_CONNECTED);
  if (!modbusMutex || xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
    webServer.send(503, "text/plain", "lock_busy");
    return;
  }
  MasterStatus masterStatus = gatewayState.masterStatus(millis());
  xSemaphoreGive(modbusMutex);
  doc["master_status"] = masterStatusName(masterStatus);
  
  doc["wifi_conn"] = isWifiConnected;
  doc["mqtt_conn"] = mqttClient.connected(); // Статус MQTT
  
  if (isWifiConnected) {
    doc["ssid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
    doc["ip"]   = WiFi.localIP().toString();
  }
  
  String json;
  serializeJson(doc, json);
  webServer.send(200, "application/json", json);
}

void handleApiCommands() {
  if (!modbusMutex || xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
    webServer.send(503, "text/plain", "lock_busy");
    return;
  }
  CommandState states[2] = {gatewayState.command(100), gatewayState.command(101)};
  bool timeout = gatewayState.timedOut(100, millis());
  xSemaphoreGive(modbusMutex);
  JsonDocument doc;
  for (int i = 0; i < 2; ++i) {
    JsonObject command = doc[String(100 + i)].to<JsonObject>();
    command["value"] = states[i].value;
    command["status"] = commandStatusName(states[i].status);
    command["unconfirmed"] = i == 0 && timeout;
  }
  String json;
  serializeJson(doc, json);
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(200, "application/json", json);
}

// Обработка сохранения Wi-Fi
void handleSaveWiFi() {
  if (webServer.hasArg("ssid") && webServer.hasArg("pass")) {
    String newS = webServer.arg("ssid");
    String newP = webServer.arg("pass");

    saveSettingsWiFi(newS, newP);

    String html = getHeader("Сохранение");//"<html><head><meta charset='UTF-8'></head><body>";
    html += "<h2>Готово!</h2><p>Перезагружаюсь...</p></body></html>";
    webServer.send(200, "text/html", html);
    
    Serial.println("Новые настройки записаны. Рестарт...");
    delay(2000);
    ESP.restart(); 
  }
}

// Обработка сохранения MQTT
void handleSaveMQTT() {
  // Проверяем наличие аргумента хоста, который есть в форме MQTT
  if (webServer.hasArg("mq_host")) { 
    String new_Mq_u = webServer.arg("mq_user");
    String new_Mq_pass = webServer.arg("mq_pass");
    if (new_Mq_pass.length() == 0) new_Mq_pass = mqtt_pass;
    String new_Mq_h = webServer.arg("mq_host");
    String new_Mq_p = webServer.arg("mq_port");
    String new_Mq_t = webServer.arg("mq_topic");
    
    saveSettingsMQTT(new_Mq_u, new_Mq_pass, new_Mq_h, new_Mq_p, new_Mq_t);
    
    String html = getHeader("Сохранение");
    html += "<h2>Готово!</h2><p>Настройки MQTT сохранены. Перезагружаюсь...</p></body></html>";
    webServer.send(200, "text/html", html);
    
    Serial.println("MQTT настройки записаны. Рестарт...");
    delay(2000);
    ESP.restart(); 
  }
}

// Обработка сохранения Telegram
void handleSaveTelegram() {
  if (webServer.hasArg("tg_token") && webServer.hasArg("tg_chat")) { 
    String new_token = webServer.arg("tg_token");
    String new_chat  = webServer.arg("tg_chat");
    
    // Удаляем случайные пробелы, которые пользователь мог скопировать
    new_token.trim();
    new_chat.trim();
    if (new_token.length() == 0) new_token = BOT_TOKEN;
    if (new_chat.length() == 0) new_chat = CHAT_ID;
    
    saveSettingsTelegram(new_token, new_chat);
    
    String html = getHeader("Сохранение");
    html += "<h2>Готово!</h2><p>Настройки Telegram сохранены. Перезагружаюсь...</p></body></html>";
    webServer.send(200, "text/html", html);
    
    Serial.println("Настройки Telegram успешно записаны. Рестарт...");
    delay(2000);
    ESP.restart(); 
  } else {
    webServer.send(400, "text/plain", "Bad Request: Missing arguments");
  }
}

// Логика обработки и записи файла во Flash-память
void handleOtaAction() {
  HTTPUpload& upload = webServer.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Начало OTA обновления: %s\n", upload.filename.c_str());
    // Если имя файла содержит "spiffs" или "littlefs", можно переключить U_SPIFFS, но для прошивки используем U_FLASH
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) { 
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { // true - очищает и завершает прошивку
      Serial.printf("OTA Успешно завершено! Размер: %u байт. Перезагрузка...\n", upload.totalSize);
      
      String html = "<html><head><meta charset='UTF-8'><meta http-equiv='refresh' content='10;URL=/'></head>";
      html += "<body style='font-family:sans-serif;text-align:center;padding-top:50px;'>";
      html += "<h2>Успешно! 🎉</h2><p>Устройство обновлено и перезагружается. Вы вернетесь на главную через 10 секунд...</p></body></html>";
      webServer.send(200, "text/html", html);
      
      delay(1000);
      ESP.restart(); // Перезапуск в новую прошивку
    } else {
      Update.printError(Serial);
      webServer.send(500, "text/plain", "OTA Error");
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);
  if (xTaskCreate(factoryResetButtonTask, "reset_button", 2048, nullptr, 1, nullptr) != pdPASS)
    Serial.println("Не удалось запустить опрос кнопки сброса");
  randomSeed(analogRead(0));

  // 1. Загружаем настройки из Flash
  loadSettingsWiFi();
  loadSettingsMQTT();
  loadSettingsTelegram();
  loadApPassword();
  loadAlertSettings();

  // 2. Инициализируем мьютекс защиты ДО запуска задач
  modbusMutex = xSemaphoreCreateMutex();
  if (modbusMutex == NULL) {
    Serial.println("Критическая ошибка: Мьютекс Modbus не создался!");
    while (1);
  }

  // 3. Настройка Wi-Fi
  WiFi.mode(WIFI_AP_STA); // Устанавливаем комбинированный режим
  WiFi.setSleep(false);
  WiFi.softAP(ap_ssid, ap_password); // Запускаем свою точку доступа (всегда активна)
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Точка доступа запущена. SSID: ");
  Serial.println(ap_ssid);
  Serial.print("Адрес для браузера: http://");
  Serial.println(IP); // Обычно это 192.168.4.1

  // mDNS follows Wi-Fi interface events, including a later STA connection.
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("Локальное имя: http://chickenhouse.local/");
  } else {
    Serial.println("Не удалось запустить mDNS; используйте IP-адрес ESP32");
  }

  // Пытаемся подключиться к внешнему роутеру (STA)
  if (sta_ssid != "") {
    WiFi.begin(sta_ssid.c_str(), sta_password.c_str());
    Serial.print("Подключение к " + sta_ssid);

    // Ждем максимум 10 секунд, чтобы не блокировать работу AP
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
      delay(500);
      Serial.print(".");
      retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nПодключено! Локальный IP: " + WiFi.localIP().toString());
    } else {
      Serial.println("\nНе удалось подключиться к роутеру.");
    }
  }

  // 4. Web Server
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP()); // Запускаем DNS сервер, который перенаправляет все домены на IP ESP32
  // HTTP connectivity probes from Android, Apple devices and Windows.
  webServer.on("/generate_204", HTTP_GET, handleCaptivePortalRequest);
  webServer.on("/gen_204", HTTP_GET, handleCaptivePortalRequest);
  webServer.on("/hotspot-detect.html", HTTP_GET, handleCaptivePortalRequest);
  webServer.on("/library/test/success.html", HTTP_GET, handleCaptivePortalRequest);
  webServer.on("/connecttest.txt", HTTP_GET, handleCaptivePortalRequest);
  webServer.on("/ncsi.txt", HTTP_GET, handleCaptivePortalRequest);
  webServer.on("/fwlink", HTTP_GET, handleCaptivePortalRequest);
  webServer.onNotFound(handleCaptivePortalRequest);
  webServer.on("/sunny-logo.png", HTTP_GET, []() { serveStaticAsset(sunnyLogo, sunnyLogoSize, "image/png", logoHash()); });
  webServer.on("/assets/table.css", HTTP_GET, []() { serveStaticAsset((const unsigned char*)tableStyle, sizeof(tableStyle) - 1, "text/css; charset=utf-8", styleHash()); });
  webServer.on("/assets/table.js", HTTP_GET, []() { serveStaticAsset((const unsigned char*)tableScript, sizeof(tableScript) - 1, "application/javascript; charset=utf-8", scriptHash()); });
  webServer.on("/", NO_STORE_ROUTE(handleIndex));
  webServer.on("/autoChickenHous", handleAutoChickenHous);
  webServer.on("/light", NO_STORE_ROUTE(handleLight));
  webServer.on("/alerts", NO_STORE_ROUTE(handleAlerts));
  webServer.on("/ventilation", NO_STORE_ROUTE(handleVentilation));
  webServer.on("/feeding", NO_STORE_ROUTE(handleFeeding));
  webServer.on("/door", NO_STORE_ROUTE(handleDoor));
  webServer.on("/climate", NO_STORE_ROUTE(handleClimate));
  webServer.on("/table", NO_STORE_ROUTE(handleTable));
  webServer.on("/api/data", NO_STORE_ROUTE(handleApiData));
  webServer.on("/api/scan", NO_STORE_ROUTE(handleApiScan));
  webServer.on("/api/status", NO_STORE_ROUTE(handleApiStatus));
  webServer.on("/api/commands", HTTP_GET, NO_STORE_ROUTE(handleApiCommands));
  webServer.on("/settings", NO_STORE_ROUTE(handleSettings));
  webServer.on("/saveWiFi", HTTP_POST, NO_STORE_ROUTE(handleSaveWiFi));
  webServer.on("/saveMQTT", HTTP_POST, NO_STORE_ROUTE(handleSaveMQTT));
  webServer.on("/saveTG", HTTP_POST, NO_STORE_ROUTE(handleSaveTelegram));
  webServer.on("/savePasswords", HTTP_POST, NO_STORE_ROUTE(handleSavePasswords));
  webServer.on("/saveAlerts", HTTP_POST, NO_STORE_ROUTE(handleSaveAlerts));
  webServer.on("/update", HTTP_GET, NO_STORE_ROUTE(handleOtaPage));
  webServer.on("/update_action", HTTP_POST, []() {
    webServer.sendHeader("Cache-Control", "no-store");
    if (Update.hasError()) {
      webServer.send(500, "text/plain; charset=utf-8", "Ошибка обновления прошивки");
    } else {
      webServer.send(200, "text/plain; charset=utf-8", "Обновление завершено. Устройство перезагрузится.");
    }
  }, handleOtaAction);

  // Эндпоинты записи битов и регистров (защищены мьютексом)
  webServer.on("/api/write_bit", HTTP_POST, []() {
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(403, "text/plain", "bit_write_forbidden");
  });
  webServer.on("/api/write_reg", HTTP_POST, []() {
    webServer.sendHeader("Cache-Control", "no-store");
    uint32_t address, value;
    if (!webServer.hasArg("addr") || !webServer.hasArg("value") ||
        !parseUnsignedArg(webServer.arg("addr"), 65535, address) ||
        !parseUnsignedArg(webServer.arg("value"), 65535, value)) {
      webServer.send(400, "text/plain", "invalid_arguments");
      return;
    }
    bool locked;
    WriteResult result = submitCommand(address, value, false, nullptr, locked);
    if (!locked) webServer.send(503, "text/plain", "lock_busy");
    else if (result == WriteResult::accepted) {
      publishModbusData();
      webServer.send(200, "text/plain", "accepted");
    } else {
      int status = result == WriteResult::busy ? 409 : result == WriteResult::readOnly ? 403 : 400;
      webServer.send(status, "text/plain", writeResultReason(result));
    }
  });
  webServer.begin();

  // 5. Modbus (адрес 16(0x10))

  // Настройка Serial2 для RS485 (RX=16, TX=17)
  RTUutils::prepareHardwareSerial(Serial2);
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // Скорость 9600, RX=16, TX=17

  // Привязка обработчиков чтения/записи к ID 16 (0x10)
  MBserver.registerWorker(16, READ_HOLD_REGISTER, &handleModbus);
  MBserver.registerWorker(16, WRITE_HOLD_REGISTER, &handleModbus);
  MBserver.registerWorker(16, WRITE_MULT_REGISTERS, &handleModbus);
  MBserver.begin(Serial2);
  Serial.println("Modbus RTU Server запущен...");

  // 7. MQTT
  mqttClient.setServer(mqtt_host.c_str(), mqtt_port);
  mqttClient.setCallback(mqttCallback); // Регистрация обработчика
  mqttClient.setBufferSize(4096);
  mqttClient.setKeepAlive(30);

  // 8. Telegram
  sendTelegramMessage("ESP32 Chickenhouse включился и доступен.");
  if (xTaskCreate(humidityTelegramTask, "humidity_alert", 8192, nullptr, 1, nullptr) != pdPASS)
    Serial.println("Не удалось запустить контроль влажности");
}

void loop() {
  if (factoryResetRequested) {
    factoryResetRequested = false;
    factoryReset();
  }
  dnsServer.processNextRequest();
  webServer.handleClient();

  // Transfer Modbus events while holding the mutex; perform network I/O only after releasing it.
  CommandEvent completed;
  bool haveCompleted = false;
  MasterStatus currentMasterStatus = MasterStatus::waiting;
  bool haveStatus = false;
  if (modbusMutex && xSemaphoreTake(modbusMutex, 0) == pdTRUE) {
    gatewayState.tick(millis());
    haveCompleted = gatewayState.popCompletion(completed);
    currentMasterStatus = gatewayState.masterStatus(millis());
    haveStatus = true;
    xSemaphoreGive(modbusMutex);
  }
  if (haveCompleted) publishCommandResult(completed.address, completed.value, "completed", nullptr, completed.requestId);
  static MasterStatus lastPublishedStatus = MasterStatus::waiting;
  static uint32_t lastStatusPush = 0;
  if (haveStatus && mqttClient.connected() &&
      (currentMasterStatus != lastPublishedStatus || (uint32_t)(millis() - lastStatusPush) >= 10000)) {
    publishMasterStatus();
    lastPublishedStatus = currentMasterStatus;
    lastStatusPush = millis();
  }

  // Работаем с MQTT-брокером только при наличии стабильного интернет-соединения
  if (WiFi.status() == WL_CONNECTED) {
    // 1. Поддерживаем соединение (reconnect)
    if (!mqttClient.connected()) {
      // 1. Попытка переподключения к брокеру (строго по таймеру раз в 5 секунд, без блокирующих задержек delay)
      static unsigned long lastReconnectAttempt = 0;
      if (millis() - lastReconnectAttempt > 5000) { // Пробуем раз в 5 сек
        lastReconnectAttempt = millis();
        reconnectMQTT();
      }
    } else {
      // 2. Поддержание сессии MQTT и обработка входящих подписок (callback-событий)
      mqttClient.loop();
      
      // 3. Отправка слепка карты памяти Modbus-регистров на брокер по таймеру (раз в 10 секунд)
      static unsigned long lastMqttPush = 0;
      if (millis() - lastMqttPush > 10000) {
        publishModbusData();
        lastMqttPush = millis();
      }
    }
  }

  // Мониторинг статуса подключения Wi-Fi в фоне
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) { // Каждые 10 секунд
    // 1. Контроль связи с домашним роутером и автопереподключение в случае обрыва
    if (WiFi.status() != WL_CONNECTED && sta_ssid != "") {
      Serial.print("Переподключаемся к роутеру...");
      WiFi.begin(sta_ssid.c_str(), sta_password.c_str()); // Автопереподключение
    }
    // 2. Анализ аварийных битов и отправка тревожных алертов в Telegram
    checkTelegram();

    lastCheck = millis();
  }

  delay(1); // Для стабильности
}
