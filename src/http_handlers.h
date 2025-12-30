#pragma once
#include <ESPAsyncWebServer.h>

// Route handler registration
// Registers all HTTP routes with the web server
void registerHttpRoutes(AsyncWebServer& server);

// Individual route handlers
void handleVersion(AsyncWebServerRequest* request);
void handleDevices(AsyncWebServerRequest* request);
void handleSettings(AsyncWebServerRequest* request);
void handleOtaUploadComplete(AsyncWebServerRequest* request);
void handleOtaUpload(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len,
                     bool final);
void handleFileRequest(AsyncWebServerRequest* request);

// Helper functions
String formatBytes(uint64_t bytes);
String getContentType(String filename, AsyncWebServerRequest* request = nullptr);
