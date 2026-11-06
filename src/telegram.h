#include <WiFiClientSecure.h>
#include "registers.h"

extern String BOT_TOKEN;
extern String CHAT_ID;

void sendTelegramMessage(String text);
void checkTelegram();
