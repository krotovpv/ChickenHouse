#include <Arduino.h>
#include <esp_task_wdt.h> // Для контроля ватчдога на Ядре 0
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h> // Библиотека для работы с Flash
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <PubSubClient.h>
#include "ModbusServerRTU.h"
#include "registers.h"
#include "telegram.h"
#include "web_pages.h"
#include <Update.h> // Системная библиотека для работы с OTA (шить Flash/FS)

Preferences prefs;
WebServer webServer(80);
ModbusServerRTU MBserver(2000);

DNSServer dnsServer;
const byte DNS_PORT = 53;

// Настройки собственной точки доступа (AP)
String ap_ssid = "ChickenHous";
String ap_password = "12345678";

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

// Пароль администратора задаётся пользователем при первом входе в режиме точки доступа.
String webAdminPassword;
String adminSessionToken;
unsigned long adminSessionLastActivity = 0;
const unsigned long ADMIN_SESSION_TIMEOUT_MS = 30UL * 60UL * 1000UL;

bool isAdminModeActive() {
  if (adminSessionToken.length() == 0) return false;
  if (millis() - adminSessionLastActivity > ADMIN_SESSION_TIMEOUT_MS) {
    adminSessionToken = "";
    return false;
  }
  return true;
}

bool hasAdminSession();
bool isCurrentRequestAdmin() { return hasAdminSession(); }

bool hasAdminSession() {
  if (!isAdminModeActive() || !webServer.hasHeader("Cookie")) return false;
  String cookies = webServer.header("Cookie");
  String expected = "CH_ADMIN=" + adminSessionToken;
  int pos = cookies.indexOf(expected);
  if (pos < 0) return false;
  bool startsCookie = pos == 0 || cookies.charAt(pos - 1) == ';' || cookies.charAt(pos - 1) == ' ';
  int end = pos + expected.length();
  bool endsCookie = end == cookies.length() || cookies.charAt(end) == ';' || cookies.charAt(end) == ' ';
  if (!startsCookie || !endsCookie) return false;
  adminSessionLastActivity = millis();
  return true;
}

bool requireAdminSession() {
  if (hasAdminSession()) return true;
  String next = webServer.uri();
  if (next == "/saveWiFi" || next == "/saveMQTT" || next == "/saveTG") next = "/settings";
  else if (next == "/update_action") next = "/update";
  webServer.sendHeader("Location", "/admin?next=" + next, true);
  webServer.send(303, "text/plain; charset=utf-8", "Сначала включите расширенный режим");
  return false;
}

bool requireAdminApiSession(const char* returnPath) {
  if (hasAdminSession()) return true;
  webServer.sendHeader("X-Admin-Next", returnPath);
  webServer.send(401, "text/plain; charset=utf-8", "Требуется расширенный режим");
  return false;
}

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

String safeReturnPath(String requested) {
  const char* allowed[] = {"/", "/settings", "/table", "/autoChickenHous", "/light", "/alerts", "/feeding", "/door", "/climate", "/ventilation", "/update"};
  for (const char* path : allowed) if (requested == path) return requested;
  return "/";
}

void loadAdminPassword() {
  prefs.begin("webAdmin", true);
  webAdminPassword = prefs.getString("password", "");
  prefs.end();
}

void handleAdminPage();
void handleAdminLogin();
void handleAdminLogout();

void handleAdminPage() {
  webServer.sendHeader("Cache-Control", "no-store");
  String next = safeReturnPath(webServer.arg("next"));
  if (hasAdminSession()) {
    if (webServer.hasArg("next")) {
      webServer.sendHeader("Location", next, true);
      webServer.send(303, "text/plain", "Возврат на предыдущую страницу");
      return;
    }
    String html = getHeader("Расширенные возможности");
    html += "<div class='card'><h2>🔓 Режим активен</h2><p>Запись, изменение настроек и OTA доступны в течение 30 минут.</p>";
    html += "<a class='btn' href='/settings'>Перейти к настройкам</a>";
    html += "<form method='POST' action='/admin/disable'><button class='btn' style='background:#b42318' type='submit'>🔒 Перейти в безопасный режим</button></form></div></div></body></html>";
    webServer.send(200, "text/html; charset=utf-8", html);
    return;
  }

  String html = getHeader("Вход в расширенный режим");
  html += "<div class='card'><h2>🔐 Расширенные возможности</h2>";
  if (webAdminPassword.length() == 0) {
    // Начальная настройка разрешена только клиенту, подключённому к точке доступа устройства.
    IPAddress remote = webServer.client().remoteIP();
    IPAddress ap = WiFi.softAPIP();
    if (remote[0] != ap[0] || remote[1] != ap[1] || remote[2] != ap[2]) {
      html += "<p>Первичную настройку пароля администратора выполните через точку доступа устройства.</p></div></div></body></html>";
      webServer.send(403, "text/html; charset=utf-8", html);
      return;
    }
    html += "<p>Задайте пароль администратора. Он понадобится для доступа к записи и обновлению прошивки.</p>";
    html += "<form method='POST' action='/admin/enable'><input type='hidden' name='next' value='" + next + "'><label class='field-label' for='admin-password'>Пароль администратора (не менее 12 символов)</label><input id='admin-password' type='password' name='password' minlength='12' placeholder='Новый пароль' autocomplete='new-password' required><label class='field-label' for='admin-password-confirm'>Повторите пароль</label><input id='admin-password-confirm' type='password' name='confirm' minlength='12' placeholder='Повторите пароль' autocomplete='new-password' required><button class='btn' type='submit'>Сохранить пароль и включить режим</button></form>";
  } else {
    html += "<p>Введите пароль, чтобы временно разрешить запись, изменение настроек и OTA.</p>";
    html += "<form method='POST' action='/admin/enable'><input type='hidden' name='next' value='" + next + "'><label class='field-label' for='admin-password'>Пароль администратора</label><input id='admin-password' type='password' name='password' placeholder='Введите пароль' autocomplete='current-password' required><button class='btn' type='submit'>Включить расширенный режим</button></form>";
  }
  html += "</div></div></body></html>";
  webServer.send(200, "text/html; charset=utf-8", html);
}

void sendAdminLoginError(const String& message, const String& next, bool firstSetup) {
  String html = getHeader("Проверьте пароль");
  html += "<div class='card'><div style='background:#fff1f0;border:1px solid #f1a7a0;border-left:5px solid #c9372c;color:#7a271a;padding:16px;border-radius:8px;margin-bottom:16px;'>";
  html += "<div style='font-size:18px;font-weight:700;margin-bottom:6px;'>Не удалось включить расширенный режим</div><div>";
  html += message;
  html += "</div></div><p style='color:#555;text-align:center;'>";
  html += firstSetup ? "Проверьте, что оба пароля совпадают и содержат не менее 12 символов." : "Проверьте раскладку клавиатуры и повторите ввод пароля.";
  html += "</p><form method='POST' action='/admin/enable'><input type='hidden' name='next' value='" + next + "'>";
  if (firstSetup) {
    html += "<label class='field-label' for='admin-password'>Пароль администратора (не менее 12 символов)</label><input id='admin-password' type='password' name='password' minlength='12' placeholder='Новый пароль' autocomplete='new-password' required><label class='field-label' for='admin-password-confirm'>Повторите пароль</label><input id='admin-password-confirm' type='password' name='confirm' minlength='12' placeholder='Повторите пароль' autocomplete='new-password' required><button class='btn' type='submit'>Попробовать снова</button>";
  } else {
    html += "<label class='field-label' for='admin-password'>Пароль администратора</label><input id='admin-password' type='password' name='password' placeholder='Введите пароль' autocomplete='current-password' autofocus required><button class='btn' type='submit'>Попробовать снова</button>";
  }
  html += "</form><a href='/' class='btn btn-secondary'>Вернуться на главную</a></div></div></body></html>";
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(403, "text/html; charset=utf-8", html);
}

void handleAdminLogin() {
  String password = webServer.arg("password");
  String next = safeReturnPath(webServer.arg("next"));
  if (webAdminPassword.length() == 0) {
    String confirm = webServer.arg("confirm");
    IPAddress remote = webServer.client().remoteIP();
    IPAddress ap = WiFi.softAPIP();
    if (remote[0] != ap[0] || remote[1] != ap[1] || remote[2] != ap[2] || password.length() < 12 || password != confirm) {
      if (remote[0] != ap[0] || remote[1] != ap[1] || remote[2] != ap[2]) {
        webServer.send(403, "text/plain; charset=utf-8", "Первичную настройку пароля выполните через точку доступа устройства.");
        return;
      }
      sendAdminLoginError("Пароли не совпадают или слишком короткие.", next, true);
      return;
    }
    prefs.begin("webAdmin", false);
    prefs.putString("password", password);
    prefs.end();
    webAdminPassword = password;
  } else if (password != webAdminPassword) {
    delay(300);
    sendAdminLoginError("Введён неверный пароль.", next, false);
    return;
  }

  adminSessionToken = String((uint32_t)esp_random(), HEX) + String((uint32_t)esp_random(), HEX) + String((uint32_t)esp_random(), HEX) + String((uint32_t)esp_random(), HEX);
  adminSessionLastActivity = millis();
  webServer.sendHeader("Set-Cookie", "CH_ADMIN=" + adminSessionToken + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=1800");
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.sendHeader("Location", next, true);
  webServer.send(303, "text/plain", "Режим расширенных возможностей включён");
}

void handleAdminLogout() {
  if (hasAdminSession()) adminSessionToken = "";
  webServer.sendHeader("Set-Cookie", "CH_ADMIN=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
  webServer.sendHeader("Location", "/", true);
  webServer.send(303, "text/plain", "Безопасный режим включён");
}

// Хэндлы объектов FreeRTOS для асинхронного Modbus
SemaphoreHandle_t modbusMutex = NULL;

bool snapshotRegisters(std::map<uint16_t, uint16_t>& snapshot) {
  if (modbusMutex == NULL || xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
  snapshot = memo;
  xSemaphoreGive(modbusMutex);
  return true;
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
    } else {
      Serial.print("Ошибка, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    JsonDocument doc;
    if (deserializeJson(doc, payload, length)) {
      Serial.println("MQTT: некорректный JSON");
      return;
    }

    // Проверяем наличие базовых полей: адрес регистра обязателен
    if (doc["address"].is<uint16_t>()) {
      uint16_t addr = doc["address"];
        
      bool changed = false;
      if (xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        auto registerIt = memo.find(addr);
        // Сценарий 1: Изменение конкретного бита (флага)
        // Ожидаем JSON: {"address": 3, "bit": 0, "state": 1}
        if (registerIt != memo.end() && doc["bit"].is<uint8_t>() && (doc["state"].is<bool>() || doc["state"].is<uint8_t>())) {
            uint8_t bitNum = doc["bit"].as<uint8_t>();
            int bitStateValue = doc["state"].is<bool>() ? (doc["state"].as<bool>() ? 1 : 0) : doc["state"].as<int>();
            
            if (bitNum < 16 && (bitStateValue == 0 || bitStateValue == 1)) {
                uint16_t currentVal = registerIt->second;
                if (bitStateValue) {
                    currentVal |= (1U << bitNum);
                } else {
                    currentVal &= ~(1U << bitNum);
                }
                registerIt->second = currentVal;
                changed = true;
                Serial.printf("MQTT Bit Write: Регистр %d, Бит %d установлен в %d\n", addr, bitNum, bitStateValue);
            }
        } 
        else if (registerIt != memo.end() && doc["value"].is<uint16_t>()) {
            uint16_t val = doc["value"].as<uint16_t>();
            registerIt->second = val;
            changed = true;
            Serial.printf("MQTT Write: Регистр %d установлен в %d\n", addr, val);
        }
        xSemaphoreGive(modbusMutex);
        if (changed) publishModbusData();
      }
    }
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
        memo.find(address)->second = countOrValue;
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
          memo.find(address + i)->second = value;
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
  JsonDocument doc;
  bool isWifiConnected = (WiFi.status() == WL_CONNECTED);
  
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

// Обработка сохранения Wi-Fi
void handleSaveWiFi() {
  if (!requireAdminSession()) return;
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
  if (!requireAdminSession()) return;
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
  if (!requireAdminSession()) return;
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
  static bool uploadAuthorized = false;
  HTTPUpload& upload = webServer.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    uploadAuthorized = hasAdminSession();
    if (!uploadAuthorized) {
      webServer.sendHeader("Location", "/admin?next=/update", true);
      webServer.send(303, "text/plain; charset=utf-8", "Сначала включите расширенный режим");
      return;
    }
    Serial.printf("Начало OTA обновления: %s\n", upload.filename.c_str());
    // Если имя файла содержит "spiffs" или "littlefs", можно переключить U_SPIFFS, но для прошивки используем U_FLASH
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) { 
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!uploadAuthorized) return;
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!uploadAuthorized) return;
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
  randomSeed(analogRead(0));

  // 1. Загружаем настройки из Flash
  loadSettingsWiFi();
  loadSettingsMQTT();
  loadSettingsTelegram();
  loadAdminPassword();

  // 2. Инициализируем мьютекс защиты ДО запуска задач
  modbusMutex = xSemaphoreCreateMutex();
  if (modbusMutex == NULL) {
    Serial.println("Критическая ошибка: Мьютекс Modbus не создался!");
    while (1);
  }

  // 3. Настройка Wi-Fi
  WiFi.mode(WIFI_AP_STA); // Устанавливаем комбинированный режим
  WiFi.softAP(ap_ssid, ap_password); // Запускаем свою точку доступа (всегда активна)
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Точка доступа запущена. SSID: ");
  Serial.println(ap_ssid);
  Serial.print("Адрес для браузера: http://");
  Serial.println(IP); // Обычно это 192.168.4.1

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
  // Запросы проверки связи от Android / Windows / iOS
  webServer.on("/generate_204", handleIndex); 
  webServer.on("/fwlink", handleIndex);
  webServer.onNotFound([]() {
    // Если запрос пришел не на главную, а на какой-то проверочный домен Google/Apple
    // Перенаправляем телефон на наш IP-адрес шлюза
    webServer.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
    webServer.send(302, "text/plain", ""); 
  });
  //webServer.onNotFound(handleIndex); // Любой неизвестный путь ведет на главную
  webServer.on("/", handleIndex);
  webServer.on("/autoChickenHous", handleAutoChickenHous);
  webServer.on("/light", handleLight);
  webServer.on("/alerts", handleAlerts);
  webServer.on("/ventilation", handleVentilation);
  webServer.on("/feeding", handleFeeding);
  webServer.on("/door", handleDoor);
  webServer.on("/climate", handleClimate);
  webServer.on("/table", handleTable);
  webServer.on("/api/data", handleApiData);
  webServer.on("/api/scan", handleApiScan);
  webServer.on("/api/status", handleApiStatus);
  webServer.on("/settings", handleSettings);
  webServer.on("/admin", HTTP_GET, handleAdminPage);
  webServer.on("/admin/enable", HTTP_POST, handleAdminLogin);
  webServer.on("/admin/disable", HTTP_POST, handleAdminLogout);
  webServer.on("/saveWiFi", HTTP_POST, handleSaveWiFi);
  webServer.on("/saveMQTT", HTTP_POST, handleSaveMQTT);
  webServer.on("/saveTG", HTTP_POST, handleSaveTelegram);
  webServer.on("/update", HTTP_GET, handleOtaPage);
  webServer.on("/update_action", HTTP_POST, []() {
    if (!hasAdminSession()) {
      webServer.sendHeader("Location", "/admin?next=/update", true);
      webServer.send(303, "text/plain; charset=utf-8", "Сначала включите расширенный режим");
    } else if (Update.hasError()) {
      webServer.send(500, "text/plain; charset=utf-8", "Ошибка обновления прошивки");
    } else {
      webServer.send(200, "text/plain; charset=utf-8", "Обновление завершено. Устройство перезагрузится.");
    }
  }, handleOtaAction);

  // Эндпоинты записи битов и регистров (защищены мьютексом)
  webServer.on("/api/write_bit", HTTP_POST, []() {
    if (!requireAdminApiSession("/table")) return;
    if (webServer.hasArg("addr") && webServer.hasArg("bit") && webServer.hasArg("state")) {
      uint32_t addrValue, bitValue, stateValue;
      if (!parseUnsignedArg(webServer.arg("addr"), 65535, addrValue) ||
          !parseUnsignedArg(webServer.arg("bit"), 15, bitValue) ||
          !parseUnsignedArg(webServer.arg("state"), 1, stateValue)) {
        webServer.send(400, "text/plain", "Invalid arguments");
        return;
      }
      uint16_t addr = static_cast<uint16_t>(addrValue);
      uint8_t bit = static_cast<uint8_t>(bitValue);
        
      if (xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        auto registerIt = memo.find(addr);
        if (registerIt == memo.end()) {
          xSemaphoreGive(modbusMutex);
          webServer.send(404, "text/plain", "Not Found");
          return;
        }
        if (stateValue) registerIt->second |= (1U << bit);
        else registerIt->second &= ~(1U << bit);
        xSemaphoreGive(modbusMutex);
        publishModbusData();
        webServer.send(200, "text/plain", "OK");
      } else {
        webServer.send(503, "text/plain", "Busy");
      }
    } else webServer.send(400, "text/plain", "Missing arguments");
  });
  webServer.on("/api/write_reg", HTTP_POST, []() {
    if (!requireAdminApiSession("/table")) return;
    if (webServer.hasArg("addr") && webServer.hasArg("value")) {
        uint32_t addrValue, registerValue;
        if (!parseUnsignedArg(webServer.arg("addr"), 65535, addrValue) ||
            !parseUnsignedArg(webServer.arg("value"), 65535, registerValue)) {
            webServer.send(400, "text/plain", "Invalid arguments");
            return;
        }
        uint16_t addr = static_cast<uint16_t>(addrValue);
        uint16_t val = static_cast<uint16_t>(registerValue);
        
        if (xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (memo.count(addr)) {
                memo[addr] = val;
                xSemaphoreGive(modbusMutex);
                publishModbusData(); 
                webServer.send(200, "text/plain", "OK");
            } else {
                xSemaphoreGive(modbusMutex);
                webServer.send(404, "text/plain", "Not Found");
            }
        } else {
            webServer.send(503, "text/plain", "Busy");
        }
    } else webServer.send(400, "text/plain", "Missing arguments");
  });
  const char* headerKeys[] = {"Cookie"};
  webServer.collectHeaders(headerKeys, 1);
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
}

void loop() {
  dnsServer.processNextRequest(); // Обработка DNS-запросов (перехват доменов для Captive Portal)
  webServer.handleClient(); // Обработка входящих веб-запросов от пользователей (смартфоны/ПК)

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
