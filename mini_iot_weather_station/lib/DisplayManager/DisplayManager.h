#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>

void initDisplayHardware();
void setupLCD();
void lcdSleep();
void lcdWake();
void drawMQTTStatus();
void renderSensorToLCD();
void updateTimeLCD();
void iconthoitiet(int x, int y, int code);
void printText(const char *text, uint16_t color, int x, int y, int textSize);

void TaskDisplay(void *pvParameters);

#endif