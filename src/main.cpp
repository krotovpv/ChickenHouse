#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h> // Библиотека для работы с Flash
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <PubSubClient.h>
#include "ModbusServerRTU.h"
#include <map>

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
String mqtt_host, mqtt_user, mqtt_pass, mqtt_topic;
int mqtt_port;

//<Адрес_Регистра, Значение>
std::map<uint16_t, uint16_t> memo = {
  {0, 0},{1, 0},{2, 0},{3, 0},{4, 0},{5, 0},{6, 0},{7, 0},{8, 0},{9, 0},
  {10, 0},{11, 0},{12, 0},{13, 0},{14, 0},{15, 0},{16, 0},{17, 0},{18, 0},{19, 0},
  {20, 0},{21, 0},{22, 0},{23, 0},{24, 0},{25, 0},{26, 0},{27, 0},{28, 0},{29, 0},
  {30, 0},{31, 0},{32, 0},{33, 0},{34, 0},{35, 0},{36, 0},{37, 0},{38, 0},{39, 0},
  {40, 0},{41, 0},{42, 0},{43, 0},{44, 0},{45, 0},{46, 0},{47, 0},{48, 0},{49, 0},
  {50, 0},{51, 0},{52, 0},{53, 0},{54, 0},{55, 0},{56, 0},{57, 0},{58, 0},{59, 0},
  {60, 0},{61, 0},{62, 0},{63, 0},{64, 0},{65, 0},{66, 0},{67, 0},{68, 0},{69, 0},
  {70, 0},{71, 0},{72, 0},{73, 0},{74, 0},{75, 0},{76, 0},{77, 0},{78, 0},{79, 0},
  {80, 0},{81, 0},{82, 0},{83, 0},{84, 0},{85, 0},{86, 0},{87, 0},{88, 0},{89, 0},
  {90, 0},{91, 0},{92, 0},{93, 0},{94, 0},{95, 0},{96, 0},{97, 0},{98, 0},{99, 0},
  {100, 0},{101, 0},
  {110, 0},{111, 0},{112, 0},{113, 0},{114, 0},{115, 0},{116, 0},{117, 0},{118, 0},{119, 0},
  {120, 0},{121, 0},{122, 0},{123, 0},{124, 0},{125, 0},{126, 0},{127, 0},{128, 0},{129, 0},
  {130, 0},{131, 0},{132, 0},{133, 0},{134, 0},{135, 0},{136, 0},{137, 0},{138, 0},{139, 0},
  {140, 0},{141, 0},{142, 0},{143, 0},{144, 0},{145, 0},{146, 0},{147, 0},{148, 0},{149, 0},
  {150, 0},{151, 0}
};

// --- Инициализация настроек из Flash ---
void loadSettings() {
  prefs.begin("configWiFi", true); // Открываем в режиме чтения (true)
  sta_ssid = prefs.getString("ssid", ""); // Дефолт, если пусто
  sta_password = prefs.getString("pass", "");
  prefs.end();

  prefs.begin("configMQTT2", true); // Открываем в режиме чтения (true)
  mqtt_user  = prefs.getString("mqtt_user", "");
  mqtt_pass  = prefs.getString("mqtt_pass", "");
  mqtt_host  = prefs.getString("mqtt_host", "mqtt-dashboard.com");
  mqtt_port  = prefs.getInt("mqtt_port", 1883);
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

// --- Обработчики страниц ---

// --- Вспомогательная функция для HTML обертки ---
String getHeader(String title) {
  return "<html><head><meta charset='UTF-8'>"
         "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
         "<style>"
         "  :root { --primary: #007bff; --bg: #f4f7f6; --text: #333; --card: #ffffff; }"
         "  body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }"
         "  .container { width: 100%; max-width: 500px; }"
         "  h2, h3 { color: #444; margin-bottom: 15px; text-align: center; }"
         "  .card { background: var(--card); border-radius: 12px; padding: 20px; box-shadow: 0 4px 15px rgba(0,0,0,0.05); margin-bottom: 20px; border: 1px solid #eee; }"
         "  .btn { display: block; text-align: center; padding: 12px; background: var(--primary); color: white; text-decoration: none; border-radius: 8px; font-weight: 600; margin: 10px 0; transition: 0.3s; border: none; cursor: pointer; }"
         "  .btn:hover { opacity: 0.9; transform: translateY(-1px); }"
         "  .btn-secondary { background: #6c757d; }"
         "  input, select { width: 100%; padding: 12px; margin: 8px 0; border: 1px solid #ddd; border-radius: 8px; box-sizing: border-box; font-size: 16px; }"
         "  table { width: 100%; border-collapse: collapse; background: var(--card); border-radius: 8px; overflow: hidden; }"
         "  th { background: #f8f9fa; padding: 12px; font-size: 13px; color: #888; text-transform: uppercase; border-bottom: 2px solid #eee; }"
         "  td { padding: 14px; border-bottom: 1px solid #eee; text-align: center; font-weight: 500; }"
         "  .status-row { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid #fafafa; }"
         "  .badge { padding: 4px 10px; border-radius: 20px; font-size: 12px; color: white; }"
         "  .bg-success { background: #28a745; } .bg-danger { background: #dc3545; }"
         "</style><title>" + title + "</title></head><body>"
         "<div class='container'>"
         "<a href='/' class='btn btn-secondary' style='margin-bottom:20px;'>🏠 Главная</a>";
}

// 1. Главная страница
void handleIndex() {
  String html = getHeader("Панель управления");
  
  // 1. Стили (проверьте, чтобы этот блок был внутри функции)
  html += R"rawliteral(
  <style>
    .status-card { margin-bottom: 20px; }
    .status-row { display: flex; justify-content: space-between; align-items: flex-start; padding: 12px 0; border-bottom: 1px solid #f4f4f4; }
    .status-row:last-child { border-bottom: none; }
    
    .ip-info { font-size: 0.85em; color: #666; margin-top: 5px; line-height: 1.5; }
    .ip-info b { color: #333; font-family: monospace; }

    /* Шкала WiFi */
    .sig-box { display: flex; align-items: flex-end; height: 20px; gap: 3px; margin-top: 5px; }
    .bar { width: 5px; background: #e0e0e0; border-radius: 1px; transition: 0.3s; }
    .b1 { height: 6px; } .b2 { height: 10px; } .b3 { height: 15px; } .b4 { height: 20px; }
    /* Цвета полосок */
    .green { background: #28a745 !important; }
    .yellow { background: #ffc107 !important; }
    .red { background: #dc3545 !important; }

    /* Индикатор MQTT */
    .dot { height: 10px; width: 10px; border-radius: 50%; display: inline-block; margin-right: 8px; background: #bbb; vertical-align: middle; }
    .online { background: #28a745; box-shadow: 0 0 8px rgba(40,167,69,0.4); }
    .offline { background: #dc3545; }
    
    .nav-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
    .nav-btn { padding: 20px; text-align: center; border-radius: 12px; color: white; text-decoration: none; font-weight: 600; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
  </style>
  )rawliteral";

  html += "<div class='container'>";
  
  // 2. Карточка статуса
  html += "<div class='card status-card'>";
  
  // Блок WiFi
  html += "  <div class='status-row'>";
  html += "    <div>";
  html += "      <span>Сеть: <b id='ssid-name'>" + (sta_ssid != "" ? sta_ssid : "Ожидание...") + "</b></span>";
  html += "      <div class='ip-info'>";
  html += "        Локальный IP: <b id='sta-ip'>...</b><br>";
  html += "        Точка (AP) IP: <b>" + WiFi.softAPIP().toString() + "</b>";
  html += "      </div>";
  html += "    </div>";
  html += "    <div class='sig-box' id='wifi-bars'>";
  html += "      <div class='bar b1'></div><div class='bar b2'></div><div class='bar b3'></div><div class='bar b4'></div>";
  html += "    </div>";
  html += "  </div>";
  
  // Блок MQTT
  html += "  <div class='status-row'>";
  html += "    <span>Статус MQTT:</span>";
  html += "    <span><span id='mqtt-dot' class='dot'></span><b id='mqtt-stat'>Подключение...</b></span>";
  html += "  </div>";
  html += "</div>";

  // 3. Кнопки
  html += "<div class='nav-grid'>";
  html += "  <a href='/autoChickenHous' class='nav-btn' style='background:#d97706;'>🐓<br>Автоматизация курятника</a>";
  html += "  <a href='/table' class='nav-btn' style='background:#4e73df;'>📊<br>Данные</a>";
  html += "  <a href='/settings' class='nav-btn' style='background:#1cc88a;'>⚙️<br>Настройки</a>";
  html += "</div>";

  // 4. Скрипт обновления (Исправленный)
  html += R"rawliteral(
  <script>
    function update() {
      fetch('/api/status').then(r => r.json()).then(d => {
        // Обновление WiFi
        const bars = document.querySelectorAll('.bar');
        const ssidEl = document.getElementById('ssid-name');
        const ipEl = document.getElementById('sta-ip');

        if (d.wifi_conn) {
          ssidEl.innerText = d.ssid;
          ipEl.innerText = d.ip;
          
          let count = 0, colorClass = '';
          if (d.rssi >= -60) { count = 4; colorClass = 'green'; }
          else if (d.rssi >= -75) { count = 3; colorClass = 'green'; }
          else if (d.rssi >= -85) { count = 2; colorClass = 'yellow'; }
          else { count = 1; colorClass = 'red'; }
          
          bars.forEach((b, i) => {
            b.classList.remove('green', 'yellow', 'red');
            if (i < count) b.classList.add(colorClass);
          });
        } else {
          ssidEl.innerHTML = '<span style="color:#dc3545">Отключено</span>';
          ipEl.innerText = "не присвоен";
          bars.forEach(b => b.classList.remove('green', 'yellow', 'red'));
        }

        // Обновление MQTT
        const dot = document.getElementById('mqtt-dot');
        const stat = document.getElementById('mqtt-stat');
        if (d.mqtt_conn) {
          dot.className = 'dot online';
          stat.innerText = 'В сети';
        } else {
          dot.className = 'dot offline';
          stat.innerText = 'Оффлайн';
        }
      }).catch(err => console.error("Ошибка API:", err));
    }
    setInterval(update, 3000);
    update();
  </script>
  )rawliteral";
  
  html += "</div></body></html>";
  webServer.send(200, "text/html", html);
}

// Эндпоинт для отдачи данных в формате JSON
void handleApiData() {
  JsonDocument doc;
  for (const auto& item : memo) {
    doc[String(item.first)] = item.second;
  }
  String json;
  serializeJson(doc, json);
  webServer.send(200, "application/json", json);
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

// 2. Страница с таблицей
void handleTable() {
    String html = getHeader("Мониторинг регистров");
    html += "<h2>Данные в реальном времени</h2>";
    
    // 1. Добавляем стили для индикаторов битов
    html += R"rawliteral(
    <style>
        .flash-update { background-color: #8ce757 !important; transition: background-color 2s; }
        .bit-row { 
            display: grid; 
            grid-template-columns: repeat(8, 1fr); 
            gap: 4px; 
            margin-top: 8px; 
            padding: 5px;
            background: #f9f9f9;
            border-radius: 4px;
        }
        .bit-dot { 
            width: 10px; height: 10px; border-radius: 50%; 
            background: #ddd; border: 1px solid #ccc;
            margin: 0 auto; position: relative;
        }
        .bit-on { background: #28a745; box-shadow: 0 0 5px #28a745; border-color: #1e7e34; }
        .bit-label { font-size: 8px; color: #999; display: block; text-align: center; }

        .switch { position: relative; display: inline-block; width: 40px; height: 20px; vertical-align: middle; margin-left: 10px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 20px; }
        .slider:before { position: absolute; content: ""; height: 14px; width: 14px; left: 3px; bottom: 3px; background-color: white; transition: .4s; border-radius: 50%; }
        input:checked + .slider { background-color: #28a745; }
        input:checked + .slider:before { transform: translateX(20px); }
        .reg-label { font-weight: bold; font-size: 14px; }
        @keyframes flash-green {
          0% { background-color: rgba(40, 167, 69, 0.7); }
          100% { background-color: transparent; }
        }
        .flash-active {
          animation: flash-green 1s ease-out;
        }
    </style>
    )rawliteral";

    html += "<table border='1' cellpadding='8' id='regTable'>";
    html += "<thead><tr><th>Регистр</th><th>Значение и флаги</th></tr></thead>";
    html += "<tbody id='tableBody'>";
    
    // Первоначальный вывод строк из памяти memo
    for (const auto& item : memo) {
        html += "<tr id='reg-" + String(item.first) + "'>";
        html += "<td><b>" + String(item.first) + "</b></td>";
        html += "<td class='val-cell'>" + String(item.second) + "</td></tr>";
    }
    html += "</tbody></table>";

    html += R"rawliteral(
    <script>

    const regTitles = {
      "0": "Основные статусы",
      "1": "Режимы вентиляции и дверей",
      "2": "Управление лазом и календарем",
      "3": "Ошибки и кормление",
      "4": "Статус подключенных модулей (K1-K4)",
      "5": "Измеренная температура курятника",
      "6": "Влажность курятника",
      "7": "Температура на улице",
      "8": "Влажность на улице",
      "9": "Время закрытия лаза (расчетное)",
      "10": "Время открытия лаза (расчетное)",
      "11": "Резерв",
      "12": "Номер кормушки следующего кормления",
      "13": "Длительность следующего кормления",
      "14": "Количество прошедших кормлений за сутки",
      "15": "Запрограммированое количество кормлений",
      "16": "Время следующего кормления",
      "17": "Секунда",
      "18": "Минута",
      "19": "Час",
      "20": "Число",
      "21": "Месяц",
      "22": "Год",
      "23": "Резерв",
      "24": "Заданная температура",
      "25": "Отклонение температуры",
      "26": "Заданная влажность",
      "27": "Отклонение влажности",
      "28": "Количество датчиков температуры",
      "29": "Заданная температура днем",
      "30": "Температура охлаждения",
      "31": "\"Газ\" количесво сработок в час",
      "32": "\"Газ\" общее число сработок",
      "33": "Температура открытия лаза",
      "34": "Количество включений вентиляции в час",
      "35": "Расчитанное время смены объема воздуха",
      "36": "Время восхода солнца",
      "37": "Время заката солнца",
      "38": "Записаное время подъема",
      "39": "Записаное время отбоя",
      "40": "Записаное время открытия лаза",
      "41": "Записаное время закрытия лаза",
      "42": "Время отложенного закрытия лаза",
      "43": "Время движения привода лаза",
      "44": "Количество включений вентиляторов в час при загазованности",
      "45": "Время дежурного освещения",
      "46": "Время сумерек",
      "47": "Среднее время нагревателя за час в сутки",
      "48": "Количество ошибок по кормушке К1",
      "49": "Количество ошибок по кормушке К2",
      "50": "Количество ошибок по кормушке К3",
      "51": "Количество ошибок по кормушке К4",
      "52": "Ошибки от модуля Wi-Fi",
      "53": "Количество используемых вентиляторов",
      "54": "Производительность вентиляторов",
      "55": "Объем помещения",
      "56": "Масса одной птицы",
      "57": "Количество птиц",
      "58": "Допустимое количество проветриваний в час",
      "59": "Используемый язык в приборе",
      "60": "Лето - объем свежего воздуха ГОСТ",
      "61": "Зима - объем свежего воздуха ГОСТ",
      "62": "Осень - объем свежего воздуха ГОСТ",
      "63": "Лето - объем свежего воздуха",
      "64": "Зима - объем свежего воздуха",
      "65": "Осень - объем свежего воздуха",
      "66": "Резерв",
      "67": "Резерв",
      "68": "Резерв",
      "69": "Резерв",
      "70": "Резерв",
      "71": "Резерв",
      "72": "Январь восход",
      "73": "Февраль восход",
      "74": "Март восход",
      "75": "Апрель восход",
      "76": "Май восход",
      "77": "Июнь восход",
      "78": "Июль восход",
      "79": "Август восход",
      "80": "Сентябрь восход",
      "81": "Октябрь восход",
      "82": "Ноябрь восход",
      "83": "Декабрь восход",
      "84": "Январь закат",
      "85": "Февраль закат",
      "86": "Март закат",
      "87": "Апрель закат",
      "88": "Май закат",
      "89": "Июнь закат",
      "90": "Июль закат",
      "91": "Август закат",
      "92": "Сентябрь закат",
      "93": "Октябрь закат",
      "94": "Ноябрь закат",
      "95": "Декабрь закат",
      "96": "Резерв",
      "97": "Резерв",
      "98": "Резерв",
      "99": "Резерв",
      "100": "Панель команд",
      "101": "Панель команд 2",
      "102": "Резерв",
      "103": "Резерв",
      "104": "Резерв",
      "105": "Резерв",
      "106": "Резерв",
      "107": "Резерв",
      "108": "Резерв",
      "109": "Резерв",
      "110": "Время 1 кормления",
      "111": "Время 2 кормления",
      "112": "Время 3 кормления",
      "113": "Время 4 кормления",
      "114": "Время 5 кормления",
      "115": "Время 6 кормления",
      "116": "Время 7 кормления",
      "117": "Время 8 кормления",
      "118": "Время 9 кормления",
      "119": "Время 10 кормления",
      "120": "Время 11 кормления",
      "121": "Время 12 кормления",
      "122": "Время 13 кормления",
      "123": "Время 14 кормления",
      "124": "Время 15 кормления",
      "125": "Длительность 1 кормления",
      "126": "Длительность 2 кормления",
      "127": "Длительность 3 кормления",
      "128": "Длительность 4 кормления",
      "129": "Длительность 5 кормления",
      "130": "Длительность 6 кормления",
      "131": "Длительность 7 кормления",
      "132": "Длительность 8 кормления",
      "133": "Длительность 9 кормления",
      "134": "Длительность 10 кормления",
      "135": "Длительность 11 кормления",
      "136": "Длительность 12 кормления",
      "137": "Длительность 13 кормления",
      "138": "Длительность 14 кормления",
      "139": "Длительность 15 кормления",
      "140": "Резерв",
      "141": "Текущая минута последнего обновления",
      "142": "Текущий час последнего обновления",
      "143": "Число последнего обновления",
      "144": "Месяц последнего обновления",
      "145": "Год последнего обновления"
    };

    // Названия для битов регистров
    const bitNames = {
      "0": ["Охлаждение", "Задержка управления", "Загазованно (датчик)", "Темп. ниже нуля (улица)", "Темп. ниже нуля (курятник)", "Прием команд", "Передача включена", "Связь с удаленным модулем",
            "Вкл. движение вниз", "Вкл. движение вверх", "Основное освещение", "Дежурное освещение", "Приточный вентилятор", "Вытяжной вентилятор", "Нагрев", "-"],
      "1": ["Сон по расписанию", "На улице светло", "Дверь открыта", "Вкл. авто. проветривание", "Вкл. ручн. проветривание", "-", "Коррекция вентиляторов", "Управление осушителем", 
            "Датч. загаз. превышен (в час)", "Авария батарейки", "Авария датчика температуры", "Опрос датчика газа", "Опрос датчика двери", "Упр. вентиляторами днем", "Упр. вентиляторами во время сна", "Ручн. упр. вентиляцией"],
      "2": ["Упр. лазом по календарю", "Упр. закрытием лаза по расписанию", "Включен экран отображения вентиляции", "Включен экран отображения температур", "Упр. вентиляцией по ГОСТ", "-", "Запрет открытия лаза по температуре", "Запрет открытия лаза по температуре на улице", 
            "Лаз открыт", "Лаз закрыт", "Ручн. режим упр. лазом", "Есть движение лаза", "Лаз зафиксирован", "Занято направлением движения лаза", "Занято направлением движения", "Упр. лазом по температуре"],
      "3": ["Упр. светом использует календарь", "При окткл. календаре свет горит постоянно", "Настройка по Wi-Fi", "Находимся в меню НАСТРОЙКА", "Находимся в одном из ручных режимов управления", "Ошибка при расчете вентиляции", "Нажата кнопка на приборе", "Управление охлаждением", 
            "Пропуски кормления", "Ошибки при кормлении", "-", "Кормление за сутки закончилось", "Количество кормлений = 0", "-", "Включен режим ручного кормления", "Отработка пропусков кормления"],
      "4": ["Подключена К1", "Подключена К2", "Подключена К3", "Подключена К4", "Используется осушитель", "-", "-", "-", 
            "Вкл. в сеть успройство К1", "Вкл. в сеть успройство К2", "Вкл. в сеть успройство К3", "Вкл. в сеть успройство К4", "Вкл. осушитель в сеть", "Вкл. в сеть новый модуль", "-", "-"],
      "100": ["Бит 0", "Бит 1", "Бит 2", "Бит 3", "Бит 4", "Бит 5", "Бит 6", "Бит 7", "Бит 8", "Бит 9", "Бит 10", "Бит 11", "Бит 12", "Бит 13", "Бит 14", "Бит 15"]
    };

    function sendBit(addr, bit, state) {
      // Отправляем команду и СРАЗУ вызываем обновление данных после ответа сервера
      fetch(`/api/write_bit?addr=${addr}&bit=${bit}&state=${state ? 1 : 0}`)
      .then(r => { if(r.ok) setTimeout(updateData, 100); }); 
    }

    function sendValue(addr, val) {
      // Проверка диапазона перед отправкой
      if (val < 0 || val > 255) {
          alert("Значение должно быть от 0 до 255");
          return;
      }
      fetch(`/api/write_reg?addr=${addr}&value=${val}`)
          .then(r => { if(r.ok) setTimeout(updateData, 100); });
    }

    function updateData() {
      fetch('/api/data').then(r => r.json()).then(data => {
        for (const [key, value] of Object.entries(data)) {
          let row = document.getElementById('reg-' + key);
          if (!row) continue;
          const valCell = row.querySelector('.val-cell');

          // 1. Логика анимации вспышки для ВСЕХ регистров при изменении
          const oldValue = row.getAttribute('data-last-val');
          if (oldValue !== null && oldValue !== String(value)) {
            row.classList.remove('flash-active');
            void row.offsetWidth; // Сброс для перезапуска анимации
            row.classList.add('flash-active');
          }
          row.setAttribute('data-last-val', value);

          // 2. Формируем заголовок
          const titleText = regTitles[key] ? regTitles[key] : `Регистр №${key}`;
          const headerHtml = `<div style="background:#4e73df; color:white; padding:5px; margin-bottom:10px; border-radius:4px; font-size:11px; text-align:center; font-weight:bold; text-transform:uppercase;">${titleText}</div>`;

          // --- КЕЙС 100 (ПАНЕЛЬ УПРАВЛЕНИЯ - ПЕРЕКЛЮЧАТЕЛИ) ---
          if (key == "100") {
            let html = headerHtml + `<div style="display:grid; grid-template-columns: 1fr 1fr; gap:8px; padding:4px;">`;
            for (let i = 0; i < 16; i++) {
              const isSet = (value >> i) & 1;
              const name = bitNames[key] ? bitNames[key][i] : `Бит ${i}`;
              html += `
                <div style="display:flex; align-items:center; justify-content:space-between; background:#f8f9fc; padding:5px 8px; border-radius:6px; border:1px solid #eaecf4;">
                  <span style="font-size:11px;">${name}</span>
                  <label class="switch"><input type="checkbox" ${isSet ? 'checked' : ''} onchange="sendBit(${key}, ${i}, this.checked)"><span class="slider"></span></label>
                </div>`;
            }
            valCell.innerHTML = html + `</div>`;

          // --- КЕЙС 101 (НАСТРОЙКА ПАРАМЕТРА - ВВОД ЧИСЛА) ---
          //} else if (key == "101") {
          //  valCell.innerHTML = headerHtml + `
          //    <div style="display:flex; flex-direction:column; align-items:center; padding:10px;">
          //      <input type="number" min="0" max="255" value="${value}" style="width:100px; text-align:center; font-weight:bold; padding:8px; border:2px solid #4e73df; border-radius:8px;" 
          //            onchange="sendValue(${key}, this.value)">
          //      <span style="font-size:11px; color:#999; margin-top:4px;">(0-255)</span>
          //    </div>`;

          // --- КЕЙС 0-4 (ТОЛЬКО ОТОБРАЖЕНИЕ БИТОВ - ЦЕНТРИРОВАНИЕ + ФОН) ---
          } else if (['0', '1', '2', '3', '4'].includes(key)) {
            let html = headerHtml + `<div style="display:grid; grid-template-columns: 1fr 1fr; gap:6px; padding:4px;">`;
            for (let i = 0; i < 16; i++) {
              const isActive = (value >> i) & 1;
              const name = (bitNames[key] && bitNames[key][i]) ? bitNames[key][i] : `Бит ${i}`;
              const bgColor = isActive ? '#28a745' : '#f0f2f5';
              const textColor = isActive ? '#fff' : '#555';
              html += `
                <div style="display:flex; align-items:center; justify-content:center; background:${bgColor}; color:${textColor}; 
                            padding:10px 4px; border-radius:8px; border:1px solid #e0e4e8; text-align:center; min-height:38px; transition: 0.3s;">
                  <span style="font-size:12px; font-weight:600; line-height:1.1;">${name}</span>
                </div>`;
            }
            valCell.innerHTML = html + '</div>';

          // --- ВСЕ ОСТАЛЬНЫЕ РЕГИСТРЫ (ТОЛЬКО ЧТЕНИЕ) ---
          } else {
            valCell.innerHTML = headerHtml + `
              <div style="text-align:center; padding:8px; font-size:18px; font-weight:bold; color:#333; background:#fff; border-radius:8px;">
                ${value}
              </div>`;
          }
        }
      });
    }


    setInterval(updateData, 2000);
    updateData();
    </script>
    )rawliteral";

    html += "</body></html>";
    webServer.send(200, "text/html", html);
}

// Страница автоматизации курятника
void handleAutoChickenHous() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Управление курятником</title>";
  html += "<style>";
  // Базовые стили для адаптивности и красоты
  html += "body { font-family: sans-serif; background-color: #f4f6f9; color: #333; margin: 0; padding: 20px; text-align: center; }";
  html += "h1 { color: #2c3e50; margin-bottom: 30px; font-size: 24px; }";
  // Сетка для кнопок-карточек
  html += ".menu-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; max-width: 900px; margin: 0 auto; }";
  // Стили самих кнопок
  html += ".menu-btn { display: flex; flex-direction: column; align-items: center; justify-content: center; background: white; ";
  html += "border-radius: 12px; padding: 25px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); text-decoration: none; color: #2c3e50; ";
  html += "font-size: 18px; font-weight: bold; transition: transform 0.2s, box-shadow 0.2s; border: 1px solid #e2e8f0; }";
  html += ".menu-btn:hover { transform: translateY(-3px); box-shadow: 0 10px 15px rgba(0,0,0,0.1); border-color: #cbd5e1; }";
  html += ".menu-btn:active { transform: translateY(0); }";
  // Иконки (простые emoji в качестве визуальных якорей)
  html += ".icon { font-size: 32px; margin-bottom: 12px; }";
  // Кастомный цвет для критической кнопки аварий
  html += ".btn-alert { border-left: 5px solid #ef4444; }";
  html += "</style></head><body>";

  html += "<h1>🐔 Автоматизация курятника</h1>";
  
  // Контейнер с кнопками меню
  html += "<div class='menu-grid'>";
  
  html += "  <a href='/climate' class='menu-btn'><span class='icon'>🌡️</span>1. Климат-контроль</a>";
  html += "  <a href='/light' class='menu-btn'><span class='icon'>💡</span>2. Освещение и распорядок</a>";
  html += "  <a href='/feeding' class='menu-btn'><span class='icon'>🌾</span>3. Кормление и поголовье</a>";
  html += "  <a href='/door' class='menu-btn'><span class='icon'>🐓</span>4. Лаз (дверь)</a>";
  html += "  <a href='/status' class='menu-btn'><span class='icon'>📊</span>5. Статусы</a>";
  html += "  <a href='/alerts' class='menu-btn menu-btn-alert'><span class='icon'>⚠️</span>6. Аварии и ошибки</a>";
  html += "  <a href='/' class='menu-btn menu-btn-alert'><span class='icon'>⬅️</span>Назад</a>";
  
  html += "</div>";

  html += "</body></html>";
  
  webServer.send(200, "text/html", html);
}

// Освещение и распорядок
void handleLight() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Освещение и распорядок</title>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f4f7f6; color: #333; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }";
  html += ".container { width: 100%; max-width: 600px; }";
  html += "h2 { color: #2c3e50; text-align: center; margin-bottom: 20px; }";
  html += ".card { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 4px 15px rgba(0,0,0,0.05); margin-bottom: 20px; border: 1px solid #eee; }";
  html += "h3 { margin-top: 0; color: #4e73df; border-bottom: 2px solid #eaecf4; padding-bottom: 8px; font-size: 16px; text-transform: uppercase; }";
  html += ".status-row { display: flex; justify-content: space-between; align-items: center; padding: 10px 0; border-bottom: 1px solid #f8f9fa; }";
  html += ".status-row:last-child { border-bottom: none; }";
  html += ".badge { padding: 5px 12px; border-radius: 20px; font-size: 13px; color: white; font-weight: bold; min-width: 60px; text-align: center; }";
  html += ".bg-success { background: #28a745; }";
  html += ".bg-danger { background: #dc3545; }";
  html += ".val-box { font-weight: bold; color: #333; background: #f8f9fc; padding: 4px 10px; border-radius: 6px; border: 1px solid #eaecf4; font-family: monospace; font-size: 15px; }";
  html += ".btn { display: block; text-align: center; padding: 12px; background: #6c757d; color: white; text-decoration: none; border-radius: 8px; font-weight: 600; margin-top: 10px; transition: 0.3s; border: none; }";
  html += ".btn:hover { opacity: 0.9; }";
  html += "table { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 14px; }";
  html += "th, td { padding: 8px; border: 1px solid #eaecf4; text-align: center; }";
  html += "th { background: #f8f9fc; color: #4e73df; font-weight: bold; }";
  html += ".month-name { text-align: left; font-weight: bold; }";
  html += "</style></head><body>";

  html += "<div class='container'>";
  html += "<h2>💡 Освещение и распорядок</h2>";

  // Блок 1: Текущие статусы (Биты)
  html += "<div class='card'>";
  html += "<h3>Текущее состояние</h3>";
  html += "<div class='status-row'><span>Основное освещение (R0:B10):</span><span id='b_0_10' class='badge bg-danger'>Выкл</span></div>";
  html += "<div class='status-row'><span>Дежурное освещение (R0:B11):</span><span id='b_0_11' class='badge bg-danger'>Выкл</span></div>";
  html += "<div class='status-row'><span>Сон по расписанию (R1:B0):</span><span id='b_1_0' class='badge bg-danger'>Выкл</span></div>";
  html += "<div class='status-row'><span>На улице светло (R1:B1):</span><span id='b_1_1' class='badge bg-danger'>Нет</span></div>";
  html += "<div class='status-row'><span>Управление по календарю (R3:B0):</span><span id='b_3_0' class='badge bg-danger'>Выкл</span></div>";
  html += "<div class='status-row'><span>Свет горит постоянно при откл. календаре (R3:B1):</span><span id='b_3_1' class='badge bg-danger'>Нет</span></div>";
  html += "</div>";

  // Блок 2: Временные уставки (Регистры)
  html += "<div class='card'>";
  html += "<h3>Оперативные уставки времени</h3>";
  html += "<div class='status-row'><span>Время восхода солнца (R36):</span><span id='r_36' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Время заката солнца (R37):</span><span id='r_37' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Записанное время подъема (R38):</span><span id='r_38' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Записанное время отбоя (R39):</span><span id='r_39' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Время дежурного освещения (R45):</span><span id='r_45' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Время сумерек (R46):</span><span id='r_46' class='val-box'>--</span></div>";
  html += "</div>";

  // Блок 3: Календарь восходов и закатов (Таблица)
  html += "<div class='card'>";
  html += "<h3>Годовой календарь (Восход / Закат)</h3>";
  html += "<table>";
  html += "<thead><tr><th>Месяц</th><th>Восход (Рег)</th><th>Закат (Рег)</th></tr></thead>";
  html += "<tbody>";
  const char* months[] = {"Январь", "Февраль", "Март", "Апрель", "Май", "Июнь", "Июль", "Август", "Сентябрь", "Октябрь", "Ноябрь", "Декабрь"};
  for (int i = 0; i < 12; i++) {
    int r_voshod = 72 + i;
    int r_zakat = 84 + i;
    html += "<tr>";
    html += "<td class='month-name'>" + String(months[i]) + "</td>";
    html += "<td id='r_" + String(r_voshod) + "'>--</td>";
    html += "<td id='r_" + String(r_zakat) + "'>--</td>";
    html += "</tr>";
  }
  html += "</tbody></table>";
  html += "</div>";

  // Кнопка Назад
  html += "<a href='/autoChickenHous' class='btn'>⬅️ Назад в меню</a>";
  html += "</div>";

  // JavaScript AJAX скрипт для обновления данных в реальном времени
  html += R"rawliteral(
  <script>
  function updateLightData() {
    fetch('/api/data')
      .then(r => r.json())
      .then(data => {
        // Функция форматирования времени (если в регистре минуты или код времени)
        // Оставляем простой вывод значения, при необходимости можно добавить логику ЧЧ:ММ
        function formatTime(val) {
          if (val === undefined) return '--';
          return val; 
        }

        // Обновление битовых флагов (0 регистр)
        let r0 = data["0"] || 0;
        updateBitBadge('b_0_10', (r0 >> 10) & 1, "Вкл", "Выкл");
        updateBitBadge('b_0_11', (r0 >> 11) & 1, "Вкл", "Выкл");

        // Обновление битовых флагов (1 регистр)
        let r1 = data["1"] || 0;
        updateBitBadge('b_1_0', (r1 >> 0) & 1, "Вкл", "Выкл");
        updateBitBadge('b_1_1', (r1 >> 1) & 1, "Да", "Нет");

        // Обновление битовых флагов (3 регистр)
        let r3 = data["3"] || 0;
        updateBitBadge('b_3_0', (r3 >> 0) & 1, "Вкл", "Выкл");
        updateBitBadge('b_3_1', (r3 >> 1) & 1, "Да", "Нет");

        // Обновление оперативных регистров времени
        const regList =;
        regList.forEach(reg => {
          let el = document.getElementById('r_' + reg);
          if (el && data[reg] !== undefined) el.innerText = formatTime(data[reg]);
        });

        // Обновление календаря (регистры 72-83 и 84-95)
        for (let i = 0; i < 12; i++) {
          let voshodReg = 72 + i;
          let zakatReg = 84 + i;
          
          let voshodEl = document.getElementById('r_' + voshodReg);
          if (voshodEl && data[voshodReg] !== undefined) voshodEl.innerText = formatTime(data[voshodReg]);
          
          let zakatEl = document.getElementById('r_' + zakatReg);
          if (zakatEl && data[zakatReg] !== undefined) zakatEl.innerText = formatTime(data[zakatReg]);
        }
      })
      .catch(err => console.error("Ошибка обновления данных:", err));
  }

  function updateBitBadge(id, state, textOn, textOff) {
    let el = document.getElementById(id);
    if (!el) return;
    if (state === 1) {
      el.innerText = textOn;
      el.className = "badge bg-success";
    } else {
      el.innerText = textOff;
      el.className = "badge bg-danger";
    }
  }

  setInterval(updateLightData, 2000);
  updateLightData();
  </script>
  )rawliteral";

  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

// Аварии и ошибки
void handleAlerts() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Аварии и ошибки</title>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f8fafc; color: #334155; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }";
  html += ".container { width: 100%; max-width: 600px; }";
  html += "h2 { color: #0f172a; text-align: center; margin-bottom: 24px; font-size: 22px; }";
  html += ".card { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.05), 0 2px 4px -2px rgba(0,0,0,0.05); margin-bottom: 20px; border: 1px solid #e2e8f0; }";
  html += "h3 { margin-top: 0; color: #64748b; border-bottom: 2px solid #f1f5f9; padding-bottom: 8px; font-size: 14px; text-transform: uppercase; letter-spacing: 0.5px; }";
  html += ".alert-row { display: flex; justify-content: space-between; align-items: center; padding: 12px; border-radius: 8px; margin-bottom: 8px; border: 1px solid #e2e8f0; background: #f8fafc; transition: all 0.3s ease; }";
  html += ".alert-row.active { background: #fef2f2; border-color: #fca5a5; color: #991b1b; font-weight: 600; }";
  html += ".badge { padding: 4px 10px; border-radius: 6px; font-size: 12px; font-weight: bold; min-width: 70px; text-align: center; text-transform: uppercase; }";
  html += ".bg-normal { background: #e2e8f0; color: #475569; }";
  html += ".bg-alarm { background: #ef4444; color: white; animation: pulse 2s infinite; }";
  html += ".counter-row { display: flex; justify-content: space-between; align-items: center; padding: 10px 0; border-bottom: 1px solid #f1f5f9; }";
  html += ".counter-row:last-child { border-bottom: none; }";
  html += ".val-box { font-weight: bold; color: #0f172a; background: #f1f5f9; padding: 4px 12px; border-radius: 6px; border: 1px solid #cbd5e1; font-family: monospace; font-size: 15px; min-width: 30px; text-align: center; }";
  html += ".val-box.has-errors { background: #fee2e2; color: #b91c1c; border-color: #fca5a5; }";
  html += ".btn { display: block; text-align: center; padding: 12px; background: #64748b; color: white; text-decoration: none; border-radius: 8px; font-weight: 600; margin-top: 15px; transition: 0.2s; border: none; }";
  html += ".btn:hover { background: #475569; }";
  html += "@keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.6; } 100% { opacity: 1; } }";
  html += "</style></head><body>";

  html += "<div class='container'>";
  html += "<h2>⚠️ Аварии и ошибки шлюза</h2>";

  // Блок 1: Активные аварии (Биты)
  html += "<div class='card'>";
  html += "<h3>Критические аварии (Биты)</h3>";
  
  html += "<div id='row_b_1_9' class='alert-row'><span>Авария батарейки (R1:B9)</span><span id='b_1_9' class='badge bg-normal'>Норма</span></div>";
  html += "<div id='row_b_1_10' class='alert-row'><span>Авария датчика температуры (R1:B10)</span><span id='b_1_10' class='badge bg-normal'>Норма</span></div>";
  html += "<div id='row_b_3_5' class='alert-row'><span>Ошибка расчета вентиляции (R3:B5)</span><span id='b_3_5' class='badge bg-normal'>Норма</span></div>";
  html += "<div id='row_b_3_9' class='alert-row'><span>Ошибка при кормлении (R3:B9)</span><span id='b_3_9' class='badge bg-normal'>Норма</span></div>";
  
  html += "</div>";

  // Блок 2: Счетчики ошибок (Регистры)
  html += "<div class='card'>";
  html += "<h3>Статистика неисправностей (Регистры)</h3>";
  
  html += "<div class='counter-row'><span>Ошибок по кормушке К1 (R48):</span><span id='r_48' class='val-box'>0</span></div>";
  html += "<div class='counter-row'><span>Ошибок по кормушке К2 (R49):</span><span id='r_49' class='val-box'>0</span></div>";
  html += "<div class='counter-row'><span>Ошибок по кормушке К3 (R50):</span><span id='r_50' class='val-box'>0</span></div>";
  html += "<div class='counter-row'><span>Ошибок по кормушке К4 (R51):</span><span id='r_51' class='val-box'>0</span></div>";
  html += "<div class='counter-row'><span>Ошибки от модуля Wi-Fi (R52):</span><span id='r_52' class='val-box'>0</span></div>";
  
  html += "</div>";

  // Кнопка Назад
  html += "<a href='/autoChickenHous' class='btn'>⬅️ Назад в меню</a>";
  html += "</div>";

  // JavaScript скрипт динамического обновления
  html += R"rawliteral(
  <script>
  function updateAlertsData() {
    fetch('/api/data')
      .then(r => r.json())
      .then(data => {
        // Чтение регистров для битовых масок
        let r1 = data["1"] || 0;
        let r3 = data["3"] || 0;

        // Проверка битов 1 регистра
        updateAlarmState('b_1_9', 'row_b_1_9', (r1 >> 9) & 1, "АВАРИЯ");
        updateAlarmState('b_1_10', 'row_b_1_10', (r1 >> 10) & 1, "АВАРИЯ");

        // Проверка битов 3 регистра
        updateAlarmState('b_3_5', 'row_b_3_5', (r3 >> 5) & 1, "ОШИБКА");
        updateAlarmState('b_3_9', 'row_b_3_9', (r3 >> 9) & 1, "ОШИБКА");

        // Обновление числовых регистров (48-52)
        const counterRegs =;
        counterRegs.forEach(reg => {
          let el = document.getElementById('r_' + reg);
          if (el && data[reg] !== undefined) {
            let val = data[reg];
            el.innerText = val;
            
            // Если количество ошибок больше 0, подсвечиваем поле
            if (val > 0) {
              el.className = "val-box has-errors";
            } else {
              el.className = "val-box";
            }
          }
        });
      })
      .catch(err => console.error("Ошибка при получении данных аварий:", err));
  }

  function updateAlarmState(badgeId, rowId, isActive, alarmText) {
    let badge = document.getElementById(badgeId);
    let row = document.getElementById(rowId);
    if (!badge || !row) return;

    if (isActive === 1) {
      badge.innerText = alarmText;
      badge.className = "badge bg-alarm";
      row.className = "alert-row active";
    } else {
      badge.innerText = "Норма";
      badge.className = "badge bg-normal";
      row.className = "alert-row";
    }
  }

  setInterval(updateAlertsData, 2000);
  updateAlertsData();
  </script>
  )rawliteral";

  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

// Статусы
void handleStatus() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Системные статусы</title>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f0f2f5; color: #1c1e21; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }";
  html += ".container { width: 100%; max-width: 600px; }";
  html += "h2 { color: #1a73e8; text-align: center; margin-bottom: 20px; font-size: 24px; }";
  html += ".card { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 2px 10px rgba(0,0,0,0.05); margin-bottom: 20px; border: 1px solid #e4e6eb; }";
  html += "h3 { margin-top: 0; color: #65676b; border-bottom: 2px solid #f0f2f5; padding-bottom: 8px; font-size: 14px; text-transform: uppercase; letter-spacing: 0.5px; }";
  html += ".status-row { display: flex; justify-content: space-between; align-items: center; padding: 12px 8px; border-bottom: 1px solid #f0f2f5; }";
  html += ".status-row:last-child { border-bottom: none; }";
  html += ".status-name { font-size: 15px; color: #050505; }";
  html += ".badge { padding: 6px 14px; border-radius: 20px; font-size: 12px; font-weight: 600; min-width: 80px; text-align: center; text-transform: uppercase; transition: all 0.2s ease; }";
  html += ".bg-off { background: #bcc0c4; color: #4b4f56; }";
  html += ".bg-on { background: #34a853; color: white; }";
  html += ".btn { display: block; text-align: center; padding: 12px; background: #1a73e8; color: white; text-decoration: none; border-radius: 8px; font-weight: 600; margin-top: 15px; transition: background 0.2s; border: none; }";
  html += ".btn:hover { background: #1557b0; }";
  html += "</style></head><body>";

  html += "<div class='container'>";
  html += "<h2>📊 Системные статусы и режимы</h2>";

  // Группа 1: Связь и обмен данными (Регистр 0)
  html += "<div class='card'>";
  html += "<h3>Связь и обмен (Регистр 0)</h3>";
  html += "<div class='status-row'><span class='status-name'>Задержка управления (R0:B1)</span><span id='b_0_1' class='badge bg-off'>Пауза</span></div>";
  html += "<div class='status-row'><span class='status-name'>Прием команд (R0:B5)</span><span id='b_0_5' class='badge bg-off'>Нет</span></div>";
  html += "<div class='status-row'><span class='status-name'>Передача включена (R0:B6)</span><span id='b_0_6' class='badge bg-off'>Выкл</span></div>";
  html += "<div class='status-row'><span class='status-name'>Связь с удаленным модулем (R0:B7)</span><span id='b_0_7' class='badge bg-off'>Отказ</span></div>";
  html += "</div>";

  // Группа 2: Отображение экранов локального дисплея (Регистр 2)
  html += "<div class='card'>";
  html += "<h3>Интерфейс прибора (Регистр 2)</h3>";
  html += "<div class='status-row'><span class='status-name'>Экран отображения вентиляции (R2:B2)</span><span id='b_2_2' class='badge bg-off'>Скрыт</span></div>";
  html += "<div class='status-row'><span class='status-name'>Экран отображения температур (R2:B3)</span><span id='b_2_3' class='badge bg-off'>Скрыт</span></div>";
  html += "</div>";

  // Группа 3: Настройки и управление (Регистр 3)
  html += "<div class='card'>";
  html += "<h3>Режимы и кнопки (Регистр 3)</h3>";
  html += "<div class='status-row'><span class='status-name'>Настройка по Wi-Fi (R3:B2)</span><span id='b_3_2' class='badge bg-off'>Выкл</span></div>";
  html += "<div class='status-row'><span class='status-name'>В меню \"Настройка\" (R3:B3)</span><span id='b_3_3' class='badge bg-off'>Нет</span></div>";
  html += "<div class='status-row'><span class='status-name'>Ручной режим управления (R3:B4)</span><span id='b_3_4' class='badge bg-off'>Авто</span></div>";
  html += "<div class='status-row'><span class='status-name'>Нажата кнопка на приборе (R3:B6)</span><span id='b_3_6' class='badge bg-off'>Отпущена</span></div>";
  html += "</div>";

  // Кнопка Назад
  html += "<a href='/autoChickenHous' class='btn'>⬅️ Назад в меню</a>";
  html += "</div>";

  // JavaScript AJAX скрипт для обновления
  html += R"rawliteral(
  <script>
  function updateStatusData() {
    fetch('/api/data')
      .then(r => r.json())
      .then(data => {
        // Чтение регистров
        let r0 = data["0"] || 0;
        let r2 = data["2"] || 0;
        let r3 = data["3"] || 0;

        // Разбор регистра 0
        updateBadge('b_0_1', (r0 >> 1) & 1, "Активна", "Пауза");
        updateBadge('b_0_5', (r0 >> 5) & 1, "Прием", "Нет");
        updateBadge('b_0_6', (r0 >> 6) & 1, "Передача", "Выкл");
        updateBadge('b_0_7', (r0 >> 7) & 1, "ОК", "Отказ");

        // Разбор регистра 2
        updateBadge('b_2_2', (r2 >> 2) & 1, "АКТИВЕН", "Скрыт");
        updateBadge('b_2_3', (r2 >> 3) & 1, "АКТИВЕН", "Скрыт");

        // Разбор регистра 3
        updateBadge('b_3_2', (r3 >> 2) & 1, "АКТИВНА", "Выкл");
        updateBadge('b_3_3', (r3 >> 3) & 1, "ДА", "Нет");
        updateBadge('b_3_4', (r3 >> 4) & 1, "РУЧНОЙ", "Авто");
        updateBadge('b_3_6', (r3 >> 6) & 1, "НАЖАТА", "Отпущена");
      })
      .catch(err => console.error("Ошибка обновления статусов:", err));
  }

  function updateBadge(id, state, textOn, textOff) {
    let el = document.getElementById(id);
    if (!el) return;
    if (state === 1) {
      el.innerText = textOn;
      el.className = "badge bg-on";
    } else {
      el.innerText = textOff;
      el.className = "badge bg-off";
    }
  }

  setInterval(updateStatusData, 2000);
  updateStatusData();
  </script>
  )rawliteral";

  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

// Кормление и поголовье
void handleFeeding() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Кормление и поголовье</title>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f4f7f6; color: #333; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }";
  html += ".container { width: 100%; max-width: 700px; }";
  html += "h2 { color: #2c3e50; text-align: center; margin-bottom: 20px; font-size: 24px; }";
  html += ".card { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 4px 15px rgba(0,0,0,0.05); margin-bottom: 20px; border: 1px solid #eee; }";
  html += "h3 { margin-top: 0; color: #27ae60; border-bottom: 2px solid #e8f5e9; padding-bottom: 8px; font-size: 15px; text-transform: uppercase; letter-spacing: 0.5px; }";
  html += ".status-row { display: flex; justify-content: space-between; align-items: center; padding: 10px 5px; border-bottom: 1px solid #f8f9fa; }";
  html += ".status-row:last-child { border-bottom: none; }";
  html += ".badge { padding: 5px 12px; border-radius: 20px; font-size: 13px; color: white; font-weight: bold; min-width: 65px; text-align: center; }";
  html += ".bg-success { background: #28a745; }";
  html += ".bg-danger { background: #dc3545; }";
  html += ".bg-neutral { background: #6c757d; }";
  html += ".val-box { font-weight: bold; color: #333; background: #f8f9fc; padding: 4px 12px; border-radius: 6px; border: 1px solid #eaecf4; font-family: monospace; font-size: 15px; }";
  html += ".grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }";
  html += "table { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 14px; }";
  html += "th, td { padding: 8px; border: 1px solid #eaecf4; text-align: center; }";
  html += "th { background: #f8f9fc; color: #27ae60; font-weight: bold; }";
  html += ".btn { display: block; text-align: center; padding: 12px; background: #27ae60; color: white; text-decoration: none; border-radius: 8px; font-weight: 600; margin-top: 15px; transition: 0.2s; border: none; }";
  html += ".btn:hover { background: #219653; }";
  html += "@media(max-width: 600px) { .grid-2 { grid-template-columns: 1fr; } }";
  html += "</style></head><body>";

  html += "<div class='container'>";
  html += "<h2>🌾 Кормление и поголовье птицы</h2>";

  // Блок 1: Поголовье и общая статистика
  html += "<div class='card'>";
  html += "<h3>Статистика и поголовье</h3>";
  html += "<div class='grid-2'>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Количество птиц (R57):</span><span id='r_57' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Масса одной птицы (R56):</span><span id='r_56' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Пройдено кормлений (R14):</span><span id='r_14' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>План кормлений (R15):</span><span id='r_15' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "</div>";
  html += "</div>";

  // Блок 2: Состояние процесса кормления (Биты регистра 3)
  html += "<div class='card'>";
  html += "<h3>Статус процесса кормления</h3>";
  html += "<div class='status-row'><span>Пропуски кормления (R3:B8):</span><span id='b_3_8' class='badge bg-danger'>Есть</span></div>";
  html += "<div class='status-row'><span>Кормление за сутки закончилось (R3:B11):</span><span id='b_3_11' class='badge bg-neutral'>Нет</span></div>";
  html += "<div class='status-row'><span>Количество кормлений равно нулю (R3:B12):</span><span id='b_3_12' class='badge bg-danger'>Да</span></div>";
  html += "<div class='status-row'><span>Включен режим ручного кормления (R3:B14):</span><span id='b_3_14' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Отработка пропусков кормления (R3:B15):</span><span id='b_3_15' class='badge bg-neutral'>Выкл</span></div>";
  html += "</div>";

  // Блок 3: Следующее кормление
  html += "<div class='card'>";
  html += "<h3>Следующее кормление</h3>";
  html += "<div class='status-row'><span>Номер кормушки (R12):</span><span id='r_12' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Время начала (R16):</span><span id='r_16' class='val-box'>--</span></div>";
  html += "<div class='status-row'><span>Длительность, сек (R13):</span><span id='r_13' class='val-box'>--</span></div>";
  html += "</div>";

  // Блок 4: Статус оборудования (Кормушки и модули)
  html += "<div class='card'>";
  html += "<h3>Статус оборудования (Кормушки 1-4)</h3>";
  html += "<table>";
  html += "<thead><tr><th>Устройство</th><th>Подключение к системе</th><th>Питание / Сеть</th></tr></thead>";
  html += "<tbody>";
  for (int i = 1; i <= 4; i++) {
    html += "<tr>";
    html += "<td>Кормушка " + String(i) + "</td>";
    html += "<td><span id='b_4_" + String(i-1) + "' class='badge bg-danger'>Откл</span></td>";
    html += "<td><span id='b_4_" + String(8 + (i-1)) + "' class='badge bg-danger'>Выкл</span></td>";
    html += "</tr>";
  }
  html += "<tr>";
  html += "<td>Новый модуль</td>";
  html += "<td colspan='2'><span id='b_4_13' class='badge bg-danger'>Выкл</span></td>";
  html += "</tr>";
  html += "</tbody></table>";
  html += "</div>";

  // Блок 5: Расписание кормлений (Таблица 1-15)
  html += "<div class='card'>";
  html += "<h3>График и длительность кормлений (1-15)</h3>";
  html += "<table>";
  html += "<thead><tr><th>№</th><th>Время кормления</th><th>Длительность</th></tr></thead>";
  html += "<tbody>";
  for (int i = 1; i <= 15; i++) {
    int r_time = 110 + (i - 1);
    int r_dur = 125 + (i - 1);
    html += "<tr>";
    html += "<td>" + String(i) + "</td>";
    html += "<td id='r_" + String(r_time) + "'>--</td>";
    html += "<td id='r_" + String(r_dur) + "'>--</td>";
    html += "</tr>";
  }
  html += "</tbody></table>";
  html += "</div>";

  // Кнопка Назад
  html += "<a href='/autoChickenHous' class='btn'>⬅️ Назад в меню</a>";
  html += "</div>";

  // JavaScript AJAX скрипт динамического обновления
  html += R"rawliteral(
  <script>
  function updateFeedingData() {
    fetch('/api/data')
      .then(r => r.json())
      .then(data => {
        // Загрузка сырых регистров
        let r3 = data["3"] || 0;
        let r4 = data["4"] || 0;

        // Обновление битовых статусов регистра 3
        updateBadge('b_3_8', (r3 >> 8) & 1, "ПРОПУСК", "Норма", true);
        updateBadge('b_3_11', (r3 >> 11) & 1, "Да", "Нет", false);
        updateBadge('b_3_12', (r3 >> 12) & 1, "ДА (0)", "В норме", true);
        updateBadge('b_3_14', (r3 >> 14) & 1, "РУЧНОЙ", "Авто", false);
        updateBadge('b_3_15', (r3 >> 15) & 1, "ОТРАБОТКА", "Нет", false);

        // Обновление битовых статусов регистра 4 (Кормушки 1-4)
        for (let i = 0; i < 4; i++) {
          updateBadge('b_4_' + i, (r4 >> i) & 1, "Подкл", "Откл", false);
          updateBadge('b_4_' + (8 + i), (r4 >> (8 + i)) & 1, "В СЕТИ", "ВЫКЛ", false);
        }
        updateBadge('b_4_13', (r4 >> 13) & 1, "В СЕТИ", "ВЫКЛ", false);

        // Оперативные регистры общего состояния
        const singleRegs =;
        singleRegs.forEach(reg => {
          let el = document.getElementById('r_' + reg);
          if (el && data[reg] !== undefined) el.innerText = data[reg];
        });

        // Заполнение таблицы расписания (110-124 и 125-139)
        for (int i = 0; i < 15; i++) {
          let tReg = 110 + i;
          let dReg = 125 + i;

          let tEl = document.getElementById('r_' + tReg);
          if (tEl && data[tReg] !== undefined) tEl.innerText = data[tReg];

          let dEl = document.getElementById('r_' + dReg);
          if (dEl && data[dReg] !== undefined) dEl.innerText = data[dReg];
        }
      })
      .catch(err => console.error("Ошибка обновления данных кормления:", err));
  }

  function updateBadge(id, state, textOn, textOff, isAlarmType) {
    let el = document.getElementById(id);
    if (!el) return;
    if (state === 1) {
      el.innerText = textOn;
      el.className = isAlarmType ? "badge bg-danger" : "badge bg-success";
    } else {
      el.innerText = textOff;
      el.className = isAlarmType ? "badge bg-success" : "badge bg-danger";
    }
  }

  setInterval(updateFeedingData, 2000);
  updateFeedingData();
  </script>
  )rawliteral";

  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

// Лаз (дверь)
void handleDoor() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Управление лазом (дверью)</title>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f8fafc; color: #1e293b; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }";
  html += ".container { width: 100%; max-width: 650px; }";
  html += "h2 { color: #0f172a; text-align: center; margin-bottom: 24px; font-size: 24px; }";
  html += ".card { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.05); margin-bottom: 20px; border: 1px solid #e2e8f0; }";
  html += "h3 { margin-top: 0; color: #d97706; border-bottom: 2px solid #fef3c7; padding-bottom: 8px; font-size: 15px; text-transform: uppercase; letter-spacing: 0.5px; }";
  html += ".status-row { display: flex; justify-content: space-between; align-items: center; padding: 10px 6px; border-bottom: 1px solid #f1f5f9; }";
  html += ".status-row:last-child { border-bottom: none; }";
  html += ".badge { padding: 5px 12px; border-radius: 20px; font-size: 12px; color: white; font-weight: bold; min-width: 75px; text-align: center; text-transform: uppercase; }";
  html += ".bg-success { background: #10b981; }";
  html += ".bg-danger { background: #ef4444; }";
  html += ".bg-info { background: #3b82f6; }";
  html += ".bg-neutral { background: #64748b; }";
  html += ".val-box { font-weight: bold; color: #0f172a; background: #f8fafc; padding: 4px 12px; border-radius: 6px; border: 1px solid #e2e8f0; font-family: monospace; font-size: 15px; }";
  html += ".grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }";
  html += ".btn { display: block; text-align: center; padding: 12px; background: #d97706; color: white; text-decoration: none; border-radius: 8px; font-weight: 600; margin-top: 15px; transition: 0.2s; border: none; }";
  html += ".btn:hover { background: #b45309; }";
  html += "@media(max-width: 600px) { .grid-2 { grid-template-columns: 1fr; } }";
  html += "</style></head><body>";

  html += "<div class='container'>";
  html += "<h2>🐓 Автоматика лаза (двери)</h2>";

  // Блок 1: Физическое состояние и датчики
  html += "<div class='card'>";
  html += "<h3>Текущее состояние и датчики</h3>";
  html += "<div class='status-row'><span>Положение лаза (R2):</span><div><span id='b_2_8' class='badge bg-neutral'>Открыт</span> <span id='b_2_9' class='badge bg-neutral'>Закрыт</span></div></div>";
  html += "<div class='status-row'><span>Физический датчик открытой двери (R1:B2):</span><span id='b_1_2' class='badge bg-neutral'>Датчик</span></div>";
  html += "<div class='status-row'><span>Опрос концевого датчика двери (R1:B12):</span><span id='b_1_12' class='badge bg-neutral'>Нет</span></div>";
  html += "<div class='status-row'><span>Состояние движения (R2:B11):</span><span id='b_2_11' class='badge bg-neutral'>Стоит</span></div>";
  html += "<div class='status-row'><span>Направление движения (R0):</span><div><span id='b_0_9' class='badge bg-neutral'>Вверх ⬆️</span> <span id='b_0_8' class='badge bg-neutral'>Вниз ⬇️</span></div></div>";
  html += "<div class='status-row'><span>Лаз зафиксирован (R2:B12):</span><span id='b_2_12' class='badge bg-neutral'>Нет</span></div>";
  html += "<div class='status-row'><span>Занято направлением движения лаза (R2:B13):</span><span id='b_2_13' class='badge bg-neutral'>Нет</span></div>";
  html += "<div class='status-row'><span>Занято направлением движения (R2:B14):</span><span id='b_2_14' class='badge bg-neutral'>Нет</span></div>";
  html += "</div>";

  // Блок 2: Режимы работы и температурные блокировки
  html += "<div class='card'>";
  html += "<h3>Режимы и блокировки управления</h3>";
  html += "<div class='status-row'><span>Режим управления лазом (R2:B10):</span><span id='b_2_10' class='badge bg-info'>Авто</span></div>";
  html += "<div class='status-row'><span>Управление лазом по календарю (R2:B0):</span><span id='b_2_0' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Управление закрытием по расписанию (R2:B1):</span><span id='b_2_1' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Управление лазом по температуре (R2:B15):</span><span id='b_2_15' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Запрет открытия по темп. внутри (R2:B6):</span><span id='b_2_6' class='badge bg-neutral'>Нет</span></div>";
  html += "<div class='status-row'><span>Запрет открытия по темп. на улице (R2:B7):</span><span id='b_2_7' class='badge bg-neutral'>Нет</span></div>";
  html += "</div>";

  // Блок 3: Конфигурация времени и температур
  html += "<div class='card'>";
  html += "<h3>Временные и температурные уставки</h3>";
  html += "<div class='grid-2'>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Расчетное откр. (R10):</span><span id='r_10' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Расчетное закр. (R9):</span><span id='r_9' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Записанное откр. (R40):</span><span id='r_40' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Записанное закр. (R41):</span><span id='r_41' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Время отлож. закр. (R42):</span><span id='r_42' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Время хода привода (R43):</span><span id='r_43' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Темп. открытия (R33):</span><span id='r_33' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "</div>";
  html += "</div>";

  // Кнопка Назад
  html += "<a href='/autoChickenHous' class='btn'>⬅️ Назад в меню</a>";
  html += "</div>";

  // JavaScript AJAX скрипт динамического обновления
  html += R"rawliteral(
  <script>
  function updateDoorData() {
    fetch('/api/data')
      .then(r => r.json())
      .then(data => {
        let r0 = data["0"] || 0;
        let r1 = data["1"] || 0;
        let r2 = data["2"] || 0;

        // Разбор флагов движения (Регистр 0)
        updateBadge('b_0_8', (r0 >> 8) & 1, "АКТИВНО", "Выкл", "bg-danger", "bg-neutral");
        updateBadge('b_0_9', (r0 >> 9) & 1, "АКТИВНО", "Выкл", "bg-success", "bg-neutral");

        // Концевик и опрос датчика (Регистр 1)
        updateBadge('b_1_2', (r1 >> 2) & 1, "ОТКРЫТА", "Закрыта", "bg-success", "bg-danger");
        updateBadge('b_1_12', (r1 >> 12) & 1, "ОПРОС", "Нет", "bg-info", "bg-neutral");

        // Статусы и режимы лаза (Регистр 2)
        updateBadge('b_2_0', (r2 >> 0) & 1, "Вкл", "Выкл", "bg-success", "bg-neutral");
        updateBadge('b_2_1', (r2 >> 1) & 1, "Вкл", "Выкл", "bg-success", "bg-neutral");
        updateBadge('b_2_6', (r2 >> 6) & 1, "ЗАПРЕТ", "Нет", "bg-danger", "bg-success");
        updateBadge('b_2_7', (r2 >> 7) & 1, "ЗАПРЕТ", "Нет", "bg-danger", "bg-success");
        updateBadge('b_2_8', (r2 >> 8) & 1, "ОТКРЫТ", "Пассив", "bg-success", "bg-neutral");
        updateBadge('b_2_9', (r2 >> 9) & 1, "ЗАКРЫТ", "Пассив", "bg-danger", "bg-neutral");
        updateBadge('b_2_10', (r2 >> 10) & 1, "РУЧНОЙ", "Авто", "bg-danger", "bg-info");
        updateBadge('b_2_11', (r2 >> 11) & 1, "ДВИЖЕНИЕ", "Стоит", "bg-danger", "bg-neutral");
        updateBadge('b_2_12', (r2 >> 12) & 1, "БЛОК", "Нет", "bg-danger", "bg-neutral");
        updateBadge('b_2_13', (r2 >> 13) & 1, "ЗАНЯТО", "Нет", "bg-danger", "bg-neutral");
        updateBadge('b_2_14', (r2 >> 14) & 1, "ЗАНЯТО", "Нет", "bg-danger", "bg-neutral");
        updateBadge('b_2_15', (r2 >> 15) & 1, "Вкл", "Выкл", "bg-success", "bg-neutral");

        // Заполнение числовых регистров (9, 10, 33, 40, 41, 42, 43)
        const doorRegs =;
        doorRegs.forEach(reg => {
          let el = document.getElementById('r_' + reg);
          if (el && data[reg] !== undefined) {
            if(reg === 33) {
              el.innerText = data[reg] + "°C";
            } else {
              el.innerText = data[reg];
            }
          }
        });
      })
      .catch(err => console.error("Ошибка обновления данных лаза:", err));
  }

  function updateBadge(id, state, textOn, textOff, classOn, classOff) {
    let el = document.getElementById(id);
    if (!el) return;
    if (state === 1) {
      el.innerText = textOn;
      el.className = "badge " + classOn;
    } else {
      el.innerText = textOff;
      el.className = "badge " + classOff;
    }
  }

  setInterval(updateDoorData, 2000);
  updateDoorData();
  </script>
  )rawliteral";

  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

// Климат-контроль
void handleClimate() {
  String html = "<!DOCTYPE html><html lang='ru'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Климат-контроль</title>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f0f4f8; color: #1e293b; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }";
  html += ".container { width: 100%; max-width: 800px; }";
  html += "h2 { color: #0f172a; text-align: center; margin-bottom: 24px; font-size: 24px; }";
  html += ".card { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 4px 10px rgba(0,0,0,0.03); margin-bottom: 20px; border: 1px solid #e2e8f0; }";
  html += "h3 { margin-top: 0; color: #0284c7; border-bottom: 2px solid #e0f2fe; padding-bottom: 8px; font-size: 15px; text-transform: uppercase; letter-spacing: 0.5px; }";
  html += ".status-row { display: flex; justify-content: space-between; align-items: center; padding: 10px 6px; border-bottom: 1px solid #f1f5f9; }";
  html += ".status-row:last-child { border-bottom: none; }";
  html += ".badge { padding: 5px 12px; border-radius: 20px; font-size: 12px; color: white; font-weight: bold; min-width: 75px; text-align: center; text-transform: uppercase; }";
  html += ".bg-success { background: #10b981; }";
  html += ".bg-danger { background: #ef4444; }";
  html += ".bg-info { background: #3b82f6; }";
  html += ".bg-neutral { background: #64748b; }";
  html += ".val-box { font-weight: bold; color: #0f172a; background: #f8fafc; padding: 4px 12px; border-radius: 6px; border: 1px solid #e2e8f0; font-family: monospace; font-size: 15px; }";
  html += ".grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }";
  html += ".grid-3 { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 15px; }";
  html += "table { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 14px; }";
  html += "th, td { padding: 8px; border: 1px solid #e2e8f0; text-align: center; }";
  html += "th { background: #f8fafc; color: #0284c7; font-weight: bold; }";
  html += ".btn { display: block; text-align: center; padding: 12px; background: #0284c7; color: white; text-decoration: none; border-radius: 8px; font-weight: 600; margin-top: 15px; transition: 0.2s; border: none; }";
  html += ".btn:hover { background: #0369a1; }";
  html += "@media(max-width: 768px) { .grid-2, .grid-3 { grid-template-columns: 1fr; } }";
  html += "</style></head><body>";

  html += "<div class='container'>";
  html += "<h2>🌡️ Управление микроклиматом</h2>";

  // Блок 1: Текущие метеоданные и датчики
  html += "<div class='card'>";
  html += "<h3>Текущие измерения и датчики</h3>";
  html += "<div class='grid-2'>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Темп. в курятнике (R5):</span><span id='r_5' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Влажность в курятнике (R6):</span><span id='r_6' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Кол-во датчиков темп. (R28):</span><span id='r_28' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Загазованность (Датчик R0:B2):</span><span id='b_0_2' class='badge bg-neutral'>Норма</span></div>";
  html += "    <div class='status-row'><span>Опрос датчика газа (R1:B11):</span><span id='b_1_11' class='badge bg-neutral'>Нет</span></div>";
  html += "  </div>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Темп. на улице (R7):</span><span id='r_7' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Влажность на улице (R8):</span><span id='r_8' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Объем помещения (R55):</span><span id='r_55' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Темп. < 0°C на улице (R0:B3):</span><span id='b_0_3' class='badge bg-neutral'>Нет</span></div>";
  html += "    <div class='status-row'><span>Темп. < 0°C в доме (R0:B4):</span><span id='b_0_4' class='badge bg-neutral'>Нет</span></div>";
  html += "  </div>";
  html += "</div>";
  html += "</div>";

  // Блок 2: Состояние исполнительных устройств
  html += "<div class='card'>";
  html += "<h3>Исполнительные устройства</h3>";
  html += "<div class='grid-2'>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Нагрев (R0:B14):</span><span id='b_0_14' class='badge bg-neutral'>Выкл</span></div>";
  html += "    <div class='status-row'><span>Охлаждение (R0:B0):</span><span id='b_0_0' class='badge bg-neutral'>Выкл</span></div>";
  html += "    <div class='status-row'><span>Приточный вент. (R0:B12):</span><span id='b_0_12' class='badge bg-neutral'>Выкл</span></div>";
  html += "    <div class='status-row'><span>Вытяжной вент. (R0:B13):</span><span id='b_0_13' class='badge bg-neutral'>Выкл</span></div>";
  html += "  </div>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Управление осушителем (R1:B7):</span><span id='b_1_7' class='badge bg-neutral'>Выкл</span></div>";
  html += "    <div class='status-row'><span>Используется осушитель (R4:B4):</span><span id='b_4_4' class='badge bg-neutral'>Нет</span></div>";
  html += "    <div class='status-row'><span>Осушитель в сети (R4:B12):</span><span id='b_4_12' class='badge bg-neutral'>Выкл</span></div>";
  html += "    <div class='status-row'><span>Раб. нагревателя за час (R47):</span><span id='r_47' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "</div>";
  html += "</div>";

  // Блок 3: Режимы и логика вентиляции
  html += "<div class='card'>";
  html += "<h3>Режимы управления и логика работы</h3>";
  html += "<div class='status-row'><span>Речное (ручное) упр. вентиляцией (R1:B15):</span><span id='b_1_15' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Авто проветривание (R1:B3):</span><span id='b_1_3' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Ручное проветривание (R1:B4):</span><span id='b_1_4' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Управление охлаждением (R3:B7):</span><span id='b_3_7' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Управление вентиляторами ДНЕМ (R1:B13):</span><span id='b_1_13' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Управление вентиляторами во время СНА (R1:B14):</span><span id='b_1_14' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Коррекция вентиляторов (R1:B6):</span><span id='b_1_6' class='badge bg-neutral'>Выкл</span></div>";
  html += "<div class='status-row'><span>Управление вентиляцией по ГОСТ (R2:B4):</span><span id='b_2_4' class='badge bg-neutral'>Выкл</span></div>";
  html += "</div>";

  // Блок 4: Контроль загазованности и статистика вентиляции
  html += "<div class='card'>";
  html += "<h3>Аналитика вентиляции и загазованности</h3>";
  html += "<div class='status-row'><span>Превышение лимита газа в час (R1:B8):</span><span id='b_1_8' class='badge bg-neutral'>Норма</span></div>";
  html += "<div class='grid-2'>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Кол-во сработок газа в час (R31):</span><span id='r_31' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Общее число сработок газа (R32):</span><span id='r_32' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Включений вент./час при газе (R44):</span><span id='r_44' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Включений вентиляции в час (R34):</span><span id='r_34' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Допуст. проветриваний в час (R58):</span><span id='r_58' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Время смены воздуха, сек (R35):</span><span id='r_35' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "</div>";
  html += "</div>";

  // Блок 5: Оперативные уставки параметров
  html += "<div class='card'>";
  html += "<h3>Временные и целевые уставки</h3>";
  html += "<div class='grid-2'>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Заданная температура (R24):</span><span id='r_24' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Отклонение темп. (R25):</span><span id='r_25' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Заданная темп. днем (R29):</span><span id='r_29' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Температура охлаждения (R30):</span><span id='r_30' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "  <div>";
  html += "    <div class='status-row'><span>Заданная влажность (R26):</span><span id='r_26' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Отклонение влажности (R27):</span><span id='r_27' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Используется вентиляторов (R53):</span><span id='r_53' class='val-box'>--</span></div>";
  html += "    <div class='status-row'><span>Производит. вент-ов (R54):</span><span id='r_54' class='val-box'>--</span></div>";
  html += "  </div>";
  html += "</div>";
  html += "</div>";

  // Блок 6: Справочные объемы воздуха
  html += "<div class='card'>";
  html += "<h3>Объемы свежего воздуха (куб.м)</h3>";
  html += "<table>";
  html += "<thead><tr><th>Режим расчета</th><th>Лето</th><th>Зима</th><th>Осень</th></tr></thead>";
  html += "<tbody>";
  html += "  <tr><td><b>По ГОСТ</b></td><td id='r_60'>--</td><td id='r_61'>--</td><td id='r_62'>--</td></tr>";
  html += "  <tr><td><b>Текущий расчет</b></td><td id='r_63'>--</td><td id='r_64'>--</td><td id='r_65'>--</td></tr>";
  html += "</tbody></table>";
  html += "</div>";

  // Кнопка Назад
  html += "<a href='/autoChickenHous' class='btn'>⬅️ Назад в меню</a>";
  html += "</div>";

  // Внедрение JavaScript для автоматического фонового AJAX-опроса по API шлюза
  html += "<script>";
  html += "function updateClimateData() {";
  html += "  fetch('/api/data')";
  html += "    .then(r => r.json())";
  html += "    .then(data => {";
  html += "      let r0 = data['0'] || 0;";
  html += "      let r1 = data['1'] || 0;";
  html += "      let r2 = data['2'] || 0;";
  html += "      let r3 = data['3'] || 0;";
  html += "      let r4 = data['4'] || 0;";
  html += "      updateBadge('b_0_0', (r0 >> 0) & 1, 'АКТИВНО', 'Выкл', 'bg-danger', 'bg-neutral');";
  html += "      updateBadge('b_0_2', (r0 >> 2) & 1, 'ГАЗ!!', 'Норма', 'bg-danger', 'bg-success');";
  html += "      updateBadge('b_0_3', (r0 >> 3) & 1, '< 0°C', 'Нет', 'bg-info', 'bg-neutral');";
  html += "      updateBadge('b_0_4', (r0 >> 4) & 1, '< 0°C', 'Нет', 'bg-info', 'bg-neutral');";
  html += "      updateBadge('b_0_12', (r0 >> 12) & 1, 'РАБОТА', 'Выкл', 'bg-success', 'bg-neutral');";
  html += "      updateBadge('b_0_13', (r0 >> 13) & 1, 'РАБОТА', 'Выкл', 'bg-success', 'bg-neutral');";
  html += "      updateBadge('b_0_14', (r0 >> 14) & 1, 'НАГРЕВ', 'Выкл', 'bg-danger', 'bg-neutral');";
  html += "      updateBadge('b_1_3', (r1 >> 3) & 1, 'Вкл', 'Выкл', 'bg-success', 'bg-neutral');";
  html += "      updateBadge('b_1_4', (r1 >> 4) & 1, 'Вкл', 'Выкл', 'bg-success', 'bg-neutral');";
  html += "      updateBadge('b_1_6', (r1 >> 6) & 1, 'АКТИВНА', 'Выкл', 'bg-info', 'bg-neutral');";
  html += "      updateBadge('b_1_7', (r1 >> 7) & 1, 'Вкл', 'Выкл', 'bg-success', 'bg-neutral');";
  html += "      updateBadge('b_1_8', (r1 >> 8) & 1, 'ПРЕВЫШЕНИЕ', 'Норма', 'bg-danger', 'bg-success');";
  html += "      updateBadge('b_1_11', (r1 >> 11) & 1, 'ОПРОС', 'Нет', 'bg-info', 'bg-neutral');";
  html += "      updateBadge('b_1_13', (r1 >> 13) & 1, 'ДЕНЬ', 'Выкл', 'bg-info', 'bg-neutral');";
  html += "      updateBadge('b_1_14', (r1 >> 14) & 1, 'СОН', 'Выкл', 'bg-neutral', 'bg-neutral');";
  html += "      updateBadge('b_1_15', (r1 >> 15) & 1, 'РУЧНОЙ', 'Авто', 'bg-danger', 'bg-success');";
  html += "      updateBadge('b_2_4', (r2 >> 4) & 1, 'ГОСТ', 'Выкл', 'bg-success', 'bg-neutral');";
  html += "      updateBadge('b_3_7', (r3 >> 7) & 1, 'Вкл', 'Выкл', 'bg-success', 'bg-neutral');";
  html += "      updateBadge('b_4_4', (r4 >> 4) & 1, 'Да', 'Нет', 'bg-success', 'bg-neutral');";
  html += "      updateBadge('b_4_12', (r4 >> 12) & 1, 'В СЕТИ', 'Выкл', 'bg-success', 'bg-neutral');";
  html += "      const climateRegs = [5, 6, 7, 8, 24, 25, 26, 27, 28, 29, 30, 31, 32, 34, 35, 44, 47, 53, 54, 55, 58, 60, 61, 62, 63, 64, 65];";
  html += "      climateRegs.forEach(reg => {";
  html += "        let el = document.getElementById('r_' + reg);";
  html += "        if (el && data[reg] !== undefined) {";
  html += "          let val = data[reg];";
  html += "          if (reg===5||reg===7||reg===24||reg===25||reg===29||reg===30) { el.innerText = val + ' °C'; }";
  html += "          else if (reg===6||reg===8||reg===26||reg===27) { el.innerText = val + ' %'; }";
  html += "          else if (reg===55) { el.innerText = val + ' м³'; }";
  html += "          else { el.innerText = val; }";
  html += "        }";
  html += "      });";
  html += "    })";
  html += "    .catch(err => console.error('Ошибка обновления:', err));";
  html += "}";
  html += "function updateBadge(id, state, textOn, textOff, classOn, classOff) {";
  html += "  let el = document.getElementById(id);";
  html += "  if (!el) return;";
  html += "  if (state === 1) { el.innerText = textOn; el.className = 'badge ' + classOn; }";
  html += "  else { el.innerText = textOff; el.className = 'badge ' + classOff; }";
  html += "}";
  html += "setInterval(updateClimateData, 2000);";
  html += "updateClimateData();";
  html += "</script>";

  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

// 3. Страница настроек
void handleSettings() {
  String html = getHeader("Настройки шлюза");
  
  // Блок Wi-Fi
  html += "<div class='card'><h3>📶 Сеть Wi-Fi</h3>";
  html += "<button onclick='scan(this)' class='btn' style='width:100%'>🔄 Сканировать сети</button>";
  html += "<select id='netList' onchange='document.getElementsByName(\"ssid\")[0].value=this.value'></select>";
  html += "<form action='/saveWiFi' method='POST'>";
  html += "<input type='text' name='ssid' placeholder='SSID роутера' required>";
  html += "<input type='password' name='pass' placeholder='Пароль' required>";
  html += "<input type='submit' value='💾 Сохранить и перезагрузить' class='btn' style='background:#28a745; width:100%'>";
  html += "</form>";
  html += "</div>";

  // Блок MQTT
  html += "<div class='card'><h3>☁️ Параметры MQTT</h3>";
  html += "<form action='/saveMQTT' method='POST'>";
  html += "<input type='text' name='mq_user' value='" + mqtt_user + "' placeholder='Пользователь'>";
  html += "<input type='password' name='mq_pass' value='" + mqtt_pass + "' placeholder='Пароль'>";
  html += "<input type='text' name='mq_host' value='" + mqtt_host + "' placeholder='Брокер (host)'>";
  html += "<input type='number' name='mq_port' value='" + String(mqtt_port) + "' placeholder='Порт'>";
  html += "<input type='submit' value='💾 Сохранить и перезагрузить' class='btn' style='background:#28a745; width:100%'>";
  html += "</div>";
  html += "</form></div></body></html>";

  // JavaScript для обработки уровней сигнала
  html += R"rawliteral(<script>
    function getSignalIcon(dbm) {
      if (dbm >= -50) return '🟢 (Отличный)';
      if (dbm >= -70) return '🟡 (Средний)';
      if (dbm >= -85) return '🟠 (Слабый)';
      return '🔴 (Очень слабый)';
    }

    function scan(btn){
      btn.disabled = true;
      const sel = document.getElementById('netList');
      sel.innerHTML = '<option>Ищу сети...</option>';
      
      fetch('/api/scan').then(r => r.json()).then(data => {
        sel.innerHTML = '<option value="">-- Нажмите для выбора --</option>';
        // Сортировка по уровню сигнала (сначала сильные)
        data.sort((a, b) => b.rssi - a.rssi);
        
        data.forEach(n => {
          let opt = document.createElement('option');
          opt.value = n.ssid;
          opt.text = `${n.ssid} ${getSignalIcon(n.rssi)} [${n.rssi} dBm]`;
          sel.appendChild(opt);
        });
        btn.disabled = false;
      }).catch(() => {
        sel.innerHTML = '<option>Ошибка сканирования</option>';
        btn.disabled = false;
      });
    }

    function togglePass() {
      const p = document.getElementById('passInput');
      const e = document.getElementById('eyeIcon');
      if (p.type === 'password') {
        p.type = 'text';
        e.innerText = '🔒'; // Иконка закрытого глаза или замка
      } else {
        p.type = 'password';
        e.innerText = '👁️';
      }
    }

    window.onload = () => scan(document.querySelector('button[onclick^="scan"]'));
  </script>)rawliteral";
  
  webServer.send(200, "text/html", html);
}

// 4.1 Обработка сохранения Wi-Fi
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

// 4.2 Обработка сохранения MQTT
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
  loadSettings();

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
    if (WiFi.status() != WL_CONNECTED && sta_ssid != "") {
      Serial.print("Переподключаемся к роутеру...");
      WiFi.begin(sta_ssid.c_str(), sta_password.c_str()); // Автопереподключение
    }
    lastCheck = millis();
  }

  delay(1); // Для стабильности
}