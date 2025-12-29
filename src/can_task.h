#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/twai.h"
#include "models/can_types.h"

// Queue sizes
#define CAN_TX_QUEUE_SIZE 20
#define SDO_RESPONSE_QUEUE_SIZE 10

// CAN I/O queues (created in initCanQueues)
extern QueueHandle_t canTxQueue;         // Raw CAN frames to transmit
extern QueueHandle_t sdoResponseQueue;   // SDO responses for oi_can/SDO protocol

// Initialize CAN queues (call before starting canTask)
void initCanQueues();

// CAN processing task - runs independently on separate core
void canTask(void* parameter);

// TWAI driver initialization functions
bool initCanBusScanning(BaudRate baud, int txPin, int rxPin); // Initialize for scanning (accept all)
bool initCanBusForDevice(uint8_t nodeId, BaudRate baud, int txPin, int rxPin); // Initialize for specific device

// Flush pending TX frames immediately (for use when immediate transmission is needed)
void flushCanTxQueue();
