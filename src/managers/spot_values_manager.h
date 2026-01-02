#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <vector>

/**
 * Manages spot values streaming - collecting parameter values at regular intervals
 * and batching them for WebSocket broadcast.
 * Uses singleton pattern since only one spot values session is active at a time.
 */
class SpotValuesManager {
public:
  // Singleton access
  static SpotValuesManager& instance();

  // Configuration
  void setInterval(uint32_t intervalMs) { interval_ = intervalMs; }
  uint32_t getInterval() const { return interval_; }

  void setParamIds(const std::vector<int>& paramIds);
  const std::vector<int>& getParamIds() const { return paramIds_; }
  size_t getParamCount() const { return paramIds_.size(); }

  // State management
  bool isActive() const { return !paramIds_.empty(); }
  void start(uint32_t intervalMs, const int* paramIds, int paramCount);
  void stop();

  // Processing (called from CAN task)
  void processQueue();  // Send pending requests (does NOT consume responses)
  void reloadQueue();   // Reload request queue at interval boundaries
  void flushBatch();    // Send accumulated values to event queue

  // Response routing (called by CAN task when SDO response received)
  bool isWaitingForParam(int paramId) const;
  void handleResponse(int paramId, double value);

  // Batch access (for getParamValues to merge latest values)
  const std::map<int, double>& getLatestValues() const { return latestValues_; }

  // Time tracking
  uint32_t getLastCollectionTime() const { return lastCollectionTime_; }
  void updateLastCollectionTime(uint32_t time) { lastCollectionTime_ = time; }

private:
  SpotValuesManager();
  SpotValuesManager(const SpotValuesManager&) = delete;
  SpotValuesManager& operator=(const SpotValuesManager&) = delete;

  // Configuration
  std::vector<int> paramIds_;
  uint32_t interval_ = 1000;  // Default 1000ms

  // State
  uint32_t lastCollectionTime_ = 0;
  std::deque<int> requestQueue_;        // Queue of pending parameter requests
  std::map<int, double> batch_;         // Accumulated values for current cycle
  std::map<int, double> latestValues_;  // Persistent cache of latest values
};
