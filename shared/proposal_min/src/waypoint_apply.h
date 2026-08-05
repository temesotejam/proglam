#pragma once
#include <stdint.h>
#include <stddef.h>

namespace proposal_min {

enum class WaypointSafetyState : uint8_t { Boot=0, Disarmed=1, ArmedIdle=2, Running=3, EStop=4, Fault=5 };
enum class WaypointApplyStatus : uint8_t { Accepted=0, Rejected=1, Duplicate=2 };
enum class WaypointApplyReason : uint8_t { None=0, Running=1, Estop=2, Length=3, Crc=4, Range=5, Empty=6, Revision=7, State=8 };
struct WaypointGeo { double latitudeDeg=0, longitudeDeg=0; };
struct WaypointStore { uint32_t revision=0, requestId=0; uint8_t count=0, activeIndex=0; float reachRadiusM=0.5f; WaypointGeo points[16]{}; };
struct WaypointRequest { uint32_t requestId=0, revision=0; uint8_t action=0, count=0; float reachRadiusM=0; const WaypointGeo* points=nullptr; };
struct WaypointApplyResult { WaypointApplyStatus status=WaypointApplyStatus::Rejected; WaypointApplyReason reason=WaypointApplyReason::State; uint32_t revision=0; uint8_t activeIndex=0, count=0; bool changed=false; };
WaypointApplyResult applyWaypointSet(WaypointSafetyState safety, const WaypointRequest& request, WaypointStore& store);

}  // namespace proposal_min