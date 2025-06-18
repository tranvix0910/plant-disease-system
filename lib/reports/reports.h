#ifndef REPORTS_H
#define REPORTS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "tele_bot.h"
extern DynamicJsonDocument lastDailyReport;

extern int dailyReportHour;
extern int dailyReportMinute;
extern bool reportSentToday;

extern String lastReportResults;
extern bool reportInProgress;
extern unsigned long reportStartTime;

extern String lastAnalysisResults;
extern bool analysisInProgress;
extern unsigned long analysisStartTime;

void saveDailyReport(JsonObject summary);
bool isTimeToSendDailyReport();
void sendDailyReport();
void requestDetailedAnalysis();
#endif
