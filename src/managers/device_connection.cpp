/*
 * This file is part of the openinverter web interface
 *
 * Copyright (C) 2025 Nishanth Samala <contact@outlandnish.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "device_connection.h"
#include <Arduino.h>

DeviceConnection::DeviceConnection() {
    // Initialize arrays
    for (int i = 0; i < 4; i++) {
        serial_[i] = 0;
    }
    jsonFileName_[0] = '\0';
}

DeviceConnection& DeviceConnection::instance() {
    static DeviceConnection instance;
    return instance;
}

void DeviceConnection::setState(State newState) {
    state_ = newState;
    resetStateStartTime();
}

void DeviceConnection::setSerialPart(uint8_t index, uint32_t value) {
    if (index < 4) {
        serial_[index] = value;
    }
}

uint32_t DeviceConnection::getSerialPart(uint8_t index) const {
    return (index < 4) ? serial_[index] : 0;
}

void DeviceConnection::generateJsonFileName() {
    snprintf(jsonFileName_, sizeof(jsonFileName_), "/%" PRIx32 ".json", serial_[3]);
}

void DeviceConnection::resetStateStartTime() {
    stateStartTime_ = millis();
}

unsigned long DeviceConnection::getStateElapsedTime() const {
    return millis() - stateStartTime_;
}

bool DeviceConnection::hasStateTimedOut(unsigned long timeoutMs) const {
    return getStateElapsedTime() > timeoutMs;
}

void DeviceConnection::clearJsonCache() {
    cachedParamJson_.clear();
    jsonReceiveBuffer_ = "";
    jsonTotalSize_ = 0;
}

bool DeviceConnection::canSendParameterRequest() {
    unsigned long currentTime = micros();
    unsigned long timeSinceLastRequest = currentTime - lastParamRequestTime_;
    return timeSinceLastRequest >= minParamRequestIntervalUs_;
}

void DeviceConnection::markParameterRequestSent() {
    lastParamRequestTime_ = micros();
}
