#ifndef WEBSOCKET_HELPERS_H
#define WEBSOCKET_HELPERS_H

#include <ArduinoJson.h>

#include <AsyncWebSocket.h>

/**
 * Sends a general error message via WebSocket
 *
 * @param client WebSocket client to send the error to
 * @param eventName The event name for the error (e.g., "canMappingError")
 * @param errorMessage The error message to send
 */
inline void sendWebSocketError(AsyncWebSocketClient* client, const char* eventName, const char* errorMessage) {
  JsonDocument errorDoc;
  errorDoc["event"] = eventName;
  errorDoc["data"]["error"] = errorMessage;
  String errorOutput;
  serializeJson(errorDoc, errorOutput);
  client->text(errorOutput);
}

/**
 * Sends a "Device is busy" error message via WebSocket
 *
 * @param client WebSocket client to send the error to
 * @param eventName The event name for the error (e.g., "canMappingError", "startDeviceError")
 */
inline void sendDeviceBusyError(AsyncWebSocketClient* client, const char* eventName) {
  sendWebSocketError(client, eventName, "Device is busy");
}

#endif  // WEBSOCKET_HELPERS_H
