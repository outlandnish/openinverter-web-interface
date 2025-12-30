#include "can_hardware.h"

namespace CanHardware {

void initAllTransceiverPins() {
#ifdef CAN0_SHUTDOWN_PIN
  initTransceiverPin(CAN0_SHUTDOWN_PIN, "CAN0 shutdown");
#endif

#ifdef CAN0_STANDBY_PIN
  initTransceiverPin(CAN0_STANDBY_PIN, "CAN0 standby");
#endif

#ifdef CAN1_SHUTDOWN_PIN
  initTransceiverPin(CAN1_SHUTDOWN_PIN, "CAN1 shutdown");
#endif

#ifdef CAN1_STANDBY_PIN
  initTransceiverPin(CAN1_STANDBY_PIN, "CAN1 standby");
#endif
}

}  // namespace CanHardware
