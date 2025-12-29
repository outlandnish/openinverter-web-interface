#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/twai.h"
#include "can_task.h"

// Queue-based CAN I/O functions for SDO protocol layer
// These replace direct twai_transmit/twai_receive calls

/**
 * Transmit a CAN frame via the TX queue
 * @param frame Pointer to the frame to transmit
 * @param timeout Ticks to wait if queue is full
 * @return true if frame was queued successfully
 */
inline bool canQueueTransmit(const twai_message_t* frame, TickType_t timeout = pdMS_TO_TICKS(10)) {
    if (canTxQueue == nullptr) {
        return false;
    }
    return xQueueSend(canTxQueue, frame, timeout) == pdTRUE;
}

/**
 * Receive an SDO response from the response queue
 * @param frame Pointer to store received frame
 * @param timeout Ticks to wait for a response
 * @return true if a frame was received
 */
inline bool canQueueReceive(twai_message_t* frame, TickType_t timeout = pdMS_TO_TICKS(10)) {
    if (sdoResponseQueue == nullptr) {
        return false;
    }
    return xQueueReceive(sdoResponseQueue, frame, timeout) == pdTRUE;
}

/**
 * Clear any pending responses from the SDO response queue
 * Useful before starting a new request sequence
 */
inline void canQueueClearResponses() {
    if (sdoResponseQueue == nullptr) {
        return;
    }
    twai_message_t frame;
    while (xQueueReceive(sdoResponseQueue, &frame, 0) == pdTRUE) {
        // Discard
    }
}
