#pragma once

#include "models/can_types.h"

#define EEPROM_VERSION 4

struct EEPROMSettings {
  int version;
  int canRXPin;
  int canTXPin;
  int canEnablePin;
  int canSpeed;
  int scanStartNode;
  int scanEndNode;
};

class Config {
public:
  Config();
  void load();

  int getCanRXPin();
  void setCanRXPin(int pin);

  int getCanTXPin();
  void setCanTXPin(int pin);

  int getCanEnablePin();
  void setCanEnablePin(int pin);

  int getCanSpeed();
  void setCanSpeed(int speed);

  // Convert stored canSpeed integer to BaudRate enum
  BaudRate getBaudRateEnum() const;

  int getScanStartNode();
  void setScanStartNode(int node);

  int getScanEndNode();
  void setScanEndNode(int node);

  void saveSettings();

private:
  EEPROMSettings settings;
};