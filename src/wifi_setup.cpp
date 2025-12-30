#include "wifi_setup.h"

#include <LittleFS.h>

#include "status_led.h"

#define DBG_OUTPUT_PORT Serial

bool WiFiSetup::loadCredentials(Credentials& creds) {
  if (!LittleFS.exists("/wifi.txt")) {
    DBG_OUTPUT_PORT.println("wifi.txt not found in LittleFS");
    return false;
  }

  File file = LittleFS.open("/wifi.txt", "r");
  if (!file) {
    DBG_OUTPUT_PORT.println("Failed to open wifi.txt");
    return false;
  }

  // Read SSID (first line)
  creds.ssid = file.readStringUntil('\n');
  creds.ssid.trim();

  // Read Password (second line)
  creds.password = file.readStringUntil('\n');
  creds.password.trim();

  file.close();

  if (creds.ssid.length() == 0) {
    DBG_OUTPUT_PORT.println("SSID is empty in wifi.txt");
    return false;
  }

  DBG_OUTPUT_PORT.println("WiFi credentials loaded from wifi.txt");
  DBG_OUTPUT_PORT.print("SSID: ");
  DBG_OUTPUT_PORT.println(creds.ssid);

  return true;
}

bool WiFiSetup::connectStation(const Credentials& creds, int maxAttempts) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.begin(creds.ssid.c_str(), creds.password.c_str());

  DBG_OUTPUT_PORT.print("Connecting to WiFi");
  StatusLED::instance().setColor(StatusLED::WIFI_CONNECTING);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    DBG_OUTPUT_PORT.print(".");
    attempts++;
  }
  DBG_OUTPUT_PORT.println();

  if (WiFi.status() == WL_CONNECTED) {
    DBG_OUTPUT_PORT.println("WiFi connected!");
    DBG_OUTPUT_PORT.print("IP address: ");
    DBG_OUTPUT_PORT.println(WiFi.localIP());
    StatusLED::instance().setColor(StatusLED::WIFI_CONNECTED);
    delay(1000);  // Show connected status for 1 second
    StatusLED::instance().off();
    return true;
  } else {
    DBG_OUTPUT_PORT.println("WiFi connection failed!");
    StatusLED::instance().setColor(StatusLED::ERROR);
    delay(1000);  // Show error for 1 second
    StatusLED::instance().off();
    return false;
  }
}

void WiFiSetup::startAccessPoint() {
  DBG_OUTPUT_PORT.println("Starting in AP mode");

  // Generate AP name using MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String apSSID = "ESP-" + String(mac[4], HEX) + String(mac[5], HEX);
  apSSID.toUpperCase();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID.c_str());

  // Set AP IP to 192.168.4.1
  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);

  DBG_OUTPUT_PORT.print("AP Name: ");
  DBG_OUTPUT_PORT.println(apSSID);
  DBG_OUTPUT_PORT.print("AP IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.softAPIP());

  StatusLED::instance().setColor(StatusLED::WIFI_CONNECTED);
  delay(1000);
  StatusLED::instance().off();
}

bool WiFiSetup::initialize() {
  Credentials creds;
  if (loadCredentials(creds)) {
    if (connectStation(creds)) {
      return true;
    }
  }

  // If STA mode failed or no credentials, start AP mode
  startAccessPoint();
  return false;
}
