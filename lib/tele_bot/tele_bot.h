#ifndef TELE_BOT_H
#define TELE_BOT_H

#include <Arduino.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>

#include "secrets.h"    
#include "dht11.h"
#include "soilmoisture.h"
#include "gemini.h"
#include "google_sheets_service.h"
#include "config.h"
#include "secrets.h"
#include "reports.h"
#include "water_pump.h"

extern WiFiClientSecure client;
extern UniversalTelegramBot bot;

extern int botRequestDelay;
extern unsigned long lastTimeBotRan;
extern const long messageInterval;
extern unsigned long lastMessageTime;

extern bool wateringScheduleActive;
extern bool alreadyWateredToday;
extern String scheduledWateringTime;

extern int dailyReportHour;
extern int dailyReportMinute;
extern bool reportSentToday;

void telegramBotSetup();
void messageCheck();
void sendToAllChannels(const String& message, const String& format = "Markdown");

#endif

