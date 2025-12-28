/*
 * This file is part of the esp32 web interface
 *
 * Copyright (C) 2023 Johannes Huebner <dev@johanneshuebner.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#pragma once

#include "driver/twai.h"
#include <FS.h>
#include <Arduino.h>

class FirmwareUpdateHandler {
public:
  enum State {
    UPD_IDLE,
    SEND_MAGIC,
    SEND_SIZE,
    SEND_PAGE,
    CHECK_CRC,
    REQUEST_JSON
  };

  // Get singleton instance
  static FirmwareUpdateHandler& instance();

  // Start firmware update
  int startUpdate(const String& fileName, uint8_t nodeId);

  // Process incoming CAN response frame
  void processResponse(const twai_message_t* rxframe);

  // Status queries
  bool isInProgress() const;
  int getCurrentPage() const;
  int getTotalPages() const;
  State getState() const;

  // Reset to idle state
  void reset();

private:
  // Private constructor for singleton
  FirmwareUpdateHandler();

  // Disable copy and assignment
  FirmwareUpdateHandler(const FirmwareUpdateHandler&) = delete;
  FirmwareUpdateHandler& operator=(const FirmwareUpdateHandler&) = delete;

  // State machine
  State state = UPD_IDLE;
  File updateFile;
  int currentPage = 0;
  int totalPages = 0;
  uint32_t crc = 0xFFFFFFFF;
  int currentByte = 0;
  uint8_t nodeId = 0;

  // Constants
  static const size_t PAGE_SIZE_BYTES = 1024;

  // State handlers
  void handleMagicResponse(const twai_message_t* rxframe);
  void handleSizeResponse(const twai_message_t* rxframe);
  void handlePageResponse(const twai_message_t* rxframe);
  void handleCrcResponse(const twai_message_t* rxframe);

  // Helper to send CAN frame
  void sendFrame(const twai_message_t& frame);
};
