#pragma once

#include "freertos/queue.h"
#include "models/can_command.h"
#include "models/can_types.h"
#include <Adafruit_NeoPixel.h>

// External declarations for globals defined in main.cpp
extern QueueHandle_t canCommandQueue;
extern Adafruit_NeoPixel statusLED;

// Status LED color definitions (uint32_t for NeoPixel)
extern const uint32_t LED_OFF;
extern const uint32_t LED_COMMAND;
extern const uint32_t LED_CAN_MAP;
extern const uint32_t LED_UPDATE;
extern const uint32_t LED_WIFI_CONNECTING;
extern const uint32_t LED_WIFI_CONNECTED;
extern const uint32_t LED_SUCCESS;
extern const uint32_t LED_ERROR;

// DBG_OUTPUT_PORT is defined as Serial in main.cpp
#define DBG_OUTPUT_PORT Serial

// Helper function to set status LED color
inline void setStatusLED(uint32_t color) {
  statusLED.setPixelColor(0, color);
  statusLED.show();
}

// Helper function to turn off status LED
inline void statusLEDOff() {
  setStatusLED(LED_OFF);
}

// Helper function to queue CAN commands
inline bool queueCanCommand(const CANCommand& cmd, const char* commandName) {
  if (xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS)) == pdTRUE) {
    DBG_OUTPUT_PORT.printf("[WebSocket] %s command queued\n", commandName);
    return true;
  } else {
    DBG_OUTPUT_PORT.printf("[WebSocket] ERROR: Failed to queue %s command\n", commandName);
    return false;
  }
}
