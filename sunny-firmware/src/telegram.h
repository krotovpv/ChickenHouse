#include <WiFiClientSecure.h>
#include "registers.h"

extern String BOT_TOKEN;
extern String CHAT_ID;

bool sendTelegramMessage(String text);
void checkTelegram();
void humidityTelegramTask(void* arg);
