#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <boat_protocol.h>
#include "waypoint_apply.h"
#include "waypoint_handler.h"

namespace {
using Points=std::array<proposal_min::WaypointGeo,16>;
proposal_min::WaypointRequest request(uint32_t revision,uint8_t count,const Points& points,float radius=0.5f,uint8_t action=1){return {9,revision,action,count,radius,points.data()};}
bool equalStore(const proposal_min::WaypointStore&a,const proposal_min::WaypointStore&b){
  if(a.revision!=b.revision||a.requestId!=b.requestId||a.count!=b.count||a.activeIndex!=b.activeIndex||a.reachRadiusM!=b.reachRadiusM)return false;
  for(size_t i=0;i<16;++i) if(a.points[i].latitudeDeg!=b.points[i].latitudeDeg||a.points[i].longitudeDeg!=b.points[i].longitudeDeg)return false;
  return true;
}
boat::WaypointSetPayload wire(const Points& points,uint32_t revision=5,uint8_t count=1,float radius=0.5f,uint8_t action=1){boat::WaypointSetPayload value{};value.requestId=9;value.revision=revision;value.action=action;value.count=count;value.reachRadiusM=radius;for(size_t i=0;i<16;++i)value.points[i]={points[i].latitudeDeg,points[i].longitudeDeg};value.canonicalCrc=boat::canonicalCrc(&value,offsetof(boat::WaypointSetPayload,canonicalCrc));return value;}
struct Capture {uint32_t handlerCalls=0,ackCalls=0; boat::WaypointAckPayload ack{};};
bool sink(void* context,const boat::WaypointAckPayload& ack){auto& c=*static_cast<Capture*>(context);++c.ackCalls;c.ack=ack;return true;}
void verifyAck(const Capture& capture){
  assert(capture.ackCalls==1); assert(capture.ack.canonicalCrc==boat::canonicalCrc(&capture.ack,offsetof(boat::WaypointAckPayload,canonicalCrc)));
  uint8_t encoded[boat::kMaxEncoded]{}; boat::Header header{boat::kVersion,(uint8_t)boat::Type::WaypointAck,(uint16_t)sizeof(capture.ack),3,7,100,0}; const size_t n=boat::encode(header,reinterpret_cast<const uint8_t*>(&capture.ack),encoded,sizeof(encoded));
  boat::Decoder decoder; boat::Frame frame{}; bool got=false; for(size_t i=0;i<n;++i) got=decoder.feed(encoded[i],frame)||got;
  assert(got&&frame.header.type==(uint8_t)boat::Type::WaypointAck&&frame.header.length==16&&decoder.crcErrors==0&&decoder.cobsErrors==0&&decoder.lengthErrors==0);
  boat::WaypointAckPayload decoded{};std::memcpy(&decoded,frame.payload,sizeof(decoded));assert(std::memcmp(&decoded,&capture.ack,sizeof(decoded))==0);
}
void feedType66(const uint8_t* payload,uint16_t length,proposal_min::WaypointStore& store,Capture& capture,boat::Decoder& decoder){
  uint8_t encoded[boat::kMaxEncoded]{};boat::Header header{boat::kVersion,(uint8_t)boat::Type::WaypointSet,length,1,7,100,0};const size_t n=boat::encode(header,payload,encoded,sizeof(encoded));assert(n>0);boat::Frame frame{};for(size_t i=0;i<n;++i)if(decoder.feed(encoded[i],frame)){++capture.handlerCalls;proposal_min::handleWaypointSetFrame(frame.payload,frame.header.length,proposal_min::WaypointSafetyState::Disarmed,store,{sink,&capture});}
}
}
int main(){
  Points base{};for(size_t i=0;i<base.size();++i){base[i]={35.0+i*0.0001,139.0+i*0.0001};}
  for(auto state:{proposal_min::WaypointSafetyState::Boot,proposal_min::WaypointSafetyState::ArmedIdle,proposal_min::WaypointSafetyState::Running,proposal_min::WaypointSafetyState::EStop,proposal_min::WaypointSafetyState::Fault}){proposal_min::WaypointStore store{};store.revision=4;store.count=1;store.points[0]=base[0];auto before=store;auto result=proposal_min::applyWaypointSet(state,request(5,1,base),store);assert(result.status==proposal_min::WaypointApplyStatus::Rejected&&equalStore(store,before));}
  proposal_min::WaypointStore store{};assert(proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(1,1,base),store).status==proposal_min::WaypointApplyStatus::Accepted);assert(proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(2,16,base),store).status==proposal_min::WaypointApplyStatus::Accepted);
  const auto stable=store; auto reject=[&](const proposal_min::WaypointRequest&r,proposal_min::WaypointApplyReason reason){auto x=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,r,store);assert(x.status==proposal_min::WaypointApplyStatus::Rejected&&x.reason==reason&&equalStore(store,stable));};
  reject(request(3,0,base),proposal_min::WaypointApplyReason::Empty);reject(request(3,17,base),proposal_min::WaypointApplyReason::Range);Points p=base;p[0].latitudeDeg=-90;assert(proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,1,p),store).status==proposal_min::WaypointApplyStatus::Accepted);store=stable;p=base;p[0].latitudeDeg=90;assert(proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,1,p),store).status==proposal_min::WaypointApplyStatus::Accepted);store=stable;
  p=base;p[0].latitudeDeg=91;reject(request(3,1,p),proposal_min::WaypointApplyReason::Range);p=base;p[0].longitudeDeg=-180;assert(proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,1,p),store).status==proposal_min::WaypointApplyStatus::Accepted);store=stable;p=base;p[0].longitudeDeg=180;assert(proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(3,1,p),store).status==proposal_min::WaypointApplyStatus::Accepted);store=stable;
  p=base;p[0].longitudeDeg=181;reject(request(3,1,p),proposal_min::WaypointApplyReason::Range);p=base;p[0].latitudeDeg=NAN;reject(request(3,1,p),proposal_min::WaypointApplyReason::Range);p=base;p[0].latitudeDeg=INFINITY;reject(request(3,1,p),proposal_min::WaypointApplyReason::Range);p=base;p[0].longitudeDeg=NAN;reject(request(3,1,p),proposal_min::WaypointApplyReason::Range);p=base;p[0].longitudeDeg=INFINITY;reject(request(3,1,p),proposal_min::WaypointApplyReason::Range);
  reject(request(3,1,base,0),proposal_min::WaypointApplyReason::Range);reject(request(3,1,base,-1),proposal_min::WaypointApplyReason::Range);reject(request(3,1,base,NAN),proposal_min::WaypointApplyReason::Range);reject(request(3,1,base,INFINITY),proposal_min::WaypointApplyReason::Range);reject(request(3,1,base,0.5f,2),proposal_min::WaypointApplyReason::Range);
  auto duplicate=proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed,request(2,1,base),store);assert(duplicate.status==proposal_min::WaypointApplyStatus::Duplicate&&equalStore(store,stable));reject(request(1,1,base),proposal_min::WaypointApplyReason::Revision);
  Points good=base;boat::WaypointSetPayload valid=wire(good);std::array<uint8_t,sizeof(valid)+1> bytes{};std::memcpy(bytes.data(),&valid,sizeof(valid));
  for(uint16_t length:std::array<uint16_t,8>{0,1,4,7,8,(uint16_t)(sizeof(valid)-1),(uint16_t)sizeof(valid),(uint16_t)(sizeof(valid)+1)}){proposal_min::WaypointStore lengthStore=stable;Capture capture{};boat::Decoder decoder;feedType66(bytes.data(),length,lengthStore,capture,decoder);assert(capture.handlerCalls==1&&capture.ackCalls==1&&decoder.crcErrors==0&&decoder.cobsErrors==0&&decoder.lengthErrors==0);verifyAck(capture);assert(capture.ack.requestId==(length>=4?9:0)&&capture.ack.revision==(length>=8?5:0));if(length==sizeof(valid))assert(capture.ack.status==(uint8_t)proposal_min::WaypointApplyStatus::Accepted);else assert(capture.ack.reason==(uint8_t)proposal_min::WaypointApplyReason::Length);}
  proposal_min::WaypointStore handlerStore{};Capture accepted{};boat::Decoder acceptedDecoder;feedType66(reinterpret_cast<const uint8_t*>(&valid),sizeof(valid),handlerStore,accepted,acceptedDecoder);assert(accepted.handlerCalls==1&&accepted.ackCalls==1&&accepted.ack.status==0&&accepted.ack.reason==0&&accepted.ack.count==1);verifyAck(accepted);
  auto atomicBefore=handlerStore;boat::WaypointSetPayload reverse=valid;reverse.revision=4;reverse.canonicalCrc=boat::canonicalCrc(&reverse,offsetof(boat::WaypointSetPayload,canonicalCrc));Capture rejected{};boat::Decoder rejectedDecoder;feedType66(reinterpret_cast<const uint8_t*>(&reverse),sizeof(reverse),handlerStore,rejected,rejectedDecoder);assert(rejected.ack.status==1&&rejected.ack.reason==(uint8_t)proposal_min::WaypointApplyReason::Revision&&equalStore(handlerStore,atomicBefore));verifyAck(rejected);
  boat::WaypointSetPayload badCrc=valid;badCrc.canonicalCrc^=1;Capture canonical{};boat::Decoder canonicalDecoder;feedType66(reinterpret_cast<const uint8_t*>(&badCrc),sizeof(badCrc),handlerStore,canonical,canonicalDecoder);assert(canonical.handlerCalls==1&&canonical.ackCalls==1&&canonical.ack.reason==(uint8_t)proposal_min::WaypointApplyReason::Crc);verifyAck(canonical);
  boat::WaypointSetPayload content=valid;content.revision=6;content.count=0;content.canonicalCrc=boat::canonicalCrc(&content,offsetof(boat::WaypointSetPayload,canonicalCrc));Capture invalidContent{};boat::Decoder contentDecoder;feedType66(reinterpret_cast<const uint8_t*>(&content),sizeof(content),handlerStore,invalidContent,contentDecoder);assert(invalidContent.handlerCalls==1&&invalidContent.ackCalls==1&&invalidContent.ack.reason==(uint8_t)proposal_min::WaypointApplyReason::Empty);verifyAck(invalidContent);
  uint8_t encoded[boat::kMaxEncoded]{};boat::Header header{boat::kVersion,(uint8_t)boat::Type::WaypointSet,(uint16_t)sizeof(valid),1,7,100,0};size_t n=boat::encode(header,reinterpret_cast<const uint8_t*>(&valid),encoded,sizeof(encoded));std::array<uint8_t,boat::kMaxEncoded> crc{};std::memcpy(crc.data(),encoded,n);crc[n-2]^=1;Capture crcCapture{};boat::Decoder crcDecoder;boat::Frame frame{};for(size_t i=0;i<n;++i)if(crcDecoder.feed(crc[i],frame)){++crcCapture.handlerCalls;}assert(crcDecoder.crcErrors==1&&crcDecoder.cobsErrors==0&&crcDecoder.lengthErrors==0&&crcCapture.handlerCalls==0&&crcCapture.ackCalls==0);
  std::array<uint8_t,boat::kMaxEncoded> cobs{};std::memcpy(cobs.data(),encoded,n);cobs[0]=0xFE;Capture cobsCapture{};boat::Decoder cobsDecoder;for(size_t i=0;i<n;++i)if(cobsDecoder.feed(cobs[i],frame)){++cobsCapture.handlerCalls;}assert(cobsDecoder.cobsErrors==1&&cobsDecoder.crcErrors==0&&cobsDecoder.lengthErrors==0&&cobsCapture.handlerCalls==0&&cobsCapture.ackCalls==0);
  std::cout<<"WAYPOINT_APPLY_HOST_PASS states=6 apply=covered handler=covered ack_wire=16 atomicity=fieldwise crc_only=1 cobs_only=1\n";
}
