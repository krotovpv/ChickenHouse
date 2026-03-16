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
  {0, 111},
  {1, 222},
  {3, 333},
  {4, 444},
  {10, 5555},
  {100, 9999}
};

// --- Инициализация настроек из Flash ---
void loadSettings() {
  prefs.begin("configWiFi", true); // Открываем в режиме чтения (true)
  sta_ssid = prefs.getString("ssid", ""); // Дефолт, если пусто
  sta_password = prefs.getString("pass", "");
  prefs.end();

  prefs.begin("configMQTT", true); // Открываем в режиме чтения (true)
  mqtt_user  = prefs.getString("mqtt_user", "");
  mqtt_pass  = prefs.getString("mqtt_pass", "");
  mqtt_host  = prefs.getString("mqtt_host", "mqtt-dashboard.com");
  mqtt_port  = prefs.getInt("mqtt_port", 1883);
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
    char buffer[512];
    serializeJson(doc, buffer);
    mqttClient.publish(mqtt_topic.c_str(), buffer);
  }
}

void reconnectMQTT() {
  if (!mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
    String clientId = "ESP32_Modbus_" + String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_pass.c_str())) {
      Serial.println("MQTT подключен");
      
      // Подписываемся на топик управления: ".../set"
      // Сообщение вида {"address": 1, "value": 777}
      String setTopic = mqtt_topic + "/set";
      mqttClient.subscribe(setTopic.c_str());
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  JsonDocument doc;
  deserializeJson(doc, payload, length);

  if (doc["address"].is<int16_t>() && doc["value"].is<int16_t>()) {
    uint16_t addr = doc["address"];
    uint16_t val  = doc["value"];
    
    // Записываем в наше Modbus-хранилище
    memo[addr] = val;
    
    Serial.printf("MQTT Write: Регистр %d установлен в %d\n", addr, val);
    
    // Опционально: сразу отправляем подтверждение обратно
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


// --- Обработчики страниц ---

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

  html += R"rawliteral(
  <style>
    .flash-update { 
      background-color: #8ce757 !important;
      transition: background-color 2s ease-in-out;
    }
  </style>
  )rawliteral";

  html += "<table border='1' cellpadding='8' id='regTable'>";
  html += "<thead><tr><th>Регистр</th><th>Значение</th></tr></thead>";
  html += "<tbody id='tableBody'>";

  // Первоначальное заполнение
  for (const auto& item : memo) {
    html += "<tr id='reg-" + String(item.first) + "'>";
    html += "<td>" + String(item.first) + "</td>";
    html += "<td class='val-cell'>" + String(item.second) + "</td></tr>";
  }
  
  html += "</tbody></table>";

  // JavaScript для обновления данных каждые 2 секунды
  html += R"rawliteral(
    <script>
      function updateData() {
        fetch('/api/data')
          .then(response => response.json())
          .then(data => {
            const tbody = document.getElementById('tableBody');
            
            for (const [key, value] of Object.entries(data)) {
              let row = document.getElementById('reg-' + key);
              
              if (!row) {
                // Если строки еще нет, создаем её
                row = document.createElement('tr');
                row.id = 'reg-' + key;
                row.innerHTML = `<td>${key}</td><td class="val-cell">${value}</td>`;
                tbody.appendChild(row);
              } else {
                const valCell = row.querySelector('.val-cell');
                // Если значение изменилось
                if (valCell.innerText != value) {
                  valCell.innerText = value;
                  // Добавляем класс подсветки
                  valCell.classList.add('flash-update');
                  // Убираем его через 2 секунды
                  setTimeout(() => {
                    valCell.classList.remove('flash-update');
                  }, 2000);
                }
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


// 4. Обработка сохранения
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
}

void loop() {
  dnsServer.processNextRequest(); // Обработка DNS запросов
  webServer.handleClient(); // Обработка веб-запросов
  mqttClient.loop();

  // Мониторинг статуса подключения Wi-Fi в фоне
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) { // Каждые 10 секунд
    if (WiFi.status() != WL_CONNECTED && sta_ssid != "") {
      Serial.print("Переподключаемся к роутеру...");
      WiFi.begin(sta_ssid.c_str(), sta_password.c_str()); // Автопереподключение
    }
    lastCheck = millis();
  }

  // Мониторинг статуса подключения MQTT в фоне
  static unsigned long lastMqtt = 0;
  if (millis() - lastMqtt > 10000) { // Каждые 10 секунд
    reconnectMQTT();
    publishModbusData();
    lastMqtt = millis();
  }

  delay(2); // Небольшая пауза для стабильности стека
}