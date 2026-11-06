#include "telegram.h"
#include <time.h>

extern bool snapshotRegisters(std::map<uint16_t, uint16_t>& snapshot);

static bool wasAlarmActive = false;

// Go Daddy Root Certificate Authority - G2, downloaded from the CA's official repository.
static const char TELEGRAM_ROOT_CA[] PROGMEM = R"CERT(-----BEGIN CERTIFICATE-----
MIIDxTCCAq2gAwIBAgIBADANBgkqhkiG9w0BAQsFADCBgzELMAkGA1UEBhMCVVMx
EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoT
EUdvRGFkZHkuY29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRp
ZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTA5MDkwMTAwMDAwMFoXDTM3MTIzMTIz
NTk1OVowgYMxCzAJBgNVBAYTAlVTMRAwDgYDVQQIEwdBcml6b25hMRMwEQYDVQQH
EwpTY290dHNkYWxlMRowGAYDVQQKExFHb0RhZGR5LmNvbSwgSW5jLjExMC8GA1UE
AxMoR28gRGFkZHkgUm9vdCBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkgLSBHMjCCASIw
DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAL9xYgjx+lk09xvJGKP3gElY6SKD
E6bFIEMBO4Tx5oVJnyfq9oQbTqC023CYxzIBsQU+B07u9PpPL1kwIuerGVZr4oAH
/PMWdYA5UXvl+TW2dE6pjYIT5LY/qQOD+qK+ihVqf94Lw7YZFAXK6sOoBJQ7Rnwy
DfMAZiLIjWltNowRGLfTshxgtDj6AozO091GB94KPutdfMh8+7ArU6SSYmlRJQVh
GkSBjCypQ5Yj36w6gZoOKcUcqeldHraenjAKOc7xiID7S13MMuyFYkMlNAJWJwGR
tDtwKj9useiciAF9n9T521NtYJ2/LOdYq7hfRvzOxBsDPAnrSTFcaUaz4EcCAwEA
AaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYE
FDqahQcQZyi27/a9BUFuIMGU2g/eMA0GCSqGSIb3DQEBCwUAA4IBAQCZ21151fmX
WWcDYfF+OwYxdS2hII5PZYe096acvNjpL9DbWu7PdIxztDhC2gV7+AJ1uP2lsdeu
9tfeE8tTEH6KRtGX+rcuKxGrkLAngPnon1rpN5+r5N9ss4UXnT3ZJE95kTXWXwTr
gIOrmgIttRD02JDHBHNA7XIloKmf7J6raBKZV8aPEjoJpL1E/QYVN8Gb5DKj7Tjo
2GTzLH4U/ALqn83/B2gX2yKQOC16jdFU8WnjXzPKej17CuPKf1855eJ1usV2GDPO
LPAvTK33sefOT6jEm0pUBsV/fdUID+Ic/n4XuKxe9tQWskMJDE32p2u0mYRlynqI
4uJEvlz36hz1
-----END CERTIFICATE-----
)CERT";

static String urlEncode(const String& value) {
  static const char hex[] = "0123456789ABCDEF";
  String encoded;
  encoded.reserve(value.length() * 3);
  for (size_t i = 0; i < value.length(); ++i) {
    uint8_t c = static_cast<uint8_t>(value[i]);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += static_cast<char>(c);
    } else {
      encoded += '%';
      encoded += hex[c >> 4];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

void sendTelegramMessage(String text) {
  if (WiFi.status() != WL_CONNECTED || BOT_TOKEN.length() == 0 || CHAT_ID.length() == 0) return;

  // Certificate validity checks require a real clock. Try SNTP once per message if needed.
  if (time(nullptr) < 1700000000) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    const unsigned long started = millis();
    while (time(nullptr) < 1700000000 && millis() - started < 8000) delay(100);
    if (time(nullptr) < 1700000000) {
      Serial.println("Не удалось получить время для проверки TLS сертификата Telegram");
      return;
    }
  }

  WiFiClientSecure secureClient;
  secureClient.setCACert(TELEGRAM_ROOT_CA);
  secureClient.setTimeout(2000);

  if (!secureClient.connect("api.telegram.org", 443)) {
    Serial.println("Ошибка TLS подключения к api.telegram.org");
    return;
  }

  // POST body encoding handles spaces, newlines, Cyrillic, and reserved URL characters.
  String body = "chat_id=" + urlEncode(CHAT_ID) + "&text=" + urlEncode(text);
  String request = "POST /bot" + BOT_TOKEN + "/sendMessage HTTP/1.1\r\n" +
                   "Host: api.telegram.org\r\n" +
                   "Content-Type: application/x-www-form-urlencoded\r\n" +
                   "Content-Length: " + String(body.length()) + "\r\n" +
                   "Connection: close\r\n\r\n";
  secureClient.print(request);
  secureClient.print(body);

  unsigned long waitStarted = millis();
  while (secureClient.connected() && !secureClient.available() && millis() - waitStarted < 2000) delay(10);
  while (secureClient.available()) secureClient.read();
  secureClient.stop();
}

void checkTelegram() {
  if (WiFi.status() != WL_CONNECTED) return;

  std::map<uint16_t, uint16_t> snapshot;
  if (!snapshotRegisters(snapshot)) return;

  uint16_t r1 = snapshot[1];
  uint16_t r3 = snapshot[3];
  bool batteryAlarm = (r1 >> 9) & 1;
  bool sensorAlarm = (r1 >> 10) & 1;
  bool calcAlarm = (r3 >> 5) & 1;
  bool feedAlarm = (r3 >> 9) & 1;
  uint16_t totalFeederErrors = snapshot[48] + snapshot[49] + snapshot[50] + snapshot[51] + snapshot[52];
  bool isAlarmActive = batteryAlarm || sensorAlarm || calcAlarm || feedAlarm || totalFeederErrors > 0;

  if (isAlarmActive && !wasAlarmActive) {
    String msg = "🚨 ВНИМАНИЕ! В курятнике обнаружена неисправность:";
    if (batteryAlarm) msg += "\n- Авария батарейки (R1:B9)";
    if (sensorAlarm) msg += "\n- Отказ датчика температуры (R1:B10)";
    if (calcAlarm) msg += "\n- Ошибка расчета вентиляции (R3:B5)";
    if (feedAlarm) msg += "\n- Ошибка процесса кормления (R3:B9)";
    if (totalFeederErrors > 0) msg += "\n- Обнаружены системные ошибки оборудования (" + String(totalFeederErrors) + " шт.)";
    sendTelegramMessage(msg);
    wasAlarmActive = true;
  }

  if (!isAlarmActive && wasAlarmActive) {
    sendTelegramMessage("✅ Все аварии устранены. Работа курятника полностью восстановлена в штатном режиме.");
    wasAlarmActive = false;
  }
}
