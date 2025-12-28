#pragma once

#include "freertos/queue.h"
#include "models/can_command.h"
#include "models/can_types.h"
#include "status_led.h"
#include <ESPAsyncWebServer.h>

// Forward declarations
class Config;

// External declarations for globals defined in main.cpp
extern QueueHandle_t canCommandQueue;
extern AsyncWebSocket ws;
extern Config config;
extern int totalUpdatePages;

// DBG_OUTPUT_PORT is defined as Serial in main.cpp
#define DBG_OUTPUT_PORT Serial

// Helper function to set status LED color
inline void setStatusLED(uint32_t color) {
  StatusLED::instance().setColor(color);
}

// Helper function to turn off status LED
inline void statusLEDOff() {
  StatusLED::instance().off();
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
