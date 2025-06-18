#ifndef GOOGLE_SHEETS_SERVICE_H
#define GOOGLE_SHEETS_SERVICE_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "dht11.h"
#include "soilmoisture.h"
#include "secrets.h"
#include "tele_bot.h"

void sendDataToGoogleSheets();
void testGoogleScriptConnection();

#endif


