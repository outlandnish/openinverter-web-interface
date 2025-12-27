#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Main dispatch function - call this from onWebSocketEvent
void dispatchWebSocketMessage(AsyncWebSocketClient* client, JsonDocument& doc);
