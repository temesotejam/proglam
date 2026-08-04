#include <cassert>
#include <cmath>
#include <cstdio>
#include "../../../shared/proposal_min/src/proposal_min.h"

int main() {
  proposal_min::Controller c;
  proposal_min::Waypoint route[]={{2.0f,0.0f},{5.0f,1.0f},{7.0f,-1.0f}};
  uint16_t ranges[8]={1200,1201,1199,1202,1200,1201,1198,1200}; uint8_t status[8]={5,5,5,5,5,5,5,5};
  proposal_min::Input in{}; in.nowUs=1000000; in.start=true; in.heartbeatOk=true; in.gnss={35.0,139.0,1.2f,0.2f,1000000,true}; in.imu={0.1f,0.02f,0.01f,0.0f,1000000,true}; in.tof={ranges,status,8,1000000,true}; in.waypoints=route; in.waypointCount=3;
  auto out=c.step(in); assert(out.safety==proposal_min::Safety::Running); assert(out.finite); assert(std::fabs(out.propulsion)<1e-6f);
  in.nowUs+=20000; in.start=false; out=c.step(in); assert(out.finite);
  in.estop=true; in.nowUs+=20000; out=c.step(in); assert(out.safety==proposal_min::Safety::EStop); assert(out.leftFront==0 && out.rightFront==0 && out.rearYaw==0);
  std::puts("proposal_min_host PASS"); return 0;
}
