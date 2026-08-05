#pragma once
#include <stddef.h>
#include <stdint.h>
#include "waypoint_apply.h"
#include <boat_protocol.h>
namespace proposal_min {
struct WaypointAckSink { bool (*send)(void* context, const boat::WaypointAckPayload& ack) = nullptr; void* context = nullptr; };
struct WaypointHandlerResult { WaypointApplyResult applied{}; boat::WaypointAckPayload ack{}; bool ackSent = false; };
WaypointHandlerResult handleWaypointSetFrame(const uint8_t* payload, size_t length, WaypointSafetyState state, WaypointStore& store, const WaypointAckSink& sink);
}
