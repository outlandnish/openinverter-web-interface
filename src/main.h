#pragma once

#include <ESPAsyncWebServer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "status_led.h"

#include "models/can_command.h"
#include "models/can_types.h"

// Forward declarations
class Config;

// ============================================================================
// Debug Output Configuration
// ============================================================================
#define DBG_OUTPUT_PORT Serial

// ============================================================================
// External Declarations for Globals
// ============================================================================

// FreeRTOS queue handles (defined in main.cpp)
extern QueueHandle_t canCommandQueue;
extern QueueHandle_t canEventQueue;

// Server instances (defined in main.cpp)
extern AsyncWebSocket ws;
extern Config config;

// ============================================================================
// Status LED Helpers
// ============================================================================

inline void setStatusLED(uint32_t color) {
  StatusLED::instance().setColor(color);
}

inline void statusLEDOff() {
  StatusLED::instance().off();
}

// ============================================================================
// CAN Command Queue Helper
// ============================================================================

/**
 * Queue a CAN command for processing by the CAN task.
 * @param cmd The command to queue
 * @param commandName Name for debug logging
 * @return true if queued successfully, false if queue was full
 */
inline bool queueCanCommand(const CANCommand& cmd, const char* commandName) {
  if (xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS)) == pdTRUE) {
    DBG_OUTPUT_PORT.printf("[WebSocket] %s command queued\n", commandName);
    return true;
  } else {
    DBG_OUTPUT_PORT.printf("[WebSocket] ERROR: Failed to queue %s command\n", commandName);
    return false;
  }
}
