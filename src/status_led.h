#pragma once
#include <Adafruit_NeoPixel.h>

// WS2812B_PIN and WS2812B_COUNT are defined in platformio.ini for each board
#ifndef WS2812B_PIN
  #define WS2812B_PIN 8  // Fallback default
#endif
#ifndef WS2812B_COUNT
  #define WS2812B_COUNT 1  // Fallback default
#endif
#define STATUS_LED_PIN WS2812B_PIN
#define STATUS_LED_COUNT WS2812B_COUNT

// Status LED manager class
class StatusLED {
private:
  Adafruit_NeoPixel led;

  StatusLED(uint8_t pin, uint8_t count);

public:
  // LED colors
  static const uint32_t OFF;
  static const uint32_t COMMAND;
  static const uint32_t CAN_MAP;
  static const uint32_t UPDATE;
  static const uint32_t WIFI_CONNECTING;
  static const uint32_t WIFI_CONNECTED;
  static const uint32_t SUCCESS;
  static const uint32_t ERROR;

  // Get singleton instance
  static StatusLED& instance();

  void begin();
  void setColor(uint32_t color);
  void off();
  Adafruit_NeoPixel& getLED() { return led; }
};
