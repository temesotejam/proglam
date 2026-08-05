#include "waypoint_apply.h"
#include <cmath>
#include <cstring>
namespace proposal_min {
WaypointApplyResult applyWaypointSet(WaypointSafetyState safety, const WaypointRequest& request, WaypointStore& store) {
  WaypointApplyResult result{}; result.revision=store.revision; result.activeIndex=store.activeIndex; result.count=store.count;
  if (safety != WaypointSafetyState::Disarmed) {
    result.reason = safety==WaypointSafetyState::Running ? WaypointApplyReason::Running : (safety==WaypointSafetyState::EStop ? WaypointApplyReason::Estop : WaypointApplyReason::State);
    return result;
  }
  if (request.revision == store.revision) { result.status=WaypointApplyStatus::Duplicate; result.reason=WaypointApplyReason::Revision; return result; }
  if (request.revision < store.revision) { result.reason=WaypointApplyReason::Revision; return result; }
  if (request.action != 1 || request.count == 0 || request.count > 16 || !std::isfinite(request.reachRadiusM) || request.reachRadiusM <= 0 || (request.count && request.points == nullptr)) { result.reason=request.count==0 ? WaypointApplyReason::Empty : WaypointApplyReason::Range; return result; }
  for (uint8_t i=0; i<request.count; ++i) if (!std::isfinite(request.points[i].latitudeDeg) || !std::isfinite(request.points[i].longitudeDeg) || request.points[i].latitudeDeg < -90 || request.points[i].latitudeDeg > 90 || request.points[i].longitudeDeg < -180 || request.points[i].longitudeDeg > 180) { result.reason=WaypointApplyReason::Range; return result; }
  WaypointStore next=store; next.requestId=request.requestId; next.revision=request.revision; next.count=request.count; next.activeIndex=0; next.reachRadiusM=request.reachRadiusM; std::memcpy(next.points,request.points,sizeof(WaypointGeo)*request.count); store=next;
  result.status=WaypointApplyStatus::Accepted; result.reason=WaypointApplyReason::None; result.revision=store.revision; result.activeIndex=store.activeIndex; result.count=store.count; result.changed=true; return result;
}
}  // namespace proposal_min