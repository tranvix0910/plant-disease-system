#ifndef GEMINI_H
#define GEMINI_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include "secrets.h"
#include "promts.h"
#include "dht11.h"
#include "tele_bot.h"

extern const char* latitude;
extern const char* longitude;
extern const char* timezone;
extern char reason[10000];

extern String predictedDisease;

extern String scheduledWateringTime;
extern bool wateringScheduleActive;
extern bool alreadyWateredToday;

extern String lastAnalysisResults;
extern bool analysisInProgress;

void sendDetailedReportToGemini(JsonObject& summary);
void getWeatherAndAskGemini();
void sendErrorMessage(String errorType, String errorDetail, String diseaseName);
void getTreatmentFromGemini(String diseaseName);

#endif
