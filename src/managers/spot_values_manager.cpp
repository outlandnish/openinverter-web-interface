#include "spot_values_manager.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "../main.h"
#include "../models/can_event.h"
#include "../oi_can.h"
#include "device_connection.h"

SpotValuesManager& SpotValuesManager::instance() {
  static SpotValuesManager instance;
  return instance;
}

SpotValuesManager::SpotValuesManager() {}

void SpotValuesManager::setParamIds(const std::vector<int>& paramIds) {
  paramIds_ = paramIds;
}

void SpotValuesManager::start(uint32_t intervalMs, const int* paramIds, int paramCount) {
  interval_ = intervalMs;
  paramIds_.clear();
  for (int i = 0; i < paramCount; i++) {
    paramIds_.push_back(paramIds[i]);
  }
  lastCollectionTime_ = millis();
  reloadQueue();
}

void SpotValuesManager::stop() {
  // Flush any remaining batched values before stopping
  flushBatch();

  paramIds_.clear();
  requestQueue_.clear();
  latestValues_.clear();
  batch_.clear();
}

void SpotValuesManager::processQueue() {
  // Try to send one request from queue (if rate limit allows)
  // NOTE: Does NOT consume responses - responses are routed by CAN task via handleResponse()
  if (!requestQueue_.empty()) {
    int paramId = requestQueue_.front();

    // Try to send request (non-blocking, respects rate limit)
    if (OICan::RequestValue(paramId)) {
      // Request sent successfully, remove from queue
      requestQueue_.pop_front();
    }
    // If request failed (rate limit or TX queue full), leave it in queue and try next iteration
  }
}

bool SpotValuesManager::isWaitingForParam(int paramId) const {
  if (!isActive()) {
    return false;
  }
  // Check if this paramId is one we're monitoring
  for (int id : paramIds_) {
    if (id == paramId) {
      return true;
    }
  }
  return false;
}

void SpotValuesManager::handleResponse(int paramId, double value) {
  // Add to batch (map auto-replaces if param already exists)
  batch_[paramId] = value;
  // Also update persistent cache for getParamValues
  latestValues_[paramId] = value;
}

void SpotValuesManager::reloadQueue() {
  if (!DeviceConnection::instance().isIdle()) {
    return;
  }

  // Flush any accumulated values from previous cycle at user-requested interval
  flushBatch();

  // Clear existing queue and reload with all parameters
  requestQueue_.clear();
  for (int paramId : paramIds_) {
    requestQueue_.push_back(paramId);
  }
}

void SpotValuesManager::flushBatch() {
  if (batch_.empty()) {
    return;
  }

  // Build event with all accumulated values
  CANEvent evt;
  evt.type = EVT_SPOT_VALUES;
  evt.data.spotValues.timestamp = millis();

  // Build JSON string with all batched values
  JsonDocument doc;
  for (const auto& pair : batch_) {
    doc[String(pair.first)] = pair.second;
  }
  serializeJson(doc, evt.data.spotValues.valuesJson);

  xQueueSend(canEventQueue, &evt, 0);

  // Clear the batch
  batch_.clear();
}
