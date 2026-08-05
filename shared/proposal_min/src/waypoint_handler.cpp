#include "waypoint_handler.h"
#include <string.h>
namespace proposal_min {
namespace {
void requestIdentity(const uint8_t* payload, size_t length, uint32_t& requestId, uint32_t& revision) {
  requestId = revision = 0;
  if (payload && length >= sizeof(uint32_t) * 2) { memcpy(&requestId, payload, sizeof(requestId)); memcpy(&revision, payload + sizeof(requestId), sizeof(revision)); }
}
void sendAck(WaypointHandlerResult& result, const WaypointAckSink& sink) { result.ackSent = sink.send ? sink.send(sink.context, result.ack) : false; }
}
WaypointHandlerResult handleWaypointSetFrame(const uint8_t* payload, size_t length, WaypointSafetyState state, WaypointStore& store, const WaypointAckSink& sink) {
  WaypointHandlerResult result{}; uint32_t requestId = 0, revision = 0; requestIdentity(payload, length, requestId, revision);
  result.ack.requestId = requestId; result.ack.revision = revision; result.ack.status = static_cast<uint8_t>(WaypointApplyStatus::Rejected); result.ack.reason = static_cast<uint8_t>(WaypointApplyReason::Length); result.ack.activeIndex = store.activeIndex; result.ack.count = store.count;
  if (!payload || length != sizeof(boat::WaypointSetPayload)) { result.applied.revision=store.revision; result.applied.activeIndex=store.activeIndex; result.applied.count=store.count; result.applied.reason=WaypointApplyReason::Length; result.ack.canonicalCrc=boat::canonicalCrc(&result.ack,offsetof(boat::WaypointAckPayload,canonicalCrc)); sendAck(result,sink); return result; }
  boat::WaypointSetPayload wire{}; memcpy(&wire,payload,sizeof(wire)); result.ack.requestId=wire.requestId; result.ack.revision=wire.revision;
  if (wire.canonicalCrc != boat::canonicalCrc(&wire,offsetof(boat::WaypointSetPayload,canonicalCrc))) { result.applied.revision=store.revision; result.applied.activeIndex=store.activeIndex; result.applied.count=store.count; result.applied.reason=WaypointApplyReason::Crc; result.ack.reason=static_cast<uint8_t>(WaypointApplyReason::Crc); result.ack.canonicalCrc=boat::canonicalCrc(&result.ack,offsetof(boat::WaypointAckPayload,canonicalCrc)); sendAck(result,sink); return result; }
  WaypointGeo points[16]{}; for(uint8_t i=0;i<wire.count&&i<16;++i){points[i].latitudeDeg=wire.points[i].latitudeDeg;points[i].longitudeDeg=wire.points[i].longitudeDeg;}
  WaypointRequest request{}; request.requestId=wire.requestId; request.revision=wire.revision; request.action=wire.action; request.count=wire.count; request.reachRadiusM=wire.reachRadiusM; request.points=points; result.applied=applyWaypointSet(state,request,store);
  result.ack.status=static_cast<uint8_t>(result.applied.status); result.ack.reason=static_cast<uint8_t>(result.applied.reason); result.ack.activeIndex=result.applied.activeIndex; result.ack.count=result.applied.count; result.ack.canonicalCrc=boat::canonicalCrc(&result.ack,offsetof(boat::WaypointAckPayload,canonicalCrc)); sendAck(result,sink); return result;
}
}
