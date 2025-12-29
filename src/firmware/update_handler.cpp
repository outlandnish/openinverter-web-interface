/*
 * This file is part of the openinverter web interface
 *
 * Copyright (C) 2025 Nishanth Samala <contact@outlandnish.com>
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
#include "update_handler.h"
#include <LittleFS.h>
#include "models/can_types.h"
#include "utils/can_utils.h"
#include "utils/can_queue.h"

#define DBG_OUTPUT_PORT Serial

FirmwareUpdateHandler::FirmwareUpdateHandler() {
  // Constructor
}

FirmwareUpdateHandler& FirmwareUpdateHandler::instance() {
  static FirmwareUpdateHandler instance;
  return instance;
}

int FirmwareUpdateHandler::startUpdate(const String& fileName, uint8_t nodeId) {
  updateFile = LittleFS.open(fileName, "r");
  currentPage = 0;
  this->nodeId = nodeId;

  // Set state BEFORE reset so we catch the bootloader's magic response
  // Note: The caller must reset the device after calling this function
  state = SEND_MAGIC;

  DBG_OUTPUT_PORT.println("Waiting for device to enter bootloader mode...");
  DBG_OUTPUT_PORT.println("Starting Update");

  totalPages = (updateFile.size() + PAGE_SIZE_BYTES - 1) / PAGE_SIZE_BYTES;
  return totalPages;
}

void FirmwareUpdateHandler::processResponse(const twai_message_t* rxframe) {
  switch (state) {
    case SEND_MAGIC:
      handleMagicResponse(rxframe);
      break;
    case SEND_SIZE:
      handleSizeResponse(rxframe);
      break;
    case SEND_PAGE:
      handlePageResponse(rxframe);
      break;
    case CHECK_CRC:
      handleCrcResponse(rxframe);
      break;
    case REQUEST_JSON:
      // Do not exit this state
      break;
    case UPD_IDLE:
      // Do not exit this state
      break;
  }
}

void FirmwareUpdateHandler::handleMagicResponse(const twai_message_t* rxframe) {
  if (rxframe->data[0] == 0x33) {
    twai_message_t tx_frame;
    tx_frame.extd = false;
    tx_frame.identifier = BOOTLOADER_COMMAND_ID;
    tx_frame.data_length_code = 4;

    // For now just reflect ID
    tx_frame.data[0] = rxframe->data[4];
    tx_frame.data[1] = rxframe->data[5];
    tx_frame.data[2] = rxframe->data[6];
    tx_frame.data[3] = rxframe->data[7];

    state = SEND_SIZE;
    DBG_OUTPUT_PORT.printf("Sending ID %" PRIu32 "\r\n", *(uint32_t*)tx_frame.data);

    sendFrame(tx_frame);

    if (rxframe->data[1] < 1) { // Bootloader with timing quirk, wait 100 ms
      delay(100);
    }
  }
}

void FirmwareUpdateHandler::handleSizeResponse(const twai_message_t* rxframe) {
  if (rxframe->data[0] == 'S') {
    twai_message_t tx_frame;
    tx_frame.extd = false;
    tx_frame.identifier = BOOTLOADER_COMMAND_ID;
    tx_frame.data_length_code = 1;

    tx_frame.data[0] = totalPages;
    state = SEND_PAGE;
    crc = 0xFFFFFFFF;
    currentByte = 0;
    currentPage = 0;

    DBG_OUTPUT_PORT.printf("Sending size %u\r\n", tx_frame.data[0]);

    sendFrame(tx_frame);
  }
}

void FirmwareUpdateHandler::handlePageResponse(const twai_message_t* rxframe) {
  if (rxframe->data[0] == 'P') {
    char buffer[8];
    size_t bytesRead = 0;

    if (currentByte < updateFile.size()) {
      updateFile.seek(currentByte);
      bytesRead = updateFile.readBytes(buffer, sizeof(buffer));
    }

    while (bytesRead < 8) {
      buffer[bytesRead++] = 0xff;
    }

    currentByte += bytesRead;
    crc = crc32_word(crc, *(uint32_t*)&buffer[0]);
    crc = crc32_word(crc, *(uint32_t*)&buffer[4]);

    twai_message_t tx_frame;
    tx_frame.extd = false;
    tx_frame.identifier = BOOTLOADER_COMMAND_ID;
    tx_frame.data_length_code = 8;
    tx_frame.data[0] = buffer[0];
    tx_frame.data[1] = buffer[1];
    tx_frame.data[2] = buffer[2];
    tx_frame.data[3] = buffer[3];
    tx_frame.data[4] = buffer[4];
    tx_frame.data[5] = buffer[5];
    tx_frame.data[6] = buffer[6];
    tx_frame.data[7] = buffer[7];

    state = SEND_PAGE;
    sendFrame(tx_frame);
  }
  else if (rxframe->data[0] == 'C') {
    twai_message_t tx_frame;
    tx_frame.extd = false;
    tx_frame.identifier = BOOTLOADER_COMMAND_ID;
    tx_frame.data_length_code = 4;
    tx_frame.data[0] = crc & 0xFF;
    tx_frame.data[1] = (crc >> 8) & 0xFF;
    tx_frame.data[2] = (crc >> 16) & 0xFF;
    tx_frame.data[3] = (crc >> 24) & 0xFF;

    state = CHECK_CRC;
    sendFrame(tx_frame);
  }
}

void FirmwareUpdateHandler::handleCrcResponse(const twai_message_t* rxframe) {
  crc = 0xFFFFFFFF;
  DBG_OUTPUT_PORT.printf("Sent bytes %u-%u... ", currentPage * PAGE_SIZE_BYTES, currentByte);

  if (rxframe->data[0] == 'P') {
    state = SEND_PAGE;
    currentPage++;
    DBG_OUTPUT_PORT.printf("CRC Good\r\n");
    processResponse(rxframe);
  }
  else if (rxframe->data[0] == 'E') {
    state = SEND_PAGE;
    currentByte = currentPage * PAGE_SIZE_BYTES;
    DBG_OUTPUT_PORT.printf("CRC Error\r\n");
    processResponse(rxframe);
  }
  else if (rxframe->data[0] == 'D') {
    state = REQUEST_JSON;
    updateFile.close();
    DBG_OUTPUT_PORT.printf("Done!\r\n");
  }
}

void FirmwareUpdateHandler::sendFrame(const twai_message_t& frame) {
  canQueueTransmit(&frame, pdMS_TO_TICKS(10));
  printCanTx(&frame);
}

bool FirmwareUpdateHandler::isInProgress() const {
  return state != UPD_IDLE;
}

int FirmwareUpdateHandler::getCurrentPage() const {
  return currentPage;
}

int FirmwareUpdateHandler::getTotalPages() const {
  return totalPages;
}

FirmwareUpdateHandler::State FirmwareUpdateHandler::getState() const {
  return state;
}

void FirmwareUpdateHandler::reset() {
  state = UPD_IDLE;
  if (updateFile) {
    updateFile.close();
  }
  currentPage = 0;
  totalPages = 0;
  crc = 0xFFFFFFFF;
  currentByte = 0;
}
