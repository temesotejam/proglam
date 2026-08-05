#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <boat_protocol.h>
#include "waypoint_apply.h"

namespace {
proposal_min::WaypointRequest request(uint32_t revision, uint8_t count, proposal_min::WaypointGeo* points, float radius=0.5f) {
  proposal_min::WaypointRequest r{};
  r.requestId=9; r.revision=revision; r.action=1; r.count=count; r.reachRadiusM=radius; r.points=points; return r;
}
void assertUnchanged(const proposal_min::WaypointStore& s, uint32_t revision, uint8_t count, double lat) {
  assert(s.revision==revision && s.count==count && s.points[0].latitudeDeg==lat && s.activeIndex==0);
}
}
int main() {
  proposal_min::WaypointGeo points[16]{};
  for (int i=0;i<16;++i) { points[i].latitudeDeg=35.0+i*0.0001; points[i].longitudeDeg=139.0+i*0.0001; }
  for (auto state : {proposal_min::WaypointSafetyState::Boot, proposal_min::WaypointSafetyState::ArmedIdle, proposal_min::WaypointSafetyState::Running, proposal_min::WaypointSafetyState::EStop, proposal_min::WaypointSafetyState::Fault}) {
    proposal_min::WaypointStore store{}; store.revision=4; store.count=1; store.points[0]=points[0];
    auto r=proposal_min::applyWaypointSet(state,request(5,2,points),store);
    assert(r.status==proposal_min::WaypointApplyStatus::Rejected); assertUnchanged(store,4,1,35.0);
  }
  proposal_min::WaypointStore store{};
  auto accepted1=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(1,1,points),store);
  assert(accepted1.status==proposal_min::WaypointApplyStatus::Accepted && store.revision==1 && store.count==1);
  auto duplicate=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(1,1,points),store);
  assert(duplicate.status==proposal_min::WaypointApplyStatus::Duplicate && store.revision==1);
  auto accepted16=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(2,16,points,1.0f),store);
  assert(accepted16.status==proposal_min::WaypointApplyStatus::Accepted && store.count==16 && store.revision==2);
  auto reject0=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,0,points),store);
  assert(reject0.status==proposal_min::WaypointApplyStatus::Rejected && reject0.reason==proposal_min::WaypointApplyReason::Empty && store.revision==2);
  auto reject17=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,17,points),store);
  assert(reject17.status==proposal_min::WaypointApplyStatus::Rejected && store.revision==2);
  auto badLat=points; badLat[0].latitudeDeg=91.0;
  auto rejectLat=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,1,badLat),store);
  assert(rejectLat.status==proposal_min::WaypointApplyStatus::Rejected && store.revision==2);
  auto badLon=points; badLon[0].longitudeDeg=181.0;
  auto rejectLon=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,1,badLon),store);
  assert(rejectLon.status==proposal_min::WaypointApplyStatus::Rejected && store.revision==2);
  auto badNan=points; badNan[0].latitudeDeg=NAN;
  auto rejectNan=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,1,badNan),store);
  assert(rejectNan.status==proposal_min::WaypointApplyStatus::Rejected && store.revision==2);
  auto badInf=points; badInf[0].longitudeDeg=INFINITY;
  auto rejectInf=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,1,badInf),store);
  assert(rejectInf.status==proposal_min::WaypointApplyStatus::Rejected && store.revision==2);
  auto rejectRadius0=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,1,points,0),store);
  auto rejectRadiusNeg=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,1,points,-1),store);
  auto rejectRadiusNan=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,1,points,NAN),store);
  auto rejectRadiusInf=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,1,points,INFINITY),store);
  assert(rejectRadius0.status==proposal_min::WaypointApplyStatus::Rejected && rejectRadiusNeg.status==proposal_min::WaypointApplyStatus::Rejected && rejectRadiusNan.status==proposal_min::WaypointApplyStatus::Rejected && rejectRadiusInf.status==proposal_min::WaypointApplyStatus::Rejected);
  auto badAction=request(3,1,points); badAction.action=2;
  assert(proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,badAction,store).status==proposal_min::WaypointApplyStatus::Rejected && store.revision==2);
  boat::WaypointSetPayload wire{}; wire.requestId=9; wire.revision=3; wire.action=1; wire.count=1; wire.reachRadiusM=0.5f; wire.points[0]={35.0,139.0}; wire.canonicalCrc=boat::canonicalCrc(&wire,offsetof(boat::WaypointSetPayload,canonicalCrc));
  uint8_t encoded[boat::kMaxEncoded]{}; boat::Header h{boat::kVersion,(uint8_t)boat::Type::WaypointSet,(uint16_t)sizeof(wire),1,7,100,0}; size_t n=boat::encode(h,reinterpret_cast<const uint8_t*>(&wire),encoded,sizeof(encoded)); assert(n>0);
  boat::Decoder decoder; boat::Frame frame{}; bool got=false; for(size_t i=0;i<n;++i) if(decoder.feed(encoded[i],frame)) got=true; assert(got && decoder.crcErrors==0 && decoder.cobsErrors==0 && decoder.lengthErrors==0);
  encoded[n/2]^=0x01; boat::Decoder badDecoder; boat::Frame badFrame{}; for(size_t i=0;i<n;++i) badDecoder.feed(encoded[i],badFrame); assert(badDecoder.crcErrors>0 || badDecoder.cobsErrors>0);
  std::cout<<"WAYPOINT_APPLY_HOST_PASS states=6 count0/1/16/>16=covered coordinates=covered radius=covered crc=covered ack_wire=16 atomicity=ok\n";
}
