#include "can_utils.h"
#include "models/can_types.h"

// CRC-32 calculation for CAN operations (STM32 polynomial 0x04C11DB7)
// This matches the IEEE 802.3 / Ethernet CRC-32 polynomial
uint32_t crc32_word(uint32_t crc, uint32_t word) {
  const uint32_t polynomial = 0x04C11DB7;
  crc ^= word;
  for (int i = 0; i < 32; i++) {
    if (crc & 0x80000000) {
      crc = (crc << 1) ^ polynomial;
    } else {
      crc = crc << 1;
    }
  }
  return crc;
}

// Debug helper for transmitted CAN frames
// Currently disabled for production, but available for debugging
void printCanTx(const twai_message_t* frame) {
  // Debug output disabled
  // To enable: uncomment and use Serial.printf() or similar
  // Serial.printf("TX: ID=0x%03X Data=", frame->identifier);
  // for (int i = 0; i < frame->data_length_code; i++) {
  //   Serial.printf("%02X ", frame->data[i]);
  // }
  // Serial.println();
}

// Debug helper for received CAN frames
// Currently disabled for production, but available for debugging
void printCanRx(const twai_message_t* frame) {
  // Debug output disabled
  // To enable: uncomment and use Serial.printf() or similar
  // Serial.printf("RX: ID=0x%03X Data=", frame->identifier);
  // for (int i = 0; i < frame->data_length_code; i++) {
  //   Serial.printf("%02X ", frame->data[i]);
  // }
  // Serial.println();
}

// Validate that a CAN frame is a valid SDO response for the given node and index
bool isValidSdoResponse(const twai_message_t& frame, uint8_t nodeId, uint16_t index) {
  // Check if the CAN ID matches the expected SDO response ID for this node
  uint32_t expectedId = SDO_RESPONSE_BASE_ID + nodeId;
  if (frame.identifier != expectedId) {
    return false;
  }

  // Check if the index in the response matches what we requested
  uint16_t responseIndex = frame.data[1] | (frame.data[2] << 8);
  if (responseIndex != index) {
    return false;
  }

  return true;
}
