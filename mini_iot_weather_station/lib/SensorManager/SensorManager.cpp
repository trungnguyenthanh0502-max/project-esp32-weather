#include "SensorManager.h"
#include "config.h"
#include "system_types.h"
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <ArduinoJson.h>
#include <NTPClient.h>

DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);
extern NTPClient timeClient;

// FIX: cờ báo cảm biến áp suất có sẵn sàng hay không, thay vì while(1) chặn cứng
static bool bmpAvailable = false;

void initSensorsHardware() {
  Wire.begin(I2C_SDA, I2C_SCL);
  dht.begin();

  if (!bmp.begin()) {
    Serial.println("Loi cam bien BMP085/BMP180! Tiep tuc chay, ap suat se khong kha dung.");
    bmpAvailable = false;
  } else {
    bmpAvailable = true;
  }
}

void TaskSensors(void *pvParameters) {
  for (;;) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    float p = 0.0f;
    if (bmpAvailable) {
      sensors_event_t event;
      bmp.getEvent(&event);
      p = event.pressure ? event.pressure : 0.0f;
    }

    if (!isnan(h) && !isnan(t)) {
      if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100))) {
        globalSensorData.temp = t;
        globalSensorData.hum = h;
        globalSensorData.press = p;
        xSemaphoreGive(dataMutex);
      }

      MqttMessage msg;
      strncpy(msg.topic, MQTT_TOPIC_DATA, sizeof(msg.topic));
      msg.topic[sizeof(msg.topic) - 1] = '\0'; // FIX: đảm bảo null-terminate

      StaticJsonDocument<256> doc;
      doc["temperature"] = t;
      doc["humidity"] = h;
      doc["pressure"] = p;
      doc["pressure_valid"] = bmpAvailable; // FIX: cho phía nhận biết áp suất có tin cậy không
      if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50))) {
        doc["weather_code"] = globalSensorData.weatherCode;
        xSemaphoreGive(dataMutex);
      }
      doc["timestamp"] = timeClient.getEpochTime();
      serializeJson(doc, msg.payload, sizeof(msg.payload));

      xQueueSend(mqttQueue, &msg, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}