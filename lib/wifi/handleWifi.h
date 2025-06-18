#ifndef WIFI_H
#define WIFI_H

#include <WiFi.h>
#include <WiFiClientSecure.h>

extern const char* ssid;
extern const char* password;

void connectToWifi();

#endif