#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>

void initSensorsHardware();
void TaskSensors(void *pvParameters);

#endif