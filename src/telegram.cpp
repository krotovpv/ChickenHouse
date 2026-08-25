#include "telegram.h"

static bool wasAlarmActive = false; // Храним прошлый статус аварии

void sendTelegramMessage(String text) {
  // Работаем с Telegram только если есть подключение к интернету
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure secureClient;
  // Отключаем строгую проверку SSL-сертификатов ради экономии памяти ESP32
  secureClient.setInsecure(); 

  // Устанавливаем короткий таймаут на операцию, чтобы не вешать основной loop()
  secureClient.setTimeout(2000); // 2 секунды вместо стандартных 15+

  if (secureClient.connect("api.telegram.org", 443)) {
    // Формируем URL для GET-запроса (текст обязательно должен быть закодирован, 
    // но для простых сообщений без спецсимволов сойдет прямая передача)
    String url = "/bot" + BOT_TOKEN + "/sendMessage?chat_id=" + CHAT_ID + "&text=" + text;
    
    // Заменяем пробелы на безопасный для URL символ %20
    url.replace(" ", "%20");

    // Собираем запрос в одну String-переменную для стабильности
    String httpRequest = "GET " + url + " HTTP/1.1\r\n" +
                         "Host: api.telegram.org\r\n" +
                         "Connection: close\r\n\r\n";

    secureClient.print(httpRequest);
    
    Serial.println("Запрос в Telegram отправлен: " + text);
    
    // Мягкое чтение ответа без блокирующих циклов while(available == 0)
    // Ждем первого байта не более 500мс
    unsigned long startWait = millis();
    while (secureClient.connected() && !secureClient.available()) {
      if (millis() - startWait > 500) {
        Serial.println("Превышен таймаут ожидания ответа от Telegram API");
        break;
      }
      delay(10);
    }

    // Очищаем буфер ответа, если сервер что-то прислал, и закрываем
    while (secureClient.available()) {
      secureClient.read(); 
    }

    secureClient.stop(); 
  } else {
    Serial.println("Ошибка подключения к api.telegram.org");
  }
}

void checkTelegram()
{
    // 2. Логика Telegram-оповещений (только если есть интернет)
  if (WiFi.status() == WL_CONNECTED) {
    // Извлекаем актуальные регистры из карты памяти Modbus
    uint16_t r1 = memo[1];
    uint16_t r3 = memo[3];
    
    // Считываем биты критических аварий (как на вкладке Аварии)
    bool batteryAlarm = (r1 >> 9) & 1;
    bool sensorAlarm = (r1 >> 10) & 1;
    bool calcAlarm = (r3 >> 5) & 1;
    bool feedAlarm = (r3 >> 9) & 1;
    
    // Считаем сумму счетчиков ошибок по кормушкам и Wi-Fi (регистры 48-52)
    uint16_t totalFeederErrors = memo[48] + memo[49] + memo[50] + memo[51] + memo[52];
    
    // Общий флаг: есть ли сейчас хоть одна проблема
    bool isAlarmActive = batteryAlarm || sensorAlarm || calcAlarm || feedAlarm || (totalFeederErrors > 0);
    
    // Сценарий А: Авария только что ПОЯВИЛАСЬ (раньше не было, а теперь есть)
    if (isAlarmActive && !wasAlarmActive) {
      
       String msg = "🚨 ВНИМАНИЕ! В курятнике обнаружена неисправность:";
      if (batteryAlarm) msg += "\n- Авария батарейки (R1:B9)";
      if (sensorAlarm)  msg += "\n- Отказ датчика температуры (R1:B10)";
      if (calcAlarm)    msg += "\n- Ошибка расчета вентиляции (R3:B5)";
      if (feedAlarm)    msg += "\n- Ошибка процесса кормления (R3:B9)";
      if (totalFeederErrors > 0) msg += "\n- Обнаружены системные ошибки оборудования (" + String(totalFeederErrors) + " шт.)";
      msg.replace(" ", "%20");
      msg.replace("\n", "%0A");

      sendTelegramMessage(msg);
      wasAlarmActive = true;
    }
    
    // Сценарий Б: Авария только что ИСЧЕЗЛА (раньше была, а теперь всё в норме)
    if (!isAlarmActive && wasAlarmActive) {
      sendTelegramMessage("✅ Все аварии устранены. Работа курятника полностью восстановлена в штатном режиме.");
      wasAlarmActive = false; // Запоминаем, что всё пришло в норму
    }
  }
}