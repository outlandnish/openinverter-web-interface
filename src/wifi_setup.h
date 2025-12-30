#pragma once
#include <Arduino.h>
#include <WiFi.h>

class WiFiSetup {
public:
  struct Credentials {
    String ssid;
    String password;
  };

  // Load WiFi credentials from LittleFS
  static bool loadCredentials(Credentials& creds);

  // Connect to WiFi in station mode
  static bool connectStation(const Credentials& creds, int maxAttempts = 20);

  // Start WiFi in access point mode
  static void startAccessPoint();

  // Initialize WiFi (tries station mode, falls back to AP)
  static bool initialize();
};
