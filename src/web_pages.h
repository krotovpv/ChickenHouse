#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "registers.h"

// Объявляем внешние переменные и объекты, чтобы компилятор знал о них
extern WebServer webServer;
extern std::map<uint16_t, uint16_t> memo;
extern String sta_ssid;
extern String sta_password;
extern String mqtt_user;
extern String mqtt_pass;
extern String mqtt_host;
extern int mqtt_port;

// Прототипы функций сохранения настроек, которые остаются в главном файле
//void saveSettingsWiFi(String s, String p);
//void saveSettingsMQTT(String mq_u, String mq_pass, String mq_h, String mq_p, String mq_t);

String getHeader(String title);
void handleIndex();
void handleApiData();
void handleTable();
void handleAutoChickenHous();
void handleLight();
void handleAlerts();
void handleStatus();
void handleFeeding();
void handleDoor();
void handleClimate();
void handleSettings();
void handleOtaPage();