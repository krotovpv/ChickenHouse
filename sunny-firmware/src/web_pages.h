#include <Arduino.h>
#include "responsive_web_server.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "registers.h"

// Объявляем внешние переменные и объекты, чтобы компилятор знал о них
extern ResponsiveWebServer webServer;
String tableStyleUrl();
String tableScriptUrl();
String sunnyLogoUrl();
String uiBuildId();
extern std::map<uint16_t, uint16_t> memo;
extern String sta_ssid;
extern String sta_password;
extern String mqtt_user;
extern String mqtt_pass;
extern String mqtt_host;
extern int mqtt_port;
extern String BOT_TOKEN;
extern String CHAT_ID;
extern uint8_t humidityLimit;
extern bool humidityEnabled;
extern bool humidityRepeat;
extern uint16_t humidityRepeatMinutes;
bool snapshotRegisters(std::map<uint16_t, uint16_t>& snapshot);

// Прототипы функций сохранения настроек, которые остаются в главном файле
//void saveSettingsWiFi(String s, String p);
//void saveSettingsMQTT(String mq_u, String mq_pass, String mq_h, String mq_p, String mq_t);

String getHeader(String title, bool showHome = true);
String getPageNavigation();
void handleIndex();
void handleApiData();
void handleTable();
void handleAutoChickenHous();
void handleLight();
void handleAlerts();
void handleVentilation();
void handleFeeding();
void handleDoor();
void handleClimate();
void handleSettings();
void handleOtaPage();
