#ifndef DHT11_H
#define DHT11_H

#include <DHT.h>

#define DHTPIN 18
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

void dht11Setup();
float readTemperature();
float readHumidity();

#endif

