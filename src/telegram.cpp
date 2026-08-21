#include "telegram.h"

static bool wasAlarmActive = false; // Храним прошлый статус аварии

void sendTelegramMessage(String text) {
  // Работаем с Telegram только если есть подключение к интернету
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure secureClient;
  // Отключаем строгую проверку SSL-сертификатов ради экономии памяти ESP32
  secureClient.setInsecure(); 

  if (secureClient.connect("api.telegram.org", 443)) {
    // Формируем URL для GET-запроса (текст обязательно должен быть закодирован, 
    // но для простых сообщений без спецсимволов сойдет прямая передача)
    String url = "/bot" + BOT_TOKEN + "/sendMessage?chat_id=" + CHAT_ID + "&text=" + text;
    
    // Заменяем пробелы на безопасный для URL символ %20
    url.replace(" ", "%20");

    secureClient.print(String("GET ") + url + " HTTP/1.1\r\n" +
                       "Host: api.telegram.org\r\n" +
                       "Connection: close\r\n\r\n");
    
    Serial.println("Запрос в Telegram отправлен: " + text);
    
    // Ждем ответа сервера (опционально, предотвращает зависание)
    unsigned long timeout = millis();
    while (secureClient.available() == 0) {
      if (millis() - timeout > 3000) {
        Serial.println("Превышено время ожидания ответа Telegram");
        return;
      }
    }
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
      String msg = "🚨 ВНИМАНИЕ! В курятнике обнаружена неисправность:\n";
      if (batteryAlarm) msg += "- Авария батарейки (R1:B9)\n";
      if (sensorAlarm)  msg += "- Отказ датчика температуры (R1:B10)\n";
      if (calcAlarm)    msg += "- Ошибка расчета вентиляции (R3:B5)\n";
      if (feedAlarm)    msg += "- Ошибка процесса кормления (R3:B9)\n";
      if (totalFeederErrors > 0) msg += "- Обнаружены системные ошибки оборудования (" + String(totalFeederErrors) + " шт.)\n";
      
      sendTelegramMessage(msg);
      wasAlarmActive = true; // Запоминаем, что мы в состоянии аварии
    }
    
    // Сценарий Б: Авария только что ИСЧЕЗЛА (раньше была, а теперь всё в норме)
    if (!isAlarmActive && wasAlarmActive) {
      sendTelegramMessage("✅ Все аварии устранены. Работа курятника полностью восстановлена в штатном режиме.");
      wasAlarmActive = false; // Запоминаем, что всё пришло в норму
    }
  }
}