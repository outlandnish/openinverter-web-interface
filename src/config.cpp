#include "config.h"
#include <EEPROM.h>
Config::Config() {
}

void Config::load() {

    EEPROM.begin(sizeof(settings));
    EEPROM.get(0, settings);
    if (settings.version != EEPROM_VERSION) {
        //defaults
        settings.version = EEPROM_VERSION;
        #ifdef CAN0_RX_PIN
        settings.canRXPin = CAN0_RX_PIN; // CAN0 RX pin for canipulator (GPIO 16)
        #else
        settings.canRXPin = GPIO_NUM_4;
        #endif
        #ifdef CAN0_TX_PIN
        settings.canTXPin = CAN0_TX_PIN; // CAN0 TX pin for canipulator (GPIO 17)
        #else
        settings.canTXPin = GPIO_NUM_5;
        #endif
        settings.canEnablePin = 0;
        settings.canSpeed = 2; // Default to 500k (Baud500k = 2)
        settings.scanStartNode = 1;
        settings.scanEndNode = 32;
    }
}
int Config::getCanRXPin() {
    return settings.canRXPin;
}

int Config::getCanTXPin() {
    return settings.canTXPin;
}

int Config::getCanEnablePin() {
    return settings.canEnablePin;
}


void Config::setCanEnablePin(int pin) {
    settings.canEnablePin = pin;
}

void Config::setCanTXPin(int pin) {
    settings.canTXPin = pin;
}

void Config::setCanRXPin(int pin) {
    settings.canRXPin = pin;
}

int Config::getCanSpeed() {
    return settings.canSpeed;
}

void Config::setCanSpeed(int speed) {
    settings.canSpeed = speed;
}

BaudRate Config::getBaudRateEnum() const {
    switch (settings.canSpeed) {
        case 0: return Baud125k;
        case 1: return Baud250k;
        case 2:
        default: return Baud500k;
    }
}

int Config::getScanStartNode() {
    return settings.scanStartNode;
}

void Config::setScanStartNode(int node) {
    settings.scanStartNode = node;
}

int Config::getScanEndNode() {
    return settings.scanEndNode;
}

void Config::setScanEndNode(int node) {
    settings.scanEndNode = node;
}

void Config::saveSettings() {
    EEPROM.put(0, settings); //save all change to eeprom
    EEPROM.commit();
}
