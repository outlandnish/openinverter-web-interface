#pragma once

#include <atomic>
#include <cstdint>

// Thread-safe request ID generator for matching async command/event pairs
class RequestIdGenerator {
public:
  static RequestIdGenerator& instance() {
    static RequestIdGenerator instance;
    return instance;
  }

  // Generate next unique request ID (thread-safe)
  uint32_t next() { return ++counter_; }

private:
  RequestIdGenerator() : counter_(0) {}
  RequestIdGenerator(const RequestIdGenerator&) = delete;
  RequestIdGenerator& operator=(const RequestIdGenerator&) = delete;

  std::atomic<uint32_t> counter_;
};

// Convenience function
inline uint32_t nextRequestId() {
  return RequestIdGenerator::instance().next();
}
