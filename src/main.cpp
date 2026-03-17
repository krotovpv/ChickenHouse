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
  {0, 65535},{1, 65535},{2, 65535},{3, 65535},{4, 65535},{5, 65535},{6, 65535},{7, 65535},{8, 65535},{9, 65535},
  {10, 65535},{11, 65535},{12, 65535},{13, 65535},{14, 65535},{15, 65535},{16, 65535},{17, 65535},{18, 65535},{19, 65535},
  {20, 65535},{21, 65535},{22, 65535},{23, 65535},{24, 65535},{25, 65535},{26, 65535},{27, 65535},{28, 65535},{29, 65535},
  {30, 65535},{31, 65535},{32, 65535},{33, 65535},{34, 65535},{35, 65535},{36, 65535},{37, 65535},{38, 65535},{39, 65535},
  {40, 65535},{41, 65535},{42, 65535},{43, 65535},{44, 65535},{45, 65535},{46, 65535},{47, 65535},{48, 65535},{49, 65535},
  {50, 65535},{51, 65535},{52, 65535},{53, 65535},{54, 65535},{55, 65535},{56, 65535},{57, 65535},{58, 65535},{59, 65535},
  {60, 65535},{61, 65535},{62, 65535},{63, 65535},{64, 65535},{65, 65535},{66, 65535},{67, 65535},{68, 65535},{69, 65535},
  {70, 65535},{71, 65535},{72, 65535},{73, 65535},{74, 65535},{75, 65535},{76, 65535},{77, 65535},{78, 65535},{79, 65535},
  {80, 65535},{81, 65535},{82, 65535},{83, 65535},{84, 65535},{85, 655350},{86, 65535},{87, 65535},{88, 65535},{89, 65535},
  {90, 65535},{91, 65535},{92, 65535},{93, 65535},{94, 65535},{95, 65535},{96, 65535},{97, 65535},{98, 65535},{99, 65535},
  {100, 65535},{101, 65535},
  {110, 65535},{111, 65535},{112, 65535},{113, 65535},{114, 65535},{115, 65535},{116, 65535},{117, 65535},{118, 65535},{119, 65535},
  {120, 65535},{121, 65535},{122, 65535},{123, 65535},{124, 65535},{125, 65535},{126, 65535},{127, 65535},{128, 65535},{129, 65535},
  {130, 65535},{131, 65535},{132, 65535},{133, 65535},{134, 65535},{135, 65535},{136, 65535},{137, 65535},{138, 65535},{139, 65535},
  {140, 65535},{141, 65535},{142, 65535},{143, 65535},{144, 65535},{145, 65535},{146, 65535},{147, 65535},{148, 65535},{149, 65535},
  {150, 65535},{151, 65535}
};

// --- Инициализация настроек из Flash ---
void loadSettings() {
  prefs.begin("configWiFi", true); // Открываем в режиме чтения (true)
  sta_ssid = prefs.getString("ssid", ""); // Дефолт, если пусто
  sta_password = prefs.getString("pass", "");
  prefs.end();

  // prefs.begin("configMQTT2", true); // Открываем в режиме чтения (true)
  // mqtt_user  = prefs.getString("mqtt_user", "");
  // mqtt_pass  = prefs.getString("mqtt_pass", "");
  // mqtt_host  = prefs.getString("mqtt_host", "mqtt-dashboard.com");
  // mqtt_port  = prefs.getInt("mqtt_port", 1883);
  // mqtt_topic = prefs.getString("mqtt_topic", "esp32/chickenhouse");
  // prefs.end();
  mqtt_user = "";
  mqtt_pass = "";
  mqtt_host = "test.mosquitto.org";
  mqtt_port = 1883;
  mqtt_topic = "esp32/chickenhouse";
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
  html += "  <a href='/table' class='nav-btn' style='background:#4e73df;'>📊<br>Данные</a>";
  html += "  <a href='/settings' class='nav-btn' style='background:#1cc88a;'>⚙️<br>Опции</a>";
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
    // Названия для битов 100 регистра
    const bitNames = {
      "100": ["Бит 0", "Бит 1", "Бит 2", "Бит 3", "Бит 4", "Бит 5", "Бит 6", "Бит 7"] //если для 8-15 не указаны имена то они заполняются по шаблону [Бит #]
    };

    function sendBit(addr, bit, state) {
        // Отправляем команду и СРАЗУ вызываем обновление данных после ответа сервера
        fetch(`/api/write_bit?addr=${addr}&bit=${bit}&state=${state ? 1 : 0}`)
        .then(r => { if(r.ok) setTimeout(updateData, 100); }); 
    }

    function updateData() {
      fetch('/api/data').then(r => r.json()).then(data => {
        for (const [key, value] of Object.entries(data)) {
          let row = document.getElementById('reg-' + key);
          if (!row) continue;
          
          const valCell = row.querySelector('.val-cell');

          // --- ЛОГИКА ДЛЯ 100 и 101 (ПЕРЕКЛЮЧАТЕЛИ) ---
          if (key == "100") {
            let controls = valCell.querySelector('.bit-controls');
            if (!controls) {
              valCell.innerHTML = `<div class="bit-controls" style="display:grid; grid-template-columns: 1fr 1fr; gap:8px; padding:8px;"></div><div class="raw-val" style="font-size:10px; color:#999; margin-top:4px;"></div>`;
              controls = valCell.querySelector('.bit-controls');
            }
            
            let togglesHtml = '';
            for (let i = 0; i < 16; i++) {
              const isSet = (value >> i) & 1;
              const name = bitNames[key][i] || `Бит ${i}`;
              togglesHtml += `
                <div style="display:flex; align-items:center; justify-content:space-between; background:#f0f2f5; padding:4px 8px; border-radius:4px;">
                  <span style="font-size:11px;">${name}</span>
                  <label class="switch"><input type="checkbox" ${isSet ? 'checked' : ''} onchange="sendBit(${key}, ${i}, this.checked)"><span class="slider"></span></label>
                </div>`;
            }
            controls.innerHTML = togglesHtml;
            valCell.querySelector('.raw-val').innerText = "Значение: " + value;

          // --- ЛОГИКА ДЛЯ 3 и 4 (ИНДИКАТОРЫ ТОЧКИ) ---
          } else if (key == "0" || key == "1" || key == '2' || key == '3' || key == '4') {
            let bitContainer = valCell.querySelector('.bit-row');
            if (!bitContainer) {
              valCell.innerHTML = `${value}<div class="bit-row" style="display:grid; grid-template-columns: repeat(8, 1fr); gap:4px; margin-top:8px;"></div>`;
              bitContainer = valCell.querySelector('.bit-row');
            } else {
                // Обновляем числовое значение, если оно изменилось
                if (valCell.firstChild.nodeValue != value) valCell.firstChild.nodeValue = value;
            }
            
            let dotsHtml = '';
            for (let i = 15; i >= 0; i--) { // Показываем 8 бит для компактности
              const isActive = (value >> i) & 1;
              dotsHtml += `<div><div class="bit-dot ${isActive ? 'bit-on' : ''}"></div><span class="bit-label">${i}</span></div>`;
            }
            bitContainer.innerHTML = dotsHtml;

          // --- ДЛЯ ВСЕХ ОСТАЛЬНЫХ РЕГИСТРОВ ---
          } else {
            valCell.innerText = value;
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