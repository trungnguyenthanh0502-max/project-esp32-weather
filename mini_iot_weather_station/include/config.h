#ifndef CONFIG_H
#define CONFIG_H

// ----------- Khai báo chân TFT ST7735 ---------------
#define TFT_CS    15
#define TFT_RST   4
#define TFT_DC    2
#define TFT_SDA   23
#define TFT_SCL   18

// ----------- Khai báo chân I2C (BMP085/BMP180) ---------------
#define I2C_SDA   21
#define I2C_SCL   22

// ----------- Khai báo chân Cảm biến DHT ---------------
#define DHTPIN    27
#define DHTTYPE   DHT11

// ----------- Cấu hình WiFi & MQTT Broker ---------------


#define MQTT_TOPIC_DATA "home/sensor/data"
#define MQTT_TOPIC_CMD  "iot/house/commands"

#endif