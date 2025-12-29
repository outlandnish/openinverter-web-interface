#pragma once

#include <Arduino.h>

/**
 * CAN hardware initialization utilities.
 */
namespace CanHardware {

/**
 * Initialize a CAN transceiver control pin (shutdown or standby).
 * Sets the pin as OUTPUT and drives it LOW to enable the transceiver.
 * @param pin The GPIO pin number
 * @param pinName Human-readable name for debug output (e.g., "CAN0 shutdown")
 */
inline void initTransceiverPin(int pin, const char* pinName) {
    if (pin >= 0) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        Serial.printf("%s pin %d set to LOW\n", pinName, pin);
    }
}

/**
 * Initialize all CAN transceiver pins for the current platform.
 * Uses compile-time defines for platform-specific pin assignments.
 */
void initAllTransceiverPins();

} // namespace CanHardware
