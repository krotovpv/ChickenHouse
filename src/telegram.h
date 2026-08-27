#include <WiFiClientSecure.h>
#include "registers.h"

//Найдите в Telegram бота @BotFather и создайте своего бота с помощью команды /newbot. Скопируйте выданный им Token (выглядит как 123456789:ABCdefGhIJKlmNoPQRsTUVwxyZ).
extern String BOT_TOKEN;// = "8480847939:AAEqcG1QaFfQHXPAeXD7fd77QVb53ny6ra0"; // ChickenHouse_Bobr_bot
//Найдите бота @myidbot (или @userinfobot) и отправьте ему команду /start. Он вернет ваш числовой Chat ID (например, 987654321).
extern String CHAT_ID;// = "1114180348";                             // Сюда ваш Chat ID

void sendTelegramMessage(String text);
void checkTelegram();