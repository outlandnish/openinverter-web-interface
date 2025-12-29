#include <WiFi.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "oi_can.h"
#include "config.h"
#include "models/can_event.h"
#include "utils/string_utils.h"
#include "utils/can_hardware.h"
#include "managers/device_cache.h"
#include "managers/device_connection.h"
#include "managers/device_discovery.h"
#include "event_processor.h"
#include "main.h"
#include "can_task.h"
#include "websocket_handlers.h"
#include "http_handlers.h"
#include "wifi_setup.h"

// ============================================================================
// Global Variables
// ============================================================================

// FreeRTOS Queue handles
QueueHandle_t canCommandQueue = nullptr;
QueueHandle_t canEventQueue = nullptr;

const char* host = "inverter";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Config config;

// ============================================================================
// Setup
// ============================================================================

void setup(void) {
    DBG_OUTPUT_PORT.begin(115200);

    // Initialize status LED (NeoPixel)
    StatusLED::instance().begin();
    statusLEDOff();

    // Start SPI Flash file system
    LittleFS.begin(false, "/littlefs", 10, "littlefs");

    // WiFi initialization
    WiFiSetup::initialize();

    MDNS.begin(host);

    config.load();

    // Initialize CAN enable pin if configured
    if (config.getCanEnablePin() > 0) {
        pinMode(config.getCanEnablePin(), OUTPUT);
        digitalWrite(config.getCanEnablePin(), LOW);
    }

    // Initialize CAN transceiver shutdown and standby pins (platform-specific)
    CanHardware::initAllTransceiverPins();

    // Initialize CAN bus at startup
    DBG_OUTPUT_PORT.println("Initializing CAN bus...");
    OICan::InitCAN(config.getBaudRateEnum(), config.getCanTXPin(), config.getCanRXPin());

    // Create FreeRTOS queues
    canCommandQueue = xQueueCreate(10, sizeof(CANCommand));
    canEventQueue = xQueueCreate(20, sizeof(CANEvent));

    if (canCommandQueue == nullptr || canEventQueue == nullptr) {
        DBG_OUTPUT_PORT.println("ERROR: Failed to create queues!");
        return;
    }

    DBG_OUTPUT_PORT.println("Queues created successfully");

    // Setup device discovery callback to post events
    DeviceDiscovery::instance().setDiscoveryCallback([](uint8_t nodeId, const char* serial, uint32_t lastSeen) {
        CANEvent evt;
        evt.type = EVT_DEVICE_DISCOVERED;
        evt.data.deviceDiscovered.nodeId = nodeId;
        safeCopyString(evt.data.deviceDiscovered.serial, serial);
        evt.data.deviceDiscovered.lastSeen = lastSeen;
        evt.data.deviceDiscovered.name[0] = '\0';

        // Look up name from cache
        std::string name = DeviceCache::instance().getDeviceName(serial);
        if (!name.empty()) {
            safeCopyString(evt.data.deviceDiscovered.name, name.c_str());
        }

        xQueueSend(canEventQueue, &evt, 0);
    });

    // Setup scan progress callback to post events
    DeviceDiscovery::instance().setProgressCallback([](uint8_t currentNode, uint8_t startNode, uint8_t endNode) {
        CANEvent evt;
        evt.type = EVT_SCAN_PROGRESS;
        evt.data.scanProgress.currentNode = currentNode;
        evt.data.scanProgress.startNode = startNode;
        evt.data.scanProgress.endNode = endNode;
        xQueueSend(canEventQueue, &evt, 0);
    });

    // Setup connection ready callback to post events when device is truly connected
    DeviceConnection::instance().setConnectionReadyCallback([](uint8_t nodeId, const char* serial) {
        DBG_OUTPUT_PORT.printf("[Callback] Connection ready - node %d, serial %s\n", nodeId, serial);
        CANEvent evt;
        evt.type = EVT_CONNECTED;
        evt.data.connected.nodeId = nodeId;
        safeCopyString(evt.data.connected.serial, serial);
        xQueueSend(canEventQueue, &evt, 0);
    });

    // Note: JSON download progress callback removed - async download now uses
    // EVT_JSON_READY event for completion notification to specific client

    // Initialize CAN queues and spawn CAN task
    initCanQueues();
#if CONFIG_FREERTOS_UNICORE
    xTaskCreate(canTask, "CAN_Task", 8192, nullptr, 1, nullptr);
    DBG_OUTPUT_PORT.println("CAN task spawned (single-core mode)");
#else
    xTaskCreatePinnedToCore(canTask, "CAN_Task", 8192, nullptr, 1, nullptr, 0);
    DBG_OUTPUT_PORT.println("CAN task spawned on Core 0 (dual-core mode)");
#endif

    // WebSocket setup
    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);

    // Server initialization
    ArduinoOTA.setHostname(host);
    ArduinoOTA.begin();

    // Register all HTTP routes
    registerHttpRoutes(server);

    server.begin();

    MDNS.addService("http", "tcp", 80);
}

// ============================================================================
// Main Loop
// ============================================================================

void loop(void) {
    ws.cleanupClients();
    ArduinoOTA.handle();

    // Process events from CAN task and firmware progress
    EventProcessor::processEvents(ws);
    EventProcessor::processFirmwareProgress(ws);
}
