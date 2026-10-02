#include <Arduino.h>
#include "config.h"
#include "system_types.h"
#include "DisplayManager.h"
#include "SensorManager.h"
#include "NetworkManager.h"
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "secrets.h"
// Định nghĩa dữ liệu toàn cục
SensorData globalSensorData = {0.0f, 0.0f, 0.0f, 0};
SemaphoreHandle_t dataMutex = NULL;
SemaphoreHandle_t tftMutex = NULL;
QueueHandle_t mqttQueue = NULL;
TaskHandle_t taskWeatherHandle = NULL;



WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200);
String weekDays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

extern PubSubClient client;

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Khoi chay ESP32 System Modularized (FreeRTOS) ===");

  // 1. Tạo Mutex & Queue
  tftMutex = xSemaphoreCreateMutex();
  dataMutex = xSemaphoreCreateMutex();
  mqttQueue = xQueueCreate(10, sizeof(MqttMessage));

  if (tftMutex == NULL || dataMutex == NULL || mqttQueue == NULL) {
    Serial.println("LOI NGHIEM TRONG: khong the tao mutex/queue (het RAM?). He thong dung lai.");
    while (1) {
      delay(1000);
    }
  }

  // 2. Khởi tạo phần cứng
  initDisplayHardware();
  initSensorsHardware();

  // 3. Kết nối mạng
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setBufferSize(512);
  client.setCallback(callback);
  timeClient.begin();

  // 4. Khởi tạo các Task RTOS
  BaseType_t ok;

  ok = xTaskCreatePinnedToCore(TaskMQTT, "TaskMQTT", 4096, NULL, 2, NULL, 0);
  if (ok != pdPASS) Serial.println("LOI: khong tao duoc TaskMQTT");

  ok = xTaskCreatePinnedToCore(TaskSensors, "TaskSensors", 3072, NULL, 1, NULL, 1);
  if (ok != pdPASS) Serial.println("LOI: khong tao duoc TaskSensors");

  ok = xTaskCreatePinnedToCore(TaskDisplay, "TaskDisplay", 4096, NULL, 1, NULL, 1);
  if (ok != pdPASS) Serial.println("LOI: khong tao duoc TaskDisplay");

  ok = xTaskCreatePinnedToCore(TaskWeatherAPI, "TaskWeatherAPI", 6144, NULL, 1, &taskWeatherHandle, 0);
  if (ok != pdPASS) Serial.println("LOI: khong tao duoc TaskWeatherAPI");
}

void loop() {
  vTaskDelete(NULL); // Xóa task loop để giải phóng RAM
}