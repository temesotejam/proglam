#pragma once
#include <stdint.h>
namespace proposal_min::state_reason_generated {
constexpr uint8_t kStateDisarmed=0,kStateRunning=1,kStateEStop=2,kStateFault=3;
constexpr uint8_t kReasonNone=0,kReasonStop=1,kReasonEStop=2,kReasonHeartbeatTimeout=3,kReasonGnssInvalid=4,kReasonGnssStale=5,kReasonImuInvalid=6,kReasonImuStale=7,kReasonTofInvalid=8,kReasonTofStale=9,kReasonNonfinite=10,kReasonVescFault=11;
constexpr uint8_t kAllowedTransitions[][2] = {{0,0},{0,1},{0,2},{0,3},{1,0},{1,1},{1,2},{1,3},{2,0},{2,2},{2,3},{3,0},{3,3}};
constexpr uint8_t kFaultReasons[] = {3,4,5,6,7,8,9,10,11};
}
