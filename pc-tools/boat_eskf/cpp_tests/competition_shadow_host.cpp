#include <cassert>
#include <cmath>
#include <cstdio>
#include "competition_shadow.h"
using namespace competition_shadow;

static SensorInput healthy(uint64_t now) {
  SensorInput x{}; x.nowUs=now; x.heartbeat=x.imuValid=x.tofValid=x.gnssValid=true;
  x.heartbeatUs=x.imuUs=x.tofUs=x.gnssUs=now; x.tofM=1.2f; return x;
}
static Output run(Controller& c, SensorInput x) { c.step(x,true,false,false,false); return c.step(x,true,false,false,false); }
int main() {
  Config cfg{}; cfg.slewPerStep=1.0f; cfg.autoPropulsion=.3f;
  Controller c(cfg); SensorInput x=healthy(1000000);
  assert(c.safety()==SafetyState::Disarmed && c.mode()==ControlMode::Manual);
  assert(c.setMode(ControlMode::Manual,1).ack==Ack::Accepted);
  assert(c.setMode(ControlMode::Manual,1).ack==Ack::Duplicate);
  ManualCommand manual{.2f,-.3f,.4f,.5f,2,1,0};
  assert(c.setManual(manual,x.nowUs).ack==Ack::Accepted);
  assert(c.setManual(manual,x.nowUs).ack==Ack::Duplicate);
  Output o=run(c,x); assert(o.safety==SafetyState::Running && o.leftFront==.2f && o.rightFront==-.3f && o.rearYaw==.4f && o.propulsion==.5f && !o.physicalGate);
  assert(c.setMode(ControlMode::HeadingHold,3).ack==Ack::Rejected);
  x.nowUs+=500001; x.heartbeatUs=x.nowUs; o=c.step(x,false,false,false,false); assert(o.safety==SafetyState::Disarmed && o.reason==StopReason::ManualTimeout);
  assert(c.setMode(ControlMode::AttitudeAssist,4).ack==Ack::Accepted); x=healthy(2000000); o=run(c,x); assert(o.leftPrelimit==o.rightPrelimit && o.leftPrelimit==0 && o.rightPrelimit==0);
  c.reset(); assert(c.setMode(ControlMode::HeadingHold,5).ack==Ack::Accepted); x=healthy(3000000); x.yawRad=3.13f; o=run(c,x); assert(std::fabs(o.targetYaw-3.13f)<.001f); assert(c.setHeading(-3.13f,6).ack==Ack::Accepted); o=c.step(x,false,false,false,false); assert(std::fabs(o.uYaw)<.1f);
  c.reset(); Waypoint route[2]={{0,0},{3,0}}; assert(c.setWaypoints(route,2,7).ack==Ack::Accepted); assert(c.setMode(ControlMode::AutoWaypoint,8).ack==Ack::Accepted); x=healthy(4000000); x.northM=0; x.eastM=0; o=run(c,x); assert(o.waypointReached && o.activeWaypoint==0); x.nowUs+=20000; x.heartbeatUs=x.imuUs=x.tofUs=x.gnssUs=x.nowUs; x.northM=3; o=c.step(x,false,false,false,false); assert(o.safety==SafetyState::Disarmed && o.reason==StopReason::FinalWaypoint);
  c.reset(); assert(c.setMode(ControlMode::Manual,9).ack==Ack::Accepted); x=healthy(5000000); assert(c.setManual(manual,x.nowUs).ack==Ack::Accepted); o=run(c,x); x.nowUs+=20000; x.heartbeat=false; o=c.step(x,false,false,false,false); assert(o.safety==SafetyState::Fault && o.reason==StopReason::Heartbeat && o.propulsion==0);
  c.reset(); x=healthy(6000000); o=c.step(x,false,false,true,false); assert(o.safety==SafetyState::EStop); o=c.step(x,true,false,false,false); assert(o.safety==SafetyState::EStop); o=c.step(x,false,false,false,true); assert(o.safety==SafetyState::Disarmed);
  PhysicalConfig physical{}; assert(!Controller::physicalConfigurationValid(physical));
  physical={0,1,2,3,1000,1500,2000,1000,1500,2000,1000,1500,2000,1000,1500,2000,true}; assert(Controller::physicalConfigurationValid(physical));
  assert(c.physicalWriteCount()==0);
  std::puts("COMPETITION_SHADOW_HOST_PASS modes=4 safety=6 manual=ok attitude=ok heading_wrap=ok auto_waypoint=ok gate_closed=ok");
}
