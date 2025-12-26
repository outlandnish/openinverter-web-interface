#ifndef MAIN_H
#define MAIN_H

#include "freertos/queue.h"
#include "models/can_command.h"
#include "models/can_types.h"
#include <SmartLeds.h>

// External declarations for globals defined in main.cpp
extern QueueHandle_t canCommandQueue;
extern SmartLed statusLED;

// Status LED color definitions
extern const Rgb LED_OFF;
extern const Rgb LED_COMMAND;
extern const Rgb LED_CAN_MAP;
extern const Rgb LED_UPDATE;
extern const Rgb LED_WIFI_CONNECTING;
extern const Rgb LED_WIFI_CONNECTED;
extern const Rgb LED_SUCCESS;
extern const Rgb LED_ERROR;

// DBG_OUTPUT_PORT is defined as Serial in main.cpp
#define DBG_OUTPUT_PORT Serial

// Helper function to set status LED color
inline void setStatusLED(Rgb color) {
  statusLED[0] = color;
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

#endif // MAIN_H
