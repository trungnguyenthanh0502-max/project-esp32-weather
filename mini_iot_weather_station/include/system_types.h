#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// Cấu trúc dữ liệu cảm biến
struct SensorData {
  float temp;
  float hum;
  float press;
  int weatherCode;
};

// Cấu trúc tin nhắn MQTT Queue
struct MqttMessage {
  char topic[64];
  char payload[256];
};

// Khai báo extern để các module dùng chung biến Global
extern SensorData globalSensorData;
extern SemaphoreHandle_t dataMutex;
extern SemaphoreHandle_t tftMutex;
extern QueueHandle_t mqttQueue;
extern TaskHandle_t taskWeatherHandle;

extern String apiKey;
extern String city;

#endif