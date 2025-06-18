#include "soilmoisture.h"

void soilMoistureSetup() {
    pinMode(SOIL_MOISTURE_PIN, INPUT);
}

int readSoilMoisture() {
    return analogRead(SOIL_MOISTURE_PIN);
}

float readSoilMoisturePercent() {
    int soilMoistureValue = readSoilMoisture();
    float soilMoisturePercent = map(soilMoistureValue, DRY_SOIL, WET_SOIL, 0, 100);
    soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
    return soilMoisturePercent;
}

