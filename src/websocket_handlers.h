#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// WebSocket event handler - register this with ws.onEvent()
void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len);

// Main dispatch function - called from onWebSocketEvent for WS_EVT_DATA
void dispatchWebSocketMessage(AsyncWebSocketClient* client, JsonDocument& doc);

// ============================================================================
// WebSocket Broadcast Helpers
// ============================================================================

/**
 * Broadcast a JSON event to all connected WebSocket clients.
 * @param event The event name
 * @param data JSON string data payload
 */
void broadcastToWebSocket(const char* event, const char* data);

/**
 * Broadcast a device discovery event to all connected WebSocket clients.
 */
void broadcastDeviceDiscovery(uint8_t nodeId, const char* serial, uint32_t lastSeen);

// Forward declarations for all handlers
void handleStartScan(AsyncWebSocketClient* client, JsonDocument& doc);
void handleStopScan(AsyncWebSocketClient* client, JsonDocument& doc);
void handleConnect(AsyncWebSocketClient* client, JsonDocument& doc);
void handleSetDeviceName(AsyncWebSocketClient* client, JsonDocument& doc);
void handleDeleteDevice(AsyncWebSocketClient* client, JsonDocument& doc);
void handleRenameDevice(AsyncWebSocketClient* client, JsonDocument& doc);
void handleGetNodeId(AsyncWebSocketClient* client, JsonDocument& doc);
void handleSetNodeId(AsyncWebSocketClient* client, JsonDocument& doc);
void handleStartSpotValues(AsyncWebSocketClient* client, JsonDocument& doc);
void handleStopSpotValues(AsyncWebSocketClient* client, JsonDocument& doc);
void handleUpdateParam(AsyncWebSocketClient* client, JsonDocument& doc);
void handleGetParamSchema(AsyncWebSocketClient* client, JsonDocument& doc);
void handleGetParamValues(AsyncWebSocketClient* client, JsonDocument& doc);
void handleReloadParams(AsyncWebSocketClient* client, JsonDocument& doc);
void handleResetDevice(AsyncWebSocketClient* client, JsonDocument& doc);
void handleDisconnect(AsyncWebSocketClient* client, JsonDocument& doc);
void handleGetCanMappings(AsyncWebSocketClient* client, JsonDocument& doc);
void handleAddCanMapping(AsyncWebSocketClient* client, JsonDocument& doc);
void handleRemoveCanMapping(AsyncWebSocketClient* client, JsonDocument& doc);
void handleSaveToFlash(AsyncWebSocketClient* client, JsonDocument& doc);
void handleLoadFromFlash(AsyncWebSocketClient* client, JsonDocument& doc);
void handleLoadDefaults(AsyncWebSocketClient* client, JsonDocument& doc);
void handleStartDevice(AsyncWebSocketClient* client, JsonDocument& doc);
void handleStopDevice(AsyncWebSocketClient* client, JsonDocument& doc);
void handleListErrors(AsyncWebSocketClient* client, JsonDocument& doc);
void handleSendCanMessage(AsyncWebSocketClient* client, JsonDocument& doc);
void handleStartCanInterval(AsyncWebSocketClient* client, JsonDocument& doc);
void handleStopCanInterval(AsyncWebSocketClient* client, JsonDocument& doc);
void handleStartCanIoInterval(AsyncWebSocketClient* client, JsonDocument& doc);
void handleStopCanIoInterval(AsyncWebSocketClient* client, JsonDocument& doc);
void handleUpdateCanIoFlags(AsyncWebSocketClient* client, JsonDocument& doc);