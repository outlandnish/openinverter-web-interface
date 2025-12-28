#include "status_led.h"

// Status LED color definitions (using Adafruit_NeoPixel::Color)
const uint32_t StatusLED::OFF = Adafruit_NeoPixel::Color(0, 0, 0);
const uint32_t StatusLED::COMMAND = Adafruit_NeoPixel::Color(0, 0, 255);        // Blue - command processing
const uint32_t StatusLED::CAN_MAP = Adafruit_NeoPixel::Color(0, 255, 255);      // Cyan - CAN mapping
const uint32_t StatusLED::UPDATE = Adafruit_NeoPixel::Color(128, 0, 255);       // Purple - firmware update
const uint32_t StatusLED::WIFI_CONNECTING = Adafruit_NeoPixel::Color(255, 128, 0); // Orange - WiFi connecting
const uint32_t StatusLED::WIFI_CONNECTED = Adafruit_NeoPixel::Color(0, 255, 0);    // Green - WiFi connected
const uint32_t StatusLED::SUCCESS = Adafruit_NeoPixel::Color(0, 255, 0);        // Green - success
const uint32_t StatusLED::ERROR = Adafruit_NeoPixel::Color(255, 0, 0);          // Red - error

// Constructor
StatusLED::StatusLED(uint8_t pin, uint8_t count)
  : led(count, pin, NEO_GRB + NEO_KHZ800) {}

// Initialize the LED
void StatusLED::begin() {
  led.begin();
  off();
}

// Set LED to a specific color
void StatusLED::setColor(uint32_t color) {
  led.setPixelColor(0, color);
  led.show();
}

// Turn off the LED
void StatusLED::off() {
  setColor(OFF);
}

// Get singleton instance
StatusLED& StatusLED::instance() {
  static StatusLED led(STATUS_LED_PIN, STATUS_LED_COUNT);
  return led;
}
