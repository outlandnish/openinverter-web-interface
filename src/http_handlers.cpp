#include "http_handlers.h"
#include "oi_can.h"
#include "managers/device_connection.h"
#include "managers/device_discovery.h"
#include "main.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

// External references to globals from main.cpp
extern AsyncWebSocket ws;
extern Config config;

// Helper function to format byte sizes
String formatBytes(uint64_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

// Helper function to get content type from filename
String getContentType(String filename, AsyncWebServerRequest *request) {
  if (request && request->hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".bin")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

// Handle file requests from LittleFS
void handleFileRequest(AsyncWebServerRequest *request) {
  String path = request->url();
  if (path.endsWith("/")) path += "index.html";

  String contentType = getContentType(path);
  bool isGzipped = false;

  // Serve web app files from /dist/ folder
  String distPath = "/dist" + path;
  String pathWithGz = distPath + ".gz";

  if (LittleFS.exists(pathWithGz) || LittleFS.exists(distPath)) {
    if (LittleFS.exists(pathWithGz)) {
      distPath += ".gz";
      isGzipped = true;
    }
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, distPath, contentType);
    response->addHeader("Cache-Control", "max-age=86400");
    if (isGzipped) {
      response->addHeader("Content-Encoding", "gzip");
    }
    request->send(response);
    return;
  }

  // Fallback to root for other files (like wifi.txt, devices.json, etc.)
  isGzipped = false;
  pathWithGz = path + ".gz";
  if (LittleFS.exists(pathWithGz) || LittleFS.exists(path)) {
    if (LittleFS.exists(pathWithGz)) {
      path += ".gz";
      isGzipped = true;
    }
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, path, contentType);
    response->addHeader("Cache-Control", "max-age=86400");
    if (isGzipped) {
      response->addHeader("Content-Encoding", "gzip");
    }
    request->send(response);
    return;
  }

  request->send(404, "text/plain", "FileNotFound");
}

// Handle version endpoint
void handleVersion(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "1.1.R-WS");
}

// Handle devices endpoint
void handleDevices(AsyncWebServerRequest *request) {
  String result = DeviceDiscovery::instance().getSavedDevices();
  request->send(200, "application/json", result);
}

// Handle settings endpoint (GET and POST)
void handleSettings(AsyncWebServerRequest *request) {
  // If query parameters are provided, update settings
  if (request->hasArg("canRXPin") || request->hasArg("canTXPin") ||
      request->hasArg("canSpeed") || request->hasArg("scanStartNode") ||
      request->hasArg("scanEndNode")) {

    if (request->hasArg("canRXPin")) {
      config.setCanRXPin(request->arg("canRXPin").toInt());
    }
    if (request->hasArg("canTXPin")) {
      config.setCanTXPin(request->arg("canTXPin").toInt());
    }
    if (request->hasArg("canEnablePin")) {
      config.setCanEnablePin(request->arg("canEnablePin").toInt());
    }
    if (request->hasArg("canSpeed")) {
      config.setCanSpeed(request->arg("canSpeed").toInt());
    }
    if (request->hasArg("scanStartNode")) {
      config.setScanStartNode(request->arg("scanStartNode").toInt());
    }
    if (request->hasArg("scanEndNode")) {
      config.setScanEndNode(request->arg("scanEndNode").toInt());
    }

    config.saveSettings();
    request->send(200, "text/plain", "Settings saved successfully");
  } else {
    // Return current settings as JSON
    JsonDocument doc;
    doc["canRXPin"] = config.getCanRXPin();
    doc["canTXPin"] = config.getCanTXPin();
    doc["canEnablePin"] = config.getCanEnablePin();
    doc["canSpeed"] = config.getCanSpeed();
    doc["scanStartNode"] = config.getScanStartNode();
    doc["scanEndNode"] = config.getScanEndNode();

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  }
}

// Handle OTA upload complete
void handleOtaUploadComplete(AsyncWebServerRequest *request) {
  // Firmware update completion is handled via WebSocket events
  // The actual update runs asynchronously in the background
  request->send(200, "text/plain", "Firmware upload started");
}

// Handle OTA upload chunks
void handleOtaUpload(AsyncWebServerRequest *request, String filename,
                     size_t index, uint8_t *data, size_t len, bool final) {
  static File firmwareFile;
  static String firmwareFilePath = "/firmware_update.bin";

  if (!index) {
    DBG_OUTPUT_PORT.printf("OTA Upload Start: %s (%zu bytes)\n", filename.c_str(), request->contentLength());

    // Check if device is connected and idle
    if (!DeviceConnection::instance().isIdle()) {
      DBG_OUTPUT_PORT.println("OTA Upload failed - device not idle");
      JsonDocument doc;
      doc["event"] = "otaError";
      doc["data"]["error"] = "Device is busy or not connected";
      String output;
      serializeJson(doc, output);
      ws.textAll(output);
      return;
    }

    // Delete old firmware file if it exists
    if (LittleFS.exists(firmwareFilePath)) {
      LittleFS.remove(firmwareFilePath);
    }

    // Create new firmware file
    firmwareFile = LittleFS.open(firmwareFilePath, "w");
    if (!firmwareFile) {
      DBG_OUTPUT_PORT.println("Failed to create firmware file");
      JsonDocument doc;
      doc["event"] = "otaError";
      doc["data"]["error"] = "Failed to create firmware file";
      String output;
      serializeJson(doc, output);
      ws.textAll(output);
      return;
    }

    setStatusLED(StatusLED::UPDATE);
  }

  // Write chunk to file
  if (firmwareFile && len > 0) {
    size_t written = firmwareFile.write(data, len);
    if (written != len) {
      DBG_OUTPUT_PORT.printf("Failed to write firmware chunk (wrote %zu of %zu bytes)\n", written, len);
      firmwareFile.close();

      JsonDocument doc;
      doc["event"] = "otaError";
      doc["data"]["error"] = "Failed to write firmware data";
      String output;
      serializeJson(doc, output);
      ws.textAll(output);

      setStatusLED(StatusLED::ERROR);
      return;
    }
  }

  if (final) {
    firmwareFile.close();
    DBG_OUTPUT_PORT.printf("Firmware file saved: %zu bytes\n", index + len);

    // Start firmware update process
    int totalPages = OICan::StartUpdate(firmwareFilePath);
    DBG_OUTPUT_PORT.printf("Starting firmware update - %d pages to send\n", totalPages);

    // Send initial progress
    JsonDocument doc;
    doc["event"] = "otaProgress";
    doc["data"]["progress"] = 0;
    String output;
    serializeJson(doc, output);
    ws.textAll(output);
  }
}

// Register all HTTP routes
void registerHttpRoutes(AsyncWebServer& server) {
  server.on("/version", HTTP_GET, handleVersion);
  server.on("/devices", HTTP_GET, handleDevices);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/ota/upload", HTTP_POST, handleOtaUploadComplete, handleOtaUpload);
  server.onNotFound(handleFileRequest);
}
