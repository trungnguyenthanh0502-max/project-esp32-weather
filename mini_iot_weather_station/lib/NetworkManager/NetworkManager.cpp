#include "NetworkManager.h"
#include "config.h"
#include "system_types.h"
#include "DisplayManager.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Adafruit_ST7735.h>

extern String apiKey;
extern String city;
extern TaskHandle_t taskWeatherHandle; // FIX: khai báo extern rõ ràng, tránh lỗi biên dịch/UB nếu chưa có trong header dùng chung

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi...");
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi khong ket noi duoc sau timeout, se thu lai trong TaskMQTT.");
  }
}

void reconnectMQTT() {
  static unsigned long lastAttempt = 0;
  if (client.connected()) return;

  unsigned long now = millis();
  if (now - lastAttempt > 10000) {
    lastAttempt = now;
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
      client.subscribe(MQTT_TOPIC_CMD);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  char message[256];
  if (length >= sizeof(message)) length = sizeof(message) - 1;
  memcpy(message, payload, length);
  message[length] = '\0';

  if (String(topic) == MQTT_TOPIC_CMD) {
    MqttMessage outMsg;
    strncpy(outMsg.topic, "iot/house/status", sizeof(outMsg.topic));
    outMsg.topic[sizeof(outMsg.topic) - 1] = '\0'; // FIX: null-terminate tường minh

    if (strcmp(message, "OPEN") == 0) {
      lcdWake();
      strncpy(outMsg.payload, "LCD OPENED", sizeof(outMsg.payload));
      outMsg.payload[sizeof(outMsg.payload) - 1] = '\0'; // FIX
      xQueueSend(mqttQueue, &outMsg, 0);
      return;
    }
    if (strcmp(message, "SLEEP") == 0) {
      lcdSleep();
      strncpy(outMsg.payload, "LCD SLEPT", sizeof(outMsg.payload));
      outMsg.payload[sizeof(outMsg.payload) - 1] = '\0'; // FIX
      xQueueSend(mqttQueue, &outMsg, 0);
      return;
    }

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, message) == DeserializationError::Ok) {
      if (doc["cmd"] == "change_city") {
        // FIX: bảo vệ ghi biến toàn cục `city` bằng mutex, vì String không thread-safe
        // và `city` được đọc từ TaskWeatherAPI (getWeather) đồng thời.
        String newCity = doc["city"].as<String>();
        if (newCity.length() > 0 && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100))) {
          city = newCity;
          xSemaphoreGive(dataMutex);

          snprintf(outMsg.payload, sizeof(outMsg.payload), "CITY_CHANGED:%s", city.c_str());
          xQueueSend(mqttQueue, &outMsg, 0);
          if (taskWeatherHandle != NULL) {
            xTaskNotifyGive(taskWeatherHandle);
          }
        }
      }
    } else {
      Serial.println("MQTT payload JSON khong hop le"); // FIX: log lỗi parse thay vì im lặng bỏ qua
    }
  }
}

void getWeather() {
  // FIX: đọc `city` có bảo vệ mutex để tránh race condition với callback() ở TaskMQTT
  String cityLocal;
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100))) {
    cityLocal = city;
    xSemaphoreGive(dataMutex);
  } else {
    return; // không lấy được city an toàn, bỏ qua lần cập nhật này
  }

  String cityEncoded = cityLocal;
  cityEncoded.replace(" ", "%20");
  WiFiClientSecure clientSecure;
  clientSecure.setInsecure();

  HTTPClient http;
  http.setTimeout(8000); // FIX: tránh treo lâu nếu API phản hồi chậm/không phản hồi
  String url = "https://api.openweathermap.org/data/2.5/weather?q=" + cityEncoded + "&appid=" + apiKey + "&units=metric";

  if (http.begin(clientSecure, url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      DynamicJsonDocument doc(2048);
      if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
        int code = doc["weather"][0]["id"].as<int>();
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50))) {
          globalSensorData.weatherCode = code;
          xSemaphoreGive(dataMutex);
        }
        if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(200))) { // FIX: timeout thay vì portMAX_DELAY (an toàn hơn nếu tftMutex bận)
          extern Adafruit_ST7735 tft;
          tft.fillRect(72, 66, 50, 40, 0x0000);
          iconthoitiet(72, 66, code);
          xSemaphoreGive(tftMutex);
        }
      } else {
        Serial.println("Loi parse JSON tu OpenWeatherMap");
      }
    } else {
      Serial.printf("Loi HTTP GET weather: %d\n", httpCode);
    }
    http.end();
  }
}

void TaskMQTT(void *pvParameters) {
  MqttMessage msg;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      vTaskDelay(pdMS_TO_TICKS(5000));
    } else {
      reconnectMQTT();
      client.loop();
      while (xQueueReceive(mqttQueue, &msg, 0) == pdTRUE) {
        if (client.connected()) {
          client.publish(msg.topic, msg.payload);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void TaskWeatherAPI(void *pvParameters) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      getWeather();
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1800000UL));
  }
}