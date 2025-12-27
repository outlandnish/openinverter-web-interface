#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// CAN processing task - runs independently on separate core
void canTask(void* parameter);
