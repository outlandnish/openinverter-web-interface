#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "models/can_types.h"

// CAN processing task - runs independently on separate core
void canTask(void* parameter);

// TWAI driver initialization functions
bool initCanBusScanning(BaudRate baud, int txPin, int rxPin); // Initialize for scanning (accept all)
bool initCanBusForDevice(uint8_t nodeId, BaudRate baud, int txPin, int rxPin); // Initialize for specific device
