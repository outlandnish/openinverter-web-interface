#include "websocket_handlers.h"
#include <map>
#include <string>
#include <vector>
#include "oi_can.h"
#include "utils/string_utils.h"
#include "utils/websocket_helpers.h"
#include "managers/client_lock_manager.h"
#include "managers/can_interval_manager.h"
#include "managers/spot_values_manager.h"
#include "managers/device_connection.h"
#include "managers/device_discovery.h"
#include "managers/device_cache.h"
#include "main.h"

// External references to globals from main.cpp
extern AsyncWebSocket ws;

// ============================================================================
// WebSocket Broadcast Helpers
// ============================================================================

void broadcastToWebSocket(const char* event, const char* data) {
    JsonDocument doc;
    doc["event"] = event;
    JsonDocument dataDoc;
    deserializeJson(dataDoc, data);
    doc["data"] = dataDoc;

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

void broadcastDeviceDiscovery(uint8_t nodeId, const char* serial, uint32_t lastSeen) {
    JsonDocument doc;
    doc["event"] = "deviceDiscovered";

    JsonObject data = doc["data"].to<JsonObject>();
    data["nodeId"] = nodeId;
    data["serial"] = serial;
    data["lastSeen"] = lastSeen;

    // Look up device name from cache
    std::string name = DeviceCache::instance().getDeviceName(serial);
    if (!name.empty()) {
        data["name"] = name;
    }

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
    DBG_OUTPUT_PORT.printf("Broadcast device discovery: %s\n", output.c_str());
}

// Handler function type
using WebSocketHandler = void (*)(AsyncWebSocketClient*, JsonDocument&);

// WebSocket action dispatch table
static const std::map<std::string, WebSocketHandler> wsHandlers = {
    {"startScan", handleStartScan},
    {"stopScan", handleStopScan},
    {"connect", handleConnect},
    {"setDeviceName", handleSetDeviceName},
    {"deleteDevice", handleDeleteDevice},
    {"renameDevice", handleRenameDevice},
    {"getNodeId", handleGetNodeId},
    {"setNodeId", handleSetNodeId},
    {"startSpotValues", handleStartSpotValues},
    {"stopSpotValues", handleStopSpotValues},
    {"updateParam", handleUpdateParam},
    {"getParamSchema", handleGetParamSchema},
    {"getParamValues", handleGetParamValues},
    {"reloadParams", handleReloadParams},
    {"resetDevice", handleResetDevice},
    {"disconnect", handleDisconnect},
    {"getCanMappings", handleGetCanMappings},
    {"addCanMapping", handleAddCanMapping},
    {"removeCanMapping", handleRemoveCanMapping},
    {"saveToFlash", handleSaveToFlash},
    {"loadFromFlash", handleLoadFromFlash},
    {"loadDefaults", handleLoadDefaults},
    {"startDevice", handleStartDevice},
    {"stopDevice", handleStopDevice},
    {"listErrors", handleListErrors},
    {"sendCanMessage", handleSendCanMessage},
    {"startCanInterval", handleStartCanInterval},
    {"stopCanInterval", handleStopCanInterval},
    {"startCanIoInterval", handleStartCanIoInterval},
    {"stopCanIoInterval", handleStopCanIoInterval},
    {"updateCanIoFlags", handleUpdateCanIoFlags}
};

// Main dispatch function
void dispatchWebSocketMessage(AsyncWebSocketClient* client, JsonDocument& doc) {
    String action = doc["action"].as<String>();

    auto it = wsHandlers.find(action.c_str());
    if (it != wsHandlers.end()) {
        it->second(client, doc);
    } else {
        DBG_OUTPUT_PORT.printf("[WebSocket] Unknown action: %s\n", action.c_str());
    }
}

// Handler implementations
void handleStartScan(AsyncWebSocketClient* client, JsonDocument& doc) {
    uint8_t start = doc["start"] | 1;
    uint8_t end = doc["end"] | 32;

    CANCommand cmd;
    cmd.type = CMD_START_SCAN;
    cmd.data.scan.start = start;
    cmd.data.scan.end = end;

    queueCanCommand(cmd, "Scan start");
}

void handleStopScan(AsyncWebSocketClient* client, JsonDocument& doc) {
    CANCommand cmd;
    cmd.type = CMD_STOP_SCAN;

    queueCanCommand(cmd, "Scan stop");
}

void handleConnect(AsyncWebSocketClient* client, JsonDocument& doc) {
    uint8_t nodeId = doc["nodeId"];
    String serial = doc["serial"] | "";
    uint32_t clientId = client->id();

    ClientLockManager& lockMgr = ClientLockManager::instance();

    // Check if device is already locked by another client
    if (lockMgr.isDeviceLocked(nodeId) && !lockMgr.isDeviceLockedByClient(nodeId, clientId)) {
        DBG_OUTPUT_PORT.printf("[WebSocket] ERROR: Node %d is already connected by client #%lu\n",
                               nodeId, (unsigned long)lockMgr.getLockHolder(nodeId));

        JsonDocument errorDoc;
        errorDoc["event"] = "error";
        errorDoc["data"]["message"] = String("Device ") + serial + " (node " + String(nodeId) +
                                       ") is already connected by another client. Please wait for the other client to disconnect.";
        errorDoc["data"]["nodeId"] = nodeId;
        errorDoc["data"]["serial"] = serial;
        errorDoc["data"]["type"] = "device_locked";
        String errorOutput;
        serializeJson(errorDoc, errorOutput);
        client->text(errorOutput);
        return;
    }

    // Try to acquire lock
    if (!lockMgr.tryAcquireLock(nodeId, clientId)) {
        DBG_OUTPUT_PORT.printf("[WebSocket] ERROR: Failed to acquire lock for node %d\n", nodeId);
        return;
    }

    CANCommand cmd;
    cmd.type = CMD_CONNECT;
    cmd.data.connect.nodeId = nodeId;
    safeCopyString(cmd.data.connect.serial, serial);

    if (!queueCanCommand(cmd, "Connect")) {
        // Release lock on failure
        lockMgr.releaseLock(nodeId);
    }
}

void handleSetDeviceName(AsyncWebSocketClient* client, JsonDocument& doc) {
    String serial = doc["serial"];
    String name = doc["name"];
    int nodeId = doc["nodeId"] | -1;

    CANCommand cmd;
    cmd.type = CMD_SET_DEVICE_NAME;
    safeCopyString(cmd.data.setDeviceName.serial, serial);
    safeCopyString(cmd.data.setDeviceName.name, name);
    cmd.data.setDeviceName.nodeId = nodeId;

    queueCanCommand(cmd, "Set device name");
}

void handleDeleteDevice(AsyncWebSocketClient* client, JsonDocument& doc) {
    String serial = doc["serial"];

    CANCommand cmd;
    cmd.type = CMD_DELETE_DEVICE;
    safeCopyString(cmd.data.deleteDevice.serial, serial);

    queueCanCommand(cmd, "Delete device");
}

void handleRenameDevice(AsyncWebSocketClient* client, JsonDocument& doc) {
    String serial = doc["serial"];
    String name = doc["name"];

    CANCommand cmd;
    cmd.type = CMD_RENAME_DEVICE;
    safeCopyString(cmd.data.renameDevice.serial, serial);
    safeCopyString(cmd.data.renameDevice.name, name);

    queueCanCommand(cmd, "Rename device");
}

void handleGetNodeId(AsyncWebSocketClient* client, JsonDocument& doc) {
    CANCommand cmd;
    cmd.type = CMD_GET_NODE_ID;

    queueCanCommand(cmd, "Get node ID");
}

void handleSetNodeId(AsyncWebSocketClient* client, JsonDocument& doc) {
    int id = doc["id"];

    CANCommand cmd;
    cmd.type = CMD_SET_NODE_ID;
    cmd.data.setNodeId.nodeId = id;

    queueCanCommand(cmd, "Set node ID");
}

void handleSendCanMessage(AsyncWebSocketClient* client, JsonDocument& doc) {
    CANCommand cmd;
    cmd.type = CMD_SEND_CAN_MESSAGE;

    if (!doc.containsKey("canId")) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: sendCanMessage missing canId");
        return;
    }
    cmd.data.sendCanMessage.canId = doc["canId"].as<uint32_t>();

    if (!doc.containsKey("data")) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: sendCanMessage missing data");
        return;
    }

    JsonArray dataArray = doc["data"].as<JsonArray>();
    cmd.data.sendCanMessage.dataLength = 0;
    for (JsonVariant dataByte : dataArray) {
        if (cmd.data.sendCanMessage.dataLength < 8) {
            cmd.data.sendCanMessage.data[cmd.data.sendCanMessage.dataLength++] = dataByte.as<uint8_t>();
        }
    }

    queueCanCommand(cmd, "Send CAN message");
}

void handleStartCanInterval(AsyncWebSocketClient* client, JsonDocument& doc) {
    CANCommand cmd;
    cmd.type = CMD_START_CAN_INTERVAL;

    if (!doc.containsKey("intervalId")) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: startCanInterval missing intervalId");
        return;
    }
    String intervalId = doc["intervalId"].as<String>();
    safeCopyString(cmd.data.startCanInterval.intervalId, intervalId);

    if (!doc.containsKey("canId")) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: startCanInterval missing canId");
        return;
    }
    cmd.data.startCanInterval.canId = doc["canId"].as<uint32_t>();

    if (!doc.containsKey("data")) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: startCanInterval missing data");
        return;
    }

    JsonArray dataArray = doc["data"].as<JsonArray>();
    cmd.data.startCanInterval.dataLength = 0;
    for (JsonVariant dataByte : dataArray) {
        if (cmd.data.startCanInterval.dataLength < 8) {
            cmd.data.startCanInterval.data[cmd.data.startCanInterval.dataLength++] = dataByte.as<uint8_t>();
        }
    }

    if (!doc.containsKey("interval")) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: startCanInterval missing interval");
        return;
    }
    cmd.data.startCanInterval.intervalMs = doc["interval"].as<uint32_t>();
    if (cmd.data.startCanInterval.intervalMs < CAN_INTERVAL_MIN_MS) cmd.data.startCanInterval.intervalMs = CAN_INTERVAL_MIN_MS;
    if (cmd.data.startCanInterval.intervalMs > CAN_INTERVAL_MAX_MS) cmd.data.startCanInterval.intervalMs = CAN_INTERVAL_MAX_MS;

    queueCanCommand(cmd, "Start CAN interval");
}

void handleStopCanInterval(AsyncWebSocketClient* client, JsonDocument& doc) {
    CANCommand cmd;
    cmd.type = CMD_STOP_CAN_INTERVAL;

    if (!doc.containsKey("intervalId")) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: stopCanInterval missing intervalId");
        return;
    }
    String intervalId = doc["intervalId"].as<String>();
    safeCopyString(cmd.data.stopCanInterval.intervalId, intervalId);

    queueCanCommand(cmd, "Stop CAN interval");
}

void handleStartCanIoInterval(AsyncWebSocketClient* client, JsonDocument& doc) {
    CANCommand cmd;
    cmd.type = CMD_START_CANIO_INTERVAL;

    cmd.data.startCanIoInterval.canId = doc.containsKey("canId") ? doc["canId"].as<uint32_t>() : 0x3F;
    cmd.data.startCanIoInterval.pot = doc.containsKey("pot") ? doc["pot"].as<uint16_t>() : 0;
    cmd.data.startCanIoInterval.pot2 = doc.containsKey("pot2") ? doc["pot2"].as<uint16_t>() : 0;
    cmd.data.startCanIoInterval.canio = doc.containsKey("canio") ? doc["canio"].as<uint8_t>() : 0;
    cmd.data.startCanIoInterval.cruisespeed = doc.containsKey("cruisespeed") ? doc["cruisespeed"].as<uint16_t>() : 0;
    cmd.data.startCanIoInterval.regenpreset = doc.containsKey("regenpreset") ? doc["regenpreset"].as<uint8_t>() : 0;

    cmd.data.startCanIoInterval.intervalMs = doc.containsKey("interval") ? doc["interval"].as<uint32_t>() : 100;
    if (cmd.data.startCanIoInterval.intervalMs < CAN_IO_INTERVAL_MIN_MS) cmd.data.startCanIoInterval.intervalMs = CAN_IO_INTERVAL_MIN_MS;
    if (cmd.data.startCanIoInterval.intervalMs > CAN_IO_INTERVAL_MAX_MS) cmd.data.startCanIoInterval.intervalMs = CAN_IO_INTERVAL_MAX_MS;

    cmd.data.startCanIoInterval.useCrc = doc.containsKey("useCrc") ? doc["useCrc"].as<bool>() : false;

    queueCanCommand(cmd, "Start CAN IO interval");
}

void handleStopCanIoInterval(AsyncWebSocketClient* client, JsonDocument& doc) {
    CANCommand cmd;
    cmd.type = CMD_STOP_CANIO_INTERVAL;

    queueCanCommand(cmd, "Stop CAN IO interval");
}

void handleUpdateCanIoFlags(AsyncWebSocketClient* client, JsonDocument& doc) {
    CANCommand cmd;
    cmd.type = CMD_UPDATE_CANIO_FLAGS;

    cmd.data.updateCanIoFlags.pot = doc.containsKey("pot") ? doc["pot"].as<uint16_t>() : 0;
    cmd.data.updateCanIoFlags.pot2 = doc.containsKey("pot2") ? doc["pot2"].as<uint16_t>() : 0;
    cmd.data.updateCanIoFlags.canio = doc.containsKey("canio") ? doc["canio"].as<uint8_t>() : 0;
    cmd.data.updateCanIoFlags.cruisespeed = doc.containsKey("cruisespeed") ? doc["cruisespeed"].as<uint16_t>() : 0;
    cmd.data.updateCanIoFlags.regenpreset = doc.containsKey("regenpreset") ? doc["regenpreset"].as<uint8_t>() : 0;

    queueCanCommand(cmd, "Update CAN IO flags");
}

void handleStartSpotValues(AsyncWebSocketClient* client, JsonDocument& doc) {
    CANCommand cmd;
    cmd.type = CMD_START_SPOT_VALUES;

    if (doc.containsKey("paramIds")) {
        JsonArray paramIds = doc["paramIds"].as<JsonArray>();
        cmd.data.spotValues.paramCount = 0;
        for (JsonVariant id : paramIds) {
            if (cmd.data.spotValues.paramCount < MAX_PARAM_IDS) {
                cmd.data.spotValues.paramIds[cmd.data.spotValues.paramCount++] = id.as<int>();
            }
        }
    }

    if (doc.containsKey("interval")) {
        cmd.data.spotValues.interval = doc["interval"].as<uint32_t>();
        if (cmd.data.spotValues.interval < SPOT_VALUES_INTERVAL_MIN_MS) cmd.data.spotValues.interval = SPOT_VALUES_INTERVAL_MIN_MS;
        if (cmd.data.spotValues.interval > SPOT_VALUES_INTERVAL_MAX_MS) cmd.data.spotValues.interval = SPOT_VALUES_INTERVAL_MAX_MS;
    } else {
        cmd.data.spotValues.interval = 1000;
    }

    queueCanCommand(cmd, "Start spot values");
}

void handleStopSpotValues(AsyncWebSocketClient* client, JsonDocument& doc) {
    CANCommand cmd;
    cmd.type = CMD_STOP_SPOT_VALUES;

    queueCanCommand(cmd, "Stop spot values");
}

void handleUpdateParam(AsyncWebSocketClient* client, JsonDocument& doc) {
    int paramId = doc["paramId"];
    double value = doc["value"];

    DBG_OUTPUT_PORT.printf("[WebSocket] Update param request: paramId=%d, value=%f\n", paramId, value);

    if (!DeviceConnection::instance().isIdle()) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Cannot update parameter - device busy");
        JsonDocument errorDoc;
        errorDoc["event"] = "paramUpdateError";
        errorDoc["data"]["paramId"] = paramId;
        errorDoc["data"]["error"] = "Device is busy";
        String errorOutput;
        serializeJson(errorDoc, errorOutput);
        client->text(errorOutput);
        return;
    }

    OICan::SetResult result = OICan::SetValue(paramId, value);

    JsonDocument responseDoc;
    if (result == OICan::Ok) {
        responseDoc["event"] = "paramUpdateSuccess";
        responseDoc["data"]["paramId"] = paramId;
        responseDoc["data"]["value"] = value;
        DBG_OUTPUT_PORT.printf("[WebSocket] Parameter %d updated successfully to %f\n", paramId, value);
    } else {
        responseDoc["event"] = "paramUpdateError";
        responseDoc["data"]["paramId"] = paramId;
        responseDoc["data"]["value"] = value;

        if (result == OICan::UnknownIndex) {
            responseDoc["data"]["error"] = "Unknown parameter ID";
        } else if (result == OICan::ValueOutOfRange) {
            responseDoc["data"]["error"] = "Value out of range";
        } else if (result == OICan::CommError) {
            responseDoc["data"]["error"] = "Communication error";
        } else {
            responseDoc["data"]["error"] = "Unknown error";
        }
        DBG_OUTPUT_PORT.printf("[WebSocket] Parameter %d update failed: %d\n", paramId, result);
    }

    String output;
    serializeJson(responseDoc, output);
    client->text(output);
}

void handleReloadParams(AsyncWebSocketClient* client, JsonDocument& doc) {
    int nodeId = doc["nodeId"];
    DBG_OUTPUT_PORT.printf("[WebSocket] Reload params request for nodeId: %d\n", nodeId);

    bool success = OICan::ReloadJson(nodeId);

    JsonDocument responseDoc;
    if (success) {
        responseDoc["event"] = "paramsReloaded";
        responseDoc["data"]["nodeId"] = nodeId;
        responseDoc["data"]["message"] = "Cached JSON cleared, will reload from device";
    } else {
        responseDoc["event"] = "paramsError";
        responseDoc["data"]["error"] = "Device busy, cannot reload";
        responseDoc["data"]["nodeId"] = nodeId;
    }

    String output;
    serializeJson(responseDoc, output);
    client->text(output);
    DBG_OUTPUT_PORT.printf("[WebSocket] Sent reload response (success=%d)\n", success);
}

void handleResetDevice(AsyncWebSocketClient* client, JsonDocument& doc) {
    DBG_OUTPUT_PORT.println("[WebSocket] Reset device request");

    bool success = OICan::ResetDevice();

    JsonDocument responseDoc;
    if (success) {
        responseDoc["event"] = "deviceReset";
        responseDoc["data"]["message"] = "Device reset command sent";
    } else {
        responseDoc["event"] = "deviceResetError";
        responseDoc["data"]["error"] = "Device busy or not connected";
    }

    String output;
    serializeJson(responseDoc, output);
    client->text(output);
    DBG_OUTPUT_PORT.printf("[WebSocket] Sent reset response (success=%d)\n", success);
}

void handleGetParamSchema(AsyncWebSocketClient* client, JsonDocument& doc) {
    int nodeId = doc["nodeId"];
    DBG_OUTPUT_PORT.printf("[WebSocket] Get param schema request for nodeId: %d\n", nodeId);

    String json = OICan::GetRawJson(nodeId);

    if (json.isEmpty() || json == "{}") {
        JsonDocument errorDoc;
        errorDoc["event"] = "paramSchemaError";
        errorDoc["data"]["error"] = "Device busy or not connected";
        errorDoc["data"]["nodeId"] = nodeId;
        String errorOutput;
        serializeJson(errorDoc, errorOutput);
        client->text(errorOutput);
        DBG_OUTPUT_PORT.println("[WebSocket] Sent paramSchemaError - device busy");
    } else {
        DBG_OUTPUT_PORT.printf("[WebSocket] Sending raw JSON as schema (%d bytes)\n", json.length());

        String output = "{\"event\":\"paramSchemaData\",\"data\":{\"nodeId\":";
        output += nodeId;
        output += ",\"schema\":";
        output += json;
        output += "}}";

        client->text(output);
        DBG_OUTPUT_PORT.printf("[WebSocket] Sent param schema (%d bytes)\n", output.length());
    }
}

void handleGetParamValues(AsyncWebSocketClient* client, JsonDocument& doc) {
    int nodeId = doc["nodeId"];
    DBG_OUTPUT_PORT.printf("[WebSocket] Get param values request for nodeId: %d\n", nodeId);

    String json = OICan::GetRawJson(nodeId);

    if (json.isEmpty() || json == "{}") {
        JsonDocument errorDoc;
        errorDoc["event"] = "paramValuesError";
        errorDoc["data"]["error"] = "Device busy or not connected";
        errorDoc["data"]["nodeId"] = nodeId;
        String errorOutput;
        serializeJson(errorDoc, errorOutput);
        client->text(errorOutput);
        DBG_OUTPUT_PORT.println("[WebSocket] Sent paramValuesError - device busy");
    } else {
        DBG_OUTPUT_PORT.printf("[WebSocket] Sending raw JSON for values (%d bytes)\n", json.length());

        // Update cached param values with latest spot values to avoid stale data
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

        String output = "{\"event\":\"paramValuesData\",\"data\":{\"nodeId\":";
        output += nodeId;
        output += ",\"rawParams\":";
        output += json;
        output += "}}";

        client->text(output);
        DBG_OUTPUT_PORT.printf("[WebSocket] Sent param values (%d bytes)\n", output.length());
    }
}

void handleDisconnect(AsyncWebSocketClient* client, JsonDocument& doc) {
    uint32_t clientId = client->id();
    ClientLockManager& lockMgr = ClientLockManager::instance();

    if (lockMgr.hasClientLock(clientId)) {
        uint8_t nodeId = lockMgr.getClientDevice(clientId);
        lockMgr.releaseClientLocks(clientId);

        // Clear interval messages when disconnecting
        CanIntervalManager::instance().clearAllIntervals();

        // Notify other clients that the device is now available
        JsonDocument notifyDoc;
        notifyDoc["event"] = "deviceUnlocked";
        notifyDoc["data"]["nodeId"] = nodeId;
        String output;
        serializeJson(notifyDoc, output);
        ws.textAll(output);

        // Send disconnected event to the client
        JsonDocument disconnectDoc;
        disconnectDoc["event"] = "disconnected";
        String disconnectOutput;
        serializeJson(disconnectDoc, disconnectOutput);
        client->text(disconnectOutput);
    }
}

void handleGetCanMappings(AsyncWebSocketClient* client, JsonDocument& doc) {
    DBG_OUTPUT_PORT.println("[WebSocket] Get CAN mappings request");

    if (!DeviceConnection::instance().isIdle()) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Cannot get mappings - device busy");
        sendDeviceBusyError(client, "canMappingsError");
        return;
    }

    String mappingsJson = OICan::GetCanMapping();

    JsonDocument responseDoc;
    responseDoc["event"] = "canMappingsData";

    JsonDocument mappingsArrayDoc;
    deserializeJson(mappingsArrayDoc, mappingsJson);
    responseDoc["data"]["mappings"] = mappingsArrayDoc;

    String output;
    serializeJson(responseDoc, output);
    client->text(output);
    DBG_OUTPUT_PORT.printf("[WebSocket] Sent CAN mappings data (%d bytes)\n", output.length());
}

void handleAddCanMapping(AsyncWebSocketClient* client, JsonDocument& doc) {
    DBG_OUTPUT_PORT.println("[WebSocket] Add CAN mapping request");

    if (!DeviceConnection::instance().isIdle()) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Cannot add mapping - device busy");
        sendDeviceBusyError(client, "canMappingError");
        return;
    }

    JsonDocument mappingDoc;
    mappingDoc["isrx"] = doc["isrx"];
    mappingDoc["id"] = doc["id"];
    mappingDoc["paramid"] = doc["paramid"];
    mappingDoc["position"] = doc["position"];
    mappingDoc["length"] = doc["length"];
    mappingDoc["gain"] = doc["gain"];
    mappingDoc["offset"] = doc["offset"];

    String mappingJson;
    serializeJson(mappingDoc, mappingJson);

    OICan::SetResult result = OICan::AddCanMapping(mappingJson);

    JsonDocument responseDoc;
    if (result == OICan::Ok) {
        responseDoc["event"] = "canMappingAdded";
        responseDoc["data"]["success"] = true;
        DBG_OUTPUT_PORT.println("[WebSocket] CAN mapping added successfully");
    } else {
        responseDoc["event"] = "canMappingError";
        responseDoc["data"]["success"] = false;

        if (result == OICan::UnknownIndex) {
            responseDoc["data"]["error"] = "Invalid mapping parameters";
        } else if (result == OICan::CommError) {
            responseDoc["data"]["error"] = "Communication error";
        } else {
            responseDoc["data"]["error"] = "Unknown error";
        }
        DBG_OUTPUT_PORT.printf("[WebSocket] CAN mapping add failed: %d\n", result);
    }

    String output;
    serializeJson(responseDoc, output);
    client->text(output);
}

void handleRemoveCanMapping(AsyncWebSocketClient* client, JsonDocument& doc) {
    DBG_OUTPUT_PORT.println("[WebSocket] Remove CAN mapping request");

    if (!DeviceConnection::instance().isIdle()) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Cannot remove mapping - device busy");
        sendDeviceBusyError(client, "canMappingError");
        return;
    }

    JsonDocument mappingDoc;
    mappingDoc["index"] = doc["index"];
    mappingDoc["subindex"] = doc["subindex"];

    String mappingJson;
    serializeJson(mappingDoc, mappingJson);

    OICan::SetResult result = OICan::RemoveCanMapping(mappingJson);

    JsonDocument responseDoc;
    if (result == OICan::Ok) {
        responseDoc["event"] = "canMappingRemoved";
        responseDoc["data"]["success"] = true;
        DBG_OUTPUT_PORT.println("[WebSocket] CAN mapping removed successfully");
    } else {
        responseDoc["event"] = "canMappingError";
        responseDoc["data"]["success"] = false;

        if (result == OICan::UnknownIndex) {
            responseDoc["data"]["error"] = "Invalid index or subindex";
        } else if (result == OICan::CommError) {
            responseDoc["data"]["error"] = "Communication error";
        } else {
            responseDoc["data"]["error"] = "Unknown error";
        }
        DBG_OUTPUT_PORT.printf("[WebSocket] CAN mapping remove failed: %d\n", result);
    }

    String output;
    serializeJson(responseDoc, output);
    client->text(output);
}

void handleSaveToFlash(AsyncWebSocketClient* client, JsonDocument& doc) {
    DBG_OUTPUT_PORT.println("[WebSocket] Save to flash request");

    if (!DeviceConnection::instance().isIdle()) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Cannot save to flash - device busy");
        sendDeviceBusyError(client, "saveToFlashError");
        return;
    }

    bool success = OICan::SaveToFlash();

    JsonDocument responseDoc;
    if (success) {
        responseDoc["event"] = "saveToFlashSuccess";
        responseDoc["data"]["message"] = "Parameters saved to flash";
        DBG_OUTPUT_PORT.println("[WebSocket] Parameters saved to flash successfully");
    } else {
        responseDoc["event"] = "saveToFlashError";
        responseDoc["data"]["error"] = "Failed to save parameters";
        DBG_OUTPUT_PORT.println("[WebSocket] Failed to save parameters to flash");
    }

    String output;
    serializeJson(responseDoc, output);
    client->text(output);
}

void handleLoadFromFlash(AsyncWebSocketClient* client, JsonDocument& doc) {
    DBG_OUTPUT_PORT.println("[WebSocket] Load from flash request");

    if (!DeviceConnection::instance().isIdle()) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Cannot load from flash - device busy");
        sendDeviceBusyError(client, "loadFromFlashError");
        return;
    }

    bool success = OICan::LoadFromFlash();

    JsonDocument responseDoc;
    if (success) {
        responseDoc["event"] = "loadFromFlashSuccess";
        responseDoc["data"]["message"] = "Parameters loaded from flash";
        DBG_OUTPUT_PORT.println("[WebSocket] Parameters loaded from flash successfully");
    } else {
        responseDoc["event"] = "loadFromFlashError";
        responseDoc["data"]["error"] = "Failed to load parameters";
        DBG_OUTPUT_PORT.println("[WebSocket] Failed to load parameters from flash");
    }

    String output;
    serializeJson(responseDoc, output);
    client->text(output);
}

void handleLoadDefaults(AsyncWebSocketClient* client, JsonDocument& doc) {
    DBG_OUTPUT_PORT.println("[WebSocket] Load defaults request");

    if (!DeviceConnection::instance().isIdle()) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Cannot load defaults - device busy");
        sendDeviceBusyError(client, "loadDefaultsError");
        return;
    }

    bool success = OICan::LoadDefaults();

    JsonDocument responseDoc;
    if (success) {
        responseDoc["event"] = "loadDefaultsSuccess";
        responseDoc["data"]["message"] = "Default parameters loaded";
        DBG_OUTPUT_PORT.println("[WebSocket] Default parameters loaded successfully");
    } else {
        responseDoc["event"] = "loadDefaultsError";
        responseDoc["data"]["error"] = "Failed to load defaults";
        DBG_OUTPUT_PORT.println("[WebSocket] Failed to load default parameters");
    }

    String output;
    serializeJson(responseDoc, output);
    client->text(output);
}

void handleStartDevice(AsyncWebSocketClient* client, JsonDocument& doc) {
    DBG_OUTPUT_PORT.println("[WebSocket] Start device request");

    if (!DeviceConnection::instance().isIdle()) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Cannot start device - device busy");
        sendDeviceBusyError(client, "startDeviceError");
        return;
    }

    uint32_t mode = doc.containsKey("mode") ? doc["mode"].as<uint32_t>() : 0;

    bool success = OICan::StartDevice(mode);

    JsonDocument responseDoc;
    if (success) {
        responseDoc["event"] = "startDeviceSuccess";
        responseDoc["data"]["message"] = "Device started";
        DBG_OUTPUT_PORT.println("[WebSocket] Device started successfully");
    } else {
        responseDoc["event"] = "startDeviceError";
        responseDoc["data"]["error"] = "Failed to start device";
        DBG_OUTPUT_PORT.println("[WebSocket] Failed to start device");
    }

    String output;
    serializeJson(responseDoc, output);
    client->text(output);
}

void handleStopDevice(AsyncWebSocketClient* client, JsonDocument& doc) {
    DBG_OUTPUT_PORT.println("[WebSocket] Stop device request");

    if (!DeviceConnection::instance().isIdle()) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Cannot stop device - device busy");
        sendDeviceBusyError(client, "stopDeviceError");
        return;
    }

    bool success = OICan::StopDevice();

    JsonDocument responseDoc;
    if (success) {
        responseDoc["event"] = "stopDeviceSuccess";
        responseDoc["data"]["message"] = "Device stopped";
        DBG_OUTPUT_PORT.println("[WebSocket] Device stopped successfully");
    } else {
        responseDoc["event"] = "stopDeviceError";
        responseDoc["data"]["error"] = "Failed to stop device";
        DBG_OUTPUT_PORT.println("[WebSocket] Failed to stop device");
    }

    String output;
    serializeJson(responseDoc, output);
    client->text(output);
}

void handleListErrors(AsyncWebSocketClient* client, JsonDocument& doc) {
    DBG_OUTPUT_PORT.println("[WebSocket] List errors request");

    if (!DeviceConnection::instance().isIdle()) {
        DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Cannot list errors - device busy");
        sendDeviceBusyError(client, "listErrorsError");
        return;
    }

    String errorsJson = OICan::ListErrors();

    JsonDocument responseDoc;
    responseDoc["event"] = "listErrorsSuccess";

    JsonDocument errorsArray;
    DeserializationError error = deserializeJson(errorsArray, errorsJson);

    if (!error) {
        responseDoc["data"]["errors"] = errorsArray.as<JsonArray>();
        DBG_OUTPUT_PORT.printf("[WebSocket] Listed errors successfully (%d bytes)\n", errorsJson.length());
    } else {
        responseDoc["data"]["errors"] = JsonArray();
        DBG_OUTPUT_PORT.printf("[WebSocket] Failed to parse errors JSON: %s\n", error.c_str());
    }

    String output;
    serializeJson(responseDoc, output);
    client->text(output);
}

// ============================================================================
// WebSocket Event Handler
// ============================================================================

void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
    ClientLockManager& lockMgr = ClientLockManager::instance();

    if (type == WS_EVT_CONNECT) {
        DBG_OUTPUT_PORT.printf("WebSocket client #%lu connected from %s\n",
                               (unsigned long)client->id(), client->remoteIP().toString().c_str());

        // Send current scanning status
        JsonDocument doc;
        doc["event"] = "scanStatus";
        doc["data"]["active"] = DeviceDiscovery::instance().isScanActive();
        String output;
        serializeJson(doc, output);
        client->text(output);

        // Send saved devices
        String devices = DeviceDiscovery::instance().getSavedDevices();
        JsonDocument devicesMsg;
        devicesMsg["event"] = "savedDevices";
        JsonDocument devicesData;
        deserializeJson(devicesData, devices);
        devicesMsg["data"] = devicesData;
        String devicesOutput;
        serializeJson(devicesMsg, devicesOutput);
        client->text(devicesOutput);

    } else if (type == WS_EVT_DISCONNECT) {
        DBG_OUTPUT_PORT.printf("WebSocket client #%lu disconnected\n", (unsigned long)client->id());

        // Release any device lock held by this client
        uint32_t clientId = client->id();
        if (lockMgr.hasClientLock(clientId)) {
            uint8_t nodeId = lockMgr.getClientDevice(clientId);
            lockMgr.releaseClientLocks(clientId);

            // Notify other clients that the device is now available
            JsonDocument doc;
            doc["event"] = "deviceUnlocked";
            doc["data"]["nodeId"] = nodeId;
            String output;
            serializeJson(doc, output);
            ws.textAll(output);
        }

    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0; // null terminate
            String message = (char*)data;

            DBG_OUTPUT_PORT.printf("WebSocket message: %s\n", message.c_str());

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, message);
            if (error) {
                DBG_OUTPUT_PORT.printf("JSON parse error: %s\n", error.c_str());
                return;
            }

            // Dispatch to WebSocket handler
            dispatchWebSocketMessage(client, doc);
        }
    }
}
