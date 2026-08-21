#include <Arduino.h>
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
String mqtt_host, portStr, mqtt_user, mqtt_pass, mqtt_topic;
int mqtt_port;

// --- Инициализация настроек из Flash ---
void loadSettingsWiFi() {
  prefs.begin("configWiFi", true); // Открываем в режиме чтения (true)
  sta_ssid = prefs.getString("ssid", ""); // Дефолт, если пусто
  sta_password = prefs.getString("pass", "");
  prefs.end();
}
// --- Инициализация настроек из Flash ---
void loadSettingsMQTT() {
  prefs.begin("configMQTT", true); // Открываем в режиме чтения (true)
  mqtt_user  = prefs.getString("mqtt_user", "");
  mqtt_pass  = prefs.getString("mqtt_pass", "");
  mqtt_host  = prefs.getString("mqtt_host", "mqtt-dashboard.com");
  portStr  = prefs.getString("mqtt_port", "1883");
  mqtt_port  = portStr.toInt();
  mqtt_topic = prefs.getString("mqtt_topic", "esp32/chickenhouse");
  prefs.end();
  // mqtt_user = "";
  // mqtt_pass = "";
  // mqtt_host = "test.mosquitto.org";
  // mqtt_port = 1883;
  // mqtt_topic = "esp32/chickenhouse";
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
  prefs.begin("configMQTT", false); // Открываем для записи (false)
  prefs.putString("mqtt_user", mq_u);
  prefs.putString("mqtt_pass", mq_pass);
  prefs.putString("mqtt_host", mq_h);
  prefs.putString("mqtt_port", mq_p);
  prefs.putString("mqtt_topic", mq_t);
  prefs.end();
}

void publishModbusData() {
  if (mqttClient.connected()) {
    JsonDocument doc;
    for (const auto& item : memo) {
      doc[String(item.first)] = item.second;
    }
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
    deserializeJson(doc, payload, length);

    // Проверяем наличие базовых полей: адрес регистра обязателен
    if (doc["address"].is<uint16_t>()) {
        uint16_t addr = doc["address"];
        
        // Сценарий 1: Изменение конкретного бита (флага)
        // Ожидаем JSON: {"address": 3, "bit": 0, "state": 1}
        if (doc["bit"].is<int16_t>() && doc["state"].is<int16_t>()) {
            uint8_t bitNum = doc["bit"];
            bool bitState = doc["state"];
            
            if (bitNum < 16) { // Проверка диапазона (регистр 16 бит)
                uint16_t currentVal = memo[addr];
                if (bitState) {
                    currentVal |= (1UL << bitNum);  // Установить бит в 1
                } else {
                    currentVal &= ~(1UL << bitNum); // Сбросить бит в 0
                }
                memo[addr] = currentVal;
                Serial.printf("MQTT Bit Write: Регистр %d, Бит %d установлен в %d\n", addr, bitNum, bitState);
            }
        } 
        // Сценарий 2: Полная перезапись значения (как было раньше)
        // Ожидаем JSON: {"address": 3, "value": 123}
        else if (doc["value"].is<int16_t>()) {
            uint16_t val = doc["value"];
            memo[addr] = val;
            Serial.printf("MQTT Write: Регистр %d установлен в %d\n", addr, val);
        }

        // Отправляем подтверждение об обновлении данных
        publishModbusData(); 
    }
}

// ОБРАБОТЧИК: Чтение (FC 03), Запись (FC 06), Запись массива (FC 16)
ModbusMessage handleModbus(ModbusMessage request) {
  uint8_t  fc = request.getFunctionCode();
  uint16_t address, countORvalue;

  request.get(2, address); // Получаем стартовый адрес
  request.get(4, countORvalue);   // Получаем кол-во (для FC03/16) или задаем значение (для FC06)

  ModbusMessage response;
  // Проверка на наличие регистра в нашей карте (для одиночных операций)
  if (fc == WRITE_HOLD_REGISTER || fc == READ_HOLD_REGISTER) {
    if (memo.find(address) == memo.end()) {
      return ModbusMessage(request.getServerID(), fc | 0x80, ILLEGAL_DATA_ADDRESS);
    }
  }

  switch (fc) {
    case READ_HOLD_REGISTER: // FC 03
      response.add(request.getServerID(), fc, (uint8_t)(countORvalue * 2));
      for (uint16_t i = 0; i < countORvalue; ++i) {
        // Если регистра нет в map, вернем 0
        uint16_t val = (memo.count(address + i)) ? memo[address + i] : 0;
        response.add(val);
      }
      break;

    case WRITE_HOLD_REGISTER: // FC 06
      memo[address] = countORvalue; 
      response = request;
      break;

    case WRITE_MULT_REGISTERS: // FC 16
      for (uint16_t i = 0; i < countORvalue; ++i) {
        uint16_t val;
        request.get(7 + (i * 2), val);
        memo[address + i] = val; // Добавит или обновит ключ в map
      }
      response.add(request.getServerID(), fc, address, countORvalue);
      break;
  }
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

void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(0));

  // 1. Загружаем настройки из Flash
  loadSettingsWiFi();
  loadSettingsMQTT();

  // 2. Настройка Wi-Fi
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

  // 3. Web Server
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP()); // Запускаем DNS сервер, который перенаправляет все домены на IP ESP32
  webServer.onNotFound(handleIndex); // Любой неизвестный путь ведет на главную
  webServer.on("/", handleIndex);
  webServer.on("/autoChickenHous", handleAutoChickenHous);
  webServer.on("/light", handleLight);
  webServer.on("/alerts", handleAlerts);
  webServer.on("/status", handleStatus);
  webServer.on("/feeding", handleFeeding);
  webServer.on("/door", handleDoor);
  webServer.on("/climate", handleClimate);
  webServer.on("/table", handleTable);
  webServer.on("/api/data", handleApiData);
  webServer.on("/api/scan", handleApiScan);
  webServer.on("/api/status", handleApiStatus);
  webServer.on("/settings", handleSettings);
  webServer.on("/saveWiFi", HTTP_POST, handleSaveWiFi);
  webServer.on("/saveMQTT", HTTP_POST, handleSaveMQTT);
  webServer.on("/api/write_bit", HTTP_GET, []() {
    if (webServer.hasArg("addr") && webServer.hasArg("bit") && webServer.hasArg("state")) {
        uint16_t addr = webServer.arg("addr").toInt();
        uint8_t bit = webServer.arg("bit").toInt();
        bool state = webServer.arg("state").toInt();
        
        if (state) memo[addr] |= (1UL << bit);
        else memo[addr] &= ~(1UL << bit);
        
        publishModbusData(); // Синхронизируем с MQTT
        webServer.send(200, "text/plain", "OK");
    }
  });
  webServer.on("/api/write_reg", HTTP_GET, []() {
    if (webServer.hasArg("addr") && webServer.hasArg("value")) {
        uint16_t addr = webServer.arg("addr").toInt();
        uint16_t val = webServer.arg("value").toInt();
        
        if (memo.count(addr)) {
            memo[addr] = val;
            publishModbusData(); // Синхронизация с MQTT
            webServer.send(200, "text/plain", "OK");
        } else {
            webServer.send(404, "text/plain", "Not Found");
        }
    }
});
  webServer.begin();

  // 4. Modbus (адрес 16(0x10))

  // Настройка Serial2 для RS485 (RX=16, TX=17)
  RTUutils::prepareHardwareSerial(Serial2);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  // Регистрируем воркеры на функции для сервера с ID 10
  MBserver.registerWorker(16, READ_HOLD_REGISTER, &handleModbus);
  MBserver.registerWorker(16, WRITE_HOLD_REGISTER, &handleModbus);
  MBserver.registerWorker(16, WRITE_MULT_REGISTERS, &handleModbus);

  MBserver.begin(Serial2);
  Serial.println("Modbus RTU Server запущен...");

  // 5. MQTT
  mqttClient.setServer(mqtt_host.c_str(), mqtt_port);
  mqttClient.setCallback(mqttCallback); // Регистрация обработчика
  mqttClient.setBufferSize(2048);
}

void loop() {
  dnsServer.processNextRequest(); // Обработка DNS запросов
  webServer.handleClient(); // Обработка веб-запросов

  // Работаем с MQTT только если есть интернет
  if (WiFi.status() == WL_CONNECTED) {
    // 1. Поддерживаем соединение (reconnect)
    if (!mqttClient.connected()) {
      static unsigned long lastReconnectAttempt = 0;
      if (millis() - lastReconnectAttempt > 5000) { // Пробуем раз в 5 сек
        lastReconnectAttempt = millis();
        reconnectMQTT();
      }
    } else {
      // 2. Обработка входящих сообщений (callback)
      mqttClient.loop();
      
      // 3. Отправка данных по таймеру (раз в 10 сек)
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
    // 1. Проверка Wi-Fi
    if (WiFi.status() != WL_CONNECTED && sta_ssid != "") {
      Serial.print("Переподключаемся к роутеру...");
      WiFi.begin(sta_ssid.c_str(), sta_password.c_str()); // Автопереподключение
    }
    // 2. Проверяем нужно ли сообщать в телеграм
    checkTelegram();

    lastCheck = millis();
  }

  delay(1); // Для стабильности
}