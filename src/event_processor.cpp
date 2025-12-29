#include "event_processor.h"
#include "models/can_event.h"
#include "firmware/update_handler.h"
#include "managers/device_connection.h"
#include "managers/spot_values_manager.h"
#include "status_led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <ArduinoJson.h>
#include <map>
#include <functional>

#define DBG_OUTPUT_PORT Serial

// External queue handle
extern QueueHandle_t canEventQueue;

namespace EventProcessor {

// Event serializer function type
using EventSerializer = std::function<void(const CANEvent&, JsonObject&)>;

// Individual serializer functions
static void serializeDeviceDiscovered(const CANEvent& evt, JsonObject& data) {
    data["nodeId"] = evt.data.deviceDiscovered.nodeId;
    data["serial"] = evt.data.deviceDiscovered.serial;
    data["lastSeen"] = evt.data.deviceDiscovered.lastSeen;
    if (evt.data.deviceDiscovered.name[0] != '\0') {
        data["name"] = evt.data.deviceDiscovered.name;
    }
}

static void serializeScanStatus(const CANEvent& evt, JsonObject& data) {
    data["active"] = evt.data.scanStatus.active;
}

static void serializeScanProgress(const CANEvent& evt, JsonObject& data) {
    data["currentNode"] = evt.data.scanProgress.currentNode;
    data["startNode"] = evt.data.scanProgress.startNode;
    data["endNode"] = evt.data.scanProgress.endNode;
}

static void serializeConnected(const CANEvent& evt, JsonObject& data) {
    data["nodeId"] = evt.data.connected.nodeId;
    data["serial"] = evt.data.connected.serial;
}

static void serializeNodeIdInfo(const CANEvent& evt, JsonObject& data) {
    data["id"] = evt.data.nodeIdInfo.id;
    data["speed"] = evt.data.nodeIdInfo.speed;
}

static void serializeNodeIdSet(const CANEvent& evt, JsonObject& data) {
    data["id"] = evt.data.nodeIdSet.id;
    data["speed"] = evt.data.nodeIdSet.speed;
}

static void serializeSpotValuesStatus(const CANEvent& evt, JsonObject& data) {
    data["active"] = evt.data.spotValuesStatus.active;
    if (evt.data.spotValuesStatus.active) {
        data["interval"] = evt.data.spotValuesStatus.interval;
        data["paramCount"] = evt.data.spotValuesStatus.paramCount;
    }
}

static void serializeSpotValues(const CANEvent& evt, JsonObject& data) {
    data["timestamp"] = evt.data.spotValues.timestamp;
    JsonDocument valuesDoc;
    deserializeJson(valuesDoc, evt.data.spotValues.valuesJson);
    data["values"] = valuesDoc;
}

static void serializeDeviceNameSet(const CANEvent& evt, JsonObject& data) {
    data["success"] = evt.data.deviceNameSet.success;
    data["serial"] = evt.data.deviceNameSet.serial;
    data["name"] = evt.data.deviceNameSet.name;
}

static void serializeDeviceDeleted(const CANEvent& evt, JsonObject& data) {
    data["success"] = evt.data.deviceDeleted.success;
    data["serial"] = evt.data.deviceDeleted.serial;
}

static void serializeDeviceRenamed(const CANEvent& evt, JsonObject& data) {
    data["success"] = evt.data.deviceRenamed.success;
    data["serial"] = evt.data.deviceRenamed.serial;
    data["name"] = evt.data.deviceRenamed.name;
}

static void serializeCanMessageSent(const CANEvent& evt, JsonObject& data) {
    data["success"] = evt.data.canMessageSent.success;
    data["canId"] = evt.data.canMessageSent.canId;
}

static void serializeCanIntervalStatus(const CANEvent& evt, JsonObject& data) {
    data["active"] = evt.data.canIntervalStatus.active;
    data["intervalId"] = evt.data.canIntervalStatus.intervalId;
    if (evt.data.canIntervalStatus.active) {
        data["intervalMs"] = evt.data.canIntervalStatus.intervalMs;
    }
}

static void serializeCanIoIntervalStatus(const CANEvent& evt, JsonObject& data) {
    data["active"] = evt.data.canIoIntervalStatus.active;
    if (evt.data.canIoIntervalStatus.active) {
        data["intervalMs"] = evt.data.canIoIntervalStatus.intervalMs;
    }
}

static void serializeError(const CANEvent& evt, JsonObject& data) {
    data["message"] = evt.data.error.message;
}

// Event name and serializer dispatch table
struct EventInfo {
    const char* eventName;
    EventSerializer serializer;
};

static const std::map<CANEventType, EventInfo> eventDispatch = {
    {EVT_DEVICE_DISCOVERED,   {"deviceDiscovered",    serializeDeviceDiscovered}},
    {EVT_SCAN_STATUS,         {"scanStatus",          serializeScanStatus}},
    {EVT_SCAN_PROGRESS,       {"scanProgress",        serializeScanProgress}},
    {EVT_CONNECTED,           {"connected",           serializeConnected}},
    {EVT_NODE_ID_INFO,        {"nodeIdInfo",          serializeNodeIdInfo}},
    {EVT_NODE_ID_SET,         {"nodeIdSet",           serializeNodeIdSet}},
    {EVT_SPOT_VALUES_STATUS,  {"spotValuesStatus",    serializeSpotValuesStatus}},
    {EVT_SPOT_VALUES,         {"spotValues",          serializeSpotValues}},
    {EVT_DEVICE_NAME_SET,     {"deviceNameSet",       serializeDeviceNameSet}},
    {EVT_DEVICE_DELETED,      {"deviceDeleted",       serializeDeviceDeleted}},
    {EVT_DEVICE_RENAMED,      {"deviceRenamed",       serializeDeviceRenamed}},
    {EVT_CAN_MESSAGE_SENT,    {"canMessageSent",      serializeCanMessageSent}},
    {EVT_CAN_INTERVAL_STATUS, {"canIntervalStatus",   serializeCanIntervalStatus}},
    {EVT_CANIO_INTERVAL_STATUS, {"canIoIntervalStatus", serializeCanIoIntervalStatus}},
    {EVT_ERROR,               {"error",               serializeError}}
};

const char* serializeEvent(const CANEvent& evt, JsonDocument& doc) {
    auto it = eventDispatch.find(evt.type);
    if (it == eventDispatch.end()) {
        return nullptr;
    }

    const EventInfo& info = it->second;
    doc["event"] = info.eventName;
    JsonObject data = doc["data"].to<JsonObject>();
    info.serializer(evt, data);

    return info.eventName;
}

// Handle EVT_JSON_READY - sends to specific client
static void handleJsonReadyEvent(AsyncWebSocket& ws, const CANEvent& evt) {
    uint32_t clientId = evt.data.jsonReady.clientId;
    AsyncWebSocketClient* client = ws.client(clientId);

    if (client == nullptr || !client->canSend()) {
        DBG_OUTPUT_PORT.printf("[EventProcessor] Client %lu not found or can't send\n", (unsigned long)clientId);
        return;
    }

    if (!evt.data.jsonReady.success) {
        // Send error response
        JsonDocument errorDoc;
        errorDoc["event"] = "paramValuesError";
        errorDoc["data"]["error"] = "Failed to download parameters";
        errorDoc["data"]["nodeId"] = evt.data.jsonReady.nodeId;
        String errorOutput;
        serializeJson(errorDoc, errorOutput);
        client->text(errorOutput);
        DBG_OUTPUT_PORT.println("[EventProcessor] Sent paramValuesError");
        return;
    }

    // Get the cached JSON
    DeviceConnection& conn = DeviceConnection::instance();
    String json = conn.getJsonReceiveBufferCopy();

    if (json.isEmpty()) {
        JsonDocument errorDoc;
        errorDoc["event"] = "paramValuesError";
        errorDoc["data"]["error"] = "No parameter data available";
        errorDoc["data"]["nodeId"] = evt.data.jsonReady.nodeId;
        String errorOutput;
        serializeJson(errorDoc, errorOutput);
        client->text(errorOutput);
        return;
    }

    DBG_OUTPUT_PORT.printf("[EventProcessor] Sending JSON to client %lu (%d bytes)\n",
                           (unsigned long)clientId, json.length());

    // Merge with latest spot values
    const auto& latestSpotValues = SpotValuesManager::instance().getLatestValues();
    if (!latestSpotValues.empty()) {
        JsonDocument paramsDoc;
        DeserializationError error = deserializeJson(paramsDoc, json);
        if (!error) {
            for (const auto& pair : latestSpotValues) {
                String paramId = String(pair.first);
                if (paramsDoc.containsKey(paramId)) {
                    paramsDoc[paramId]["value"] = pair.second;
                }
            }
            json = "";
            serializeJson(paramsDoc, json);
        }
    }

    // Build response
    String output = "{\"event\":\"paramValuesData\",\"data\":{\"nodeId\":";
    output += evt.data.jsonReady.nodeId;
    output += ",\"rawParams\":";
    output += json;
    output += "}}";

    client->text(output);
    DBG_OUTPUT_PORT.printf("[EventProcessor] Sent param values (%d bytes)\n", output.length());
}

void processEvents(AsyncWebSocket& ws) {
    CANEvent evt;

    // Process all pending events (non-blocking)
    while (xQueueReceive(canEventQueue, &evt, 0) == pdTRUE) {
        // Handle special events that need per-client delivery
        if (evt.type == EVT_JSON_READY) {
            handleJsonReadyEvent(ws, evt);
            continue;
        }

        JsonDocument doc;

        const char* eventName = serializeEvent(evt, doc);
        if (eventName == nullptr) {
            continue; // Unknown event type, skip
        }

        String output;
        serializeJson(doc, output);
        ws.textAll(output);
    }
}

void processFirmwareProgress(AsyncWebSocket& ws) {
    FirmwareUpdateHandler& handler = FirmwareUpdateHandler::instance();

    // Check for progress updates
    int progress = 0;
    if (handler.checkProgressUpdate(progress)) {
        DBG_OUTPUT_PORT.printf("Firmware update progress: page %d/%d (%d%%)\n",
                               handler.getCurrentPage(), handler.getTotalPages(), progress);

        JsonDocument doc;
        doc["event"] = "otaProgress";
        doc["data"]["progress"] = progress;
        String output;
        serializeJson(doc, output);
        ws.textAll(output);
    }

    // Check for completion
    if (handler.checkCompletion()) {
        DBG_OUTPUT_PORT.println("Firmware update completed successfully");

        JsonDocument doc;
        doc["event"] = "otaSuccess";
        String output;
        serializeJson(doc, output);
        ws.textAll(output);

        StatusLED::instance().setColor(StatusLED::SUCCESS);
        delay(1000);
        StatusLED::instance().off();
    }
}

} // namespace EventProcessor
