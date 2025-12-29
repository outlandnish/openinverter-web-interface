#pragma once

#include <ESPAsyncWebServer.h>

// Forward declaration
struct CANEvent;

/**
 * Processes CAN events from the event queue and broadcasts them via WebSocket.
 * Uses a dispatch table pattern for cleaner event-to-JSON serialization.
 */
namespace EventProcessor {

/**
 * Process all pending events from canEventQueue and broadcast to WebSocket.
 * Should be called from the main loop.
 * @param ws The WebSocket to broadcast events to
 */
void processEvents(AsyncWebSocket& ws);

/**
 * Process firmware update progress and broadcast status via WebSocket.
 * Should be called from the main loop.
 * @param ws The WebSocket to broadcast progress to
 */
void processFirmwareProgress(AsyncWebSocket& ws);

/**
 * Serialize a single event to JSON and return the event name.
 * Returns empty string if event type is unknown.
 */
const char* serializeEvent(const CANEvent& evt, JsonDocument& doc);

} // namespace EventProcessor
