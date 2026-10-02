#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>

void setup_wifi();
void reconnectMQTT();
void callback(char* topic, byte* payload, unsigned int length);
void getWeather();

void TaskMQTT(void *pvParameters);
void TaskWeatherAPI(void *pvParameters);

#endif