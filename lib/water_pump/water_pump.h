#ifndef WATER_PUMP_H
#define WATER_PUMP_H

#include <Arduino.h>
#include <tele_bot.h>

#define WATER_PUMP_PIN 5
#define PUMP_DURATION 10000
#define SOIL_THRESHOLD 2800

extern unsigned long wateringStartTime;

void waterPumpSetup();
int waterPumpRead();
bool checkAndWater();
void startWaterPump();
void checkAndStopPump();

#endif

