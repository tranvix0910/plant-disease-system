#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>
#include "index.h"
#include "dht11.h"
#include "soilmoisture.h"
#include "water_pump.h"
#include "config.h"
#include "reports.h"

WebServer server(80);

extern float latestTemperature;
extern float latestHumidity;
extern float latestSoilMoisturePercent;

extern String lastReportResults;
extern bool reportInProgress;
extern unsigned long reportStartTime;

extern String lastAnalysisResults;
extern bool analysisInProgress;
extern unsigned long analysisStartTime;

extern const char* ssid;
extern const char* password;

extern unsigned long lastDiseaseUpdateTime;

void webServerSetup();
void handleRoot();
void handleUpdate();
void handleUpdateToSheets();
void handleDailyReport();
void handleReportResults();
void handleDetailedAnalysis();
void handleAnalysisResults();

void handleStatus();
void handlePredict();
void handleStartWaterPump();
void handleSetScheduledWateringTime();
void handleReceiveDisease();

#endif