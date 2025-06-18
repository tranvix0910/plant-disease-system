#ifndef SOILMOISTURE_H
#define SOILMOISTURE_H

#include <Arduino.h>

#define SOIL_MOISTURE_PIN 34
#define DRY_SOIL 3500
#define WET_SOIL 1500

void soilMoistureSetup();
int readSoilMoisture();
float readSoilMoisturePercent();

#endif

