// File: secrets_example.h
// HƯỚNG DẪN: Người dùng khi tải code về hãy đổi tên file này thành "secrets.h" 
// và điền các thông tin WiFi, MQTT, API Key thực tế vào đây.

#ifndef SECRETS_H
#define SECRETS_H

#include 

// ----------- Cấu hình WiFi & MQTT Broker -------------
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASS       "YOUR_WIFI_PASSWORD"

#define MQTT_SERVER     "192.168.x.x"   // Nhập địa chỉ IP của MQTT Broker
#define MQTT_PORT       1883
#define MQTT_USER       "YOUR_MQTT_USER" // Để trống "" nếu không có
#define MQTT_PASS       "YOUR_MQTT_PASS" // Để trống "" nếu không có
#define MQTT_CLIENT_ID  "ESP32_CLIENT"

#define MQTT_TOPIC_DATA "home/sensor/data"
#define MQTT_TOPIC_CMD  "iot/house/commands"

// ----------- Cấu hình OpenWeatherMap API -------------
String apiKey = "YOUR_API_KEY_HERE";
String city = "YOUR_CITY_NAME";

#endif