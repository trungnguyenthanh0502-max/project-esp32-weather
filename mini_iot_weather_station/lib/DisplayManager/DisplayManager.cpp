#include "DisplayManager.h"
#include "config.h"
#include "../../include/system_types.h"
#include "../../include/bitmaps.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <WiFi.h>

// Sử dụng TFT_SDA và TFT_SCL khớp chính xác với file config.h của bạn
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_SDA, TFT_SCL, TFT_RST);
bool lcdSleeping = false;

extern NTPClient timeClient;
extern PubSubClient client;
extern String weekDays[7];

static const int HEADER_LOGO_X   = 2;
static const int HEADER_LOGO_Y   = 1;
static const int HEADER_LOGO_W   = 32;
static const int HEADER_LOGO_H   = 32;
static const int HEADER_TEXT_X   = 38;

static const int MQTT_LABEL_X    = HEADER_TEXT_X;
static const int MQTT_LABEL_Y    = 22;
static const int MQTT_VALUE_X    = 76;
static const int MQTT_VALUE_Y    = 22;

static const int DIVIDER_Y       = 34;

static const int DATE_ROW_Y      = 37;
static const int TIME_ROW_Y      = 48;

static const int SENSOR_ICON_X   = 4;
static const int SENSOR_TEXT_X   = 28;
static const int SENSOR_ROW_H    = 21;      // 20px icon + 1px đệm
static const int SENSOR_ROW0_Y   = 65;      // nhiệt độ
static const int SENSOR_ROW1_Y   = SENSOR_ROW0_Y + SENSOR_ROW_H;   // độ ẩm
static const int SENSOR_ROW2_Y   = SENSOR_ROW1_Y + SENSOR_ROW_H;   // áp suất

void printText(const char *text, uint16_t color, int x, int y, int textSize) {
  tft.setCursor(x, y);
  tft.setTextColor(color);
  tft.setTextSize(textSize);
  tft.print(text);
}

void setupLCD() {
  tft.fillScreen(0x0000);

  // --- Header: logo + tên trạm + trạng thái MQTT ---
  tft.drawRGBBitmap(HEADER_LOGO_X, HEADER_LOGO_Y, logo_iuh, HEADER_LOGO_W, HEADER_LOGO_H);

  printText("WEATHER STATION", 0x07FF, HEADER_TEXT_X, 4, 1);
  printText("IUH", 0xFFE0, HEADER_TEXT_X, 13, 1);
  printText("MQTT:", 0x07E0, MQTT_LABEL_X, MQTT_LABEL_Y, 1);

  tft.drawFastHLine(0, DIVIDER_Y, 128, 0x39C7);

  // --- 3 hàng cảm biến: icon bên trái, giá trị bên phải ---
  tft.drawBitmap(SENSOR_ICON_X, SENSOR_ROW0_Y, temperature, 20, 20, 0x07FF);
  tft.drawBitmap(SENSOR_ICON_X, SENSOR_ROW1_Y, humidity, 20, 20, 0x07FF);
  tft.drawBitmap(SENSOR_ICON_X, SENSOR_ROW2_Y, pressure, 20, 20, 0x07FF);
}

void initDisplayHardware() {
  Serial.println("[Display] Bat dau initDisplayHardware()...");

  tft.initR(INITR_144GREENTAB);
  Serial.println("[Display] tft.initR() da goi xong.");

  tft.setSPISpeed(27000000);
  Serial.println("[Display] tft.setSPISpeed() da goi xong.");

  if (xSemaphoreTake(tftMutex, portMAX_DELAY)) {
    Serial.println("[Display] Da lay duoc tftMutex, chuan bi goi setupLCD()...");
    setupLCD();
    Serial.println("[Display] setupLCD() da chay xong.");
    xSemaphoreGive(tftMutex);
  } else {
    Serial.println("[Display] LOI: khong lay duoc tftMutex!");
  }

  Serial.println("[Display] initDisplayHardware() HOAN TAT.");
}

void lcdSleep() {
  if (lcdSleeping) return;

  if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(200))) {
    lcdSleeping = true;
    tft.fillScreen(0x0000);
    tft.writeCommand(ST77XX_DISPOFF);
    tft.writeCommand(ST77XX_SLPIN);
    xSemaphoreGive(tftMutex);
  } else {
    Serial.println("lcdSleep: khong lay duoc tftMutex, bo qua lan nay");
  }
}

void lcdWake() {
  if (!lcdSleeping) return;

  if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(200))) {
    lcdSleeping = false;
    tft.writeCommand(ST77XX_SLPOUT);
    tft.writeCommand(ST77XX_DISPON);
    setupLCD();
    xSemaphoreGive(tftMutex);
  } else {
    Serial.println("lcdWake: khong lay duoc tftMutex, bo qua lan nay");
  }
}

void drawMQTTStatus() {
  if (lcdSleeping) return;
  const int x = MQTT_VALUE_X, y = MQTT_VALUE_Y;
  tft.fillRect(x, y, 40, 10, 0x0000);
  tft.setCursor(x, y);
  tft.setTextSize(1);
  if (client.connected()) {
    tft.setTextColor(0x07E0, 0x0000);
    tft.print("OK");
  } else {
    tft.setTextColor(0xF800, 0x0000);
    tft.print("LOST");
  }
}

void renderSensorToLCD() {
  if (lcdSleeping) return;
  float t = 0, h = 0, p = 0;
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20))) {
    t = globalSensorData.temp;
    h = globalSensorData.hum;
    p = globalSensorData.press;
    xSemaphoreGive(dataMutex);
  }

  char buf[16];

  // Nhiệt độ - độ rộng 5 ký tự, vd " 25.3" hoặc "-10.0"
  snprintf(buf, sizeof(buf), "%5.1f", t);
  tft.setCursor(SENSOR_TEXT_X, SENSOR_ROW0_Y + 6);
  tft.setTextColor(0xFFE0, 0x0000);
  tft.setTextSize(1);
  tft.print(buf);
  tft.drawBitmap(SENSOR_TEXT_X + 27, SENSOR_ROW0_Y, icondo, 10, 20, 0xFFE0);
  tft.setCursor(SENSOR_TEXT_X + 39, SENSOR_ROW0_Y + 6);
  tft.setTextColor(0xFFE0, 0x0000);
  tft.print("C");

  // Độ ẩm - độ rộng 5 ký tự + "%"
  snprintf(buf, sizeof(buf), "%5.1f%%", h);
  tft.setCursor(SENSOR_TEXT_X, SENSOR_ROW1_Y + 6);
  tft.setTextColor(0x07E0, 0x0000);
  tft.setTextSize(1);
  tft.print(buf);

  // Áp suất - độ rộng 4 ký tự + "hPa"
  snprintf(buf, sizeof(buf), "%4dhPa", (int)p);
  tft.setCursor(SENSOR_TEXT_X, SENSOR_ROW2_Y + 6);
  tft.setTextColor(0x07E0, 0x0000);
  tft.setTextSize(1);
  tft.print(buf);
}

void updateTimeLCD() {
  if (lcdSleeping) return;
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime((time_t *)&epochTime);
  int year = ptm->tm_year + 1900;

  static bool ntpWasReady = false;
  static int lastDay = -1;

  if (year < 2024) {
    Serial.printf("[Display] NTP chua san sang - epoch=%lu, nam tinh duoc=%d, WiFi=%s\n",
                  epochTime, year, (WiFi.status() == WL_CONNECTED) ? "OK" : "MAT KET NOI");
    if (ntpWasReady) {
      tft.fillRect(0, DATE_ROW_Y, 128, 8 + 16, 0x0000);
      ntpWasReady = false;
      lastDay = -1;
    }
    tft.setCursor(4, DATE_ROW_Y);
    tft.setTextColor(0xF800, 0x0000);
    tft.setTextSize(1);
    tft.print("Dang dong bo gio...");
    return;
  }

  if (!ntpWasReady) {
    // Lần đầu NTP vừa sẵn sàng: xóa thông báo "Dang dong bo..." một lần duy nhất.
    tft.fillRect(0, DATE_ROW_Y, 128, 8, 0x0000);
    ntpWasReady = true;
    lastDay = -1;  // ép vẽ lại thứ/ngày ngay bên dưới
  }

  int today = timeClient.getDay();
  if (today != lastDay) {
    lastDay = today;
    tft.fillRect(0, DATE_ROW_Y, 128, 8, 0x0000);  // chỉ xóa khi sang ngày mới
    tft.setCursor(4, DATE_ROW_Y);
    tft.setTextColor(0xFFE0, 0x0000);
    tft.setTextSize(1);
    tft.print(weekDays[today]);

    tft.setCursor(62, DATE_ROW_Y);
    tft.setTextColor(0x07FF, 0x0000);
    tft.setTextSize(1);
    tft.printf("%02d-%02d-%04d", ptm->tm_mday, ptm->tm_mon + 1, year);
  }

  // Giờ: đè trực tiếp lên số cũ mỗi giây, không xóa trước -> không chớp.
  tft.setCursor(10, TIME_ROW_Y);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setTextSize(2);
  tft.printf("%02d:%02d:%02d", timeClient.getHours(), timeClient.getMinutes(), timeClient.getSeconds());
}

void iconthoitiet(int x, int y, int code) {
  int gio = timeClient.getHours();
  bool isDay = (gio >= 6 && gio < 18);
  if (code >= 200 && code <= 232) {
    tft.drawBitmap(x, y, (code <= 212) ? lighting_rain : lighting, 50, 40, 0xFFFF);
  } else if ((code >= 300 && code <= 321) || (code >= 500 && code <= 531) || (code >= 600 && code <= 622)) {
    tft.drawBitmap(x, y, rain, 50, 40, 0xFFFF);
  } else if (code == 800) {
    tft.drawBitmap(x, y, isDay ? clear_sky : night, 50, 40, 0xFFFF);
  } else if (code >= 801 && code <= 803) {
    tft.drawBitmap(x, y, isDay ? clear_cloudy : night_cloud, 50, 40, 0xFFFF);
  } else if (code == 804 || (code >= 701 && code <= 781)) {
    tft.drawBitmap(x, y, cloudy, 50, 40, 0xFFFF);
  }
}

void TaskDisplay(void *pvParameters) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      timeClient.update();
    }
    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(50))) {
      updateTimeLCD();
      renderSensorToLCD();
      drawMQTTStatus();
      xSemaphoreGive(tftMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}