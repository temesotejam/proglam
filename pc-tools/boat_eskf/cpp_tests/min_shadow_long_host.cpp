#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include "../../../shared/proposal_min/src/proposal_min.h"

namespace {
#pragma pack(push, 1)
struct BinHeader { uint8_t version, type; uint16_t length; uint32_t sequence, bootId; uint64_t sourceUs; uint16_t flags; };
struct SnapshotPayload {
  uint64_t timestampUs; uint32_t cycle, waypointRevision, gnssAgeUs, imuAgeUs, tofAgeUs;
  double latitudeDeg, longitudeDeg, targetWaypointLatitudeDeg, targetWaypointLongitudeDeg;
  float speedMps, gnssCourseRad, localNorthM, localEastM, targetBearingRad, courseErrorRad, waypointDistanceM;
  float rollRad, pitchRad, yawRad, rollRateRadS, pitchRateRadS, yawRateRadS; uint16_t tofRawMm;
  float tofFilteredM, heightErrorM, uHeight, uPitch, uRoll, uYaw, frontCommon, frontDifferential;
  float leftFrontWing, rightFrontWing, rearYaw, propulsion, leftPrelimit, rightPrelimit, rearYawPrelimit, propulsionPrelimit;
  uint8_t gnssValid, imuValid, tofValid, heightValid, waypointReached, outputValid, state, safetyReason, mode, activeWaypoint, reserved[2];
};
struct InaPayload { uint64_t timestampUs; uint32_t ageUs; float busVoltageV, shuntVoltageV, currentA, powerW; uint8_t valid, errorCode; uint16_t reserved; };
struct VescPayload { uint64_t timestampUs; uint32_t ageUs; float inputVoltageV, motorCurrentA, inputCurrentA, duty, erpm, mosTempC, motorTempC; int32_t tachometer; uint8_t valid, mechanicalRpmValid, fault, reserved; };
#pragma pack(pop)
static_assert(sizeof(SnapshotPayload)==190, "snapshot wire size");
static_assert(sizeof(InaPayload)==32, "ina wire size");
static_assert(sizeof(VescPayload)==48, "vesc wire size");
constexpr uint32_t kMagic=0x424C4F47UL;

void writeRecord(std::ofstream& out, uint8_t type, uint32_t& sequence, uint64_t timestamp, const void* payload, uint16_t length) {
  out.write(reinterpret_cast<const char*>(&kMagic), sizeof(kMagic));
  out.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
  BinHeader h{1,type,length,++sequence,7,timestamp,0};
  out.write(reinterpret_cast<const char*>(&h), sizeof(h));
  out.write(reinterpret_cast<const char*>(payload), length);
}

}  // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "min_shadow_30min.BIN";
  constexpr uint64_t kDurationUs=1800ULL*1000000ULL, kPeriodUs=20000ULL;
  const uint64_t steps=kDurationUs/kPeriodUs;
  std::ofstream out(path, std::ios::binary); if(!out) return 2;
  proposal_min::Controller controller;
  proposal_min::ControlConfig cfg{};
  cfg.leftFrontWing=proposal_min::ActuatorConfig{0,-1,1,1,1,false};
  cfg.rightFrontWing=proposal_min::ActuatorConfig{0,-1,1,1,1,false};
  cfg.rearYaw=proposal_min::ActuatorConfig{0,-1,1,1,1,false};
  cfg.propulsion=proposal_min::ActuatorConfig{0,0,0,1,1,false};
  proposal_min::Waypoint route[]={{0.0f,0.0f},{4.0f,0.0f},{8.0f,2.0f},{12.0f,0.0f}};
  uint16_t ranges[8]={1200,1201,1199,1202,1200,1201,1198,1200}; uint8_t status[8]={5,5,5,5,5,5,5,5};
  uint32_t sequence=0; uint64_t lastGnss=0,lastImu=0,lastTof=0; uint64_t outputCount=0;
  uint8_t waypointSet[276]{}; uint8_t waypointAck[16]{};
  writeRecord(out,66,sequence,0,waypointSet,sizeof(waypointSet));
  writeRecord(out,67,sequence,1,waypointAck,sizeof(waypointAck));
  for(uint64_t i=0;i<steps;++i) {
    const uint64_t now=(i+1)*kPeriodUs;
    proposal_min::Input in{}; in.nowUs=now; in.start=(i==0 || now==920000000ULL); in.stop=(now==900000000ULL);
    in.estop=(now>=1200000000ULL && now<1210000000ULL); in.heartbeatOk=!(now>=1400000000ULL && now<1410000000ULL);
    const bool gnssFault=(now>=180000000ULL && now<190000000ULL); in.gnss={35.0+(now/1000000.0)*0.000001,139.0,1.2f,0.15f+0.4f*sinf((float)now/10000000.0f),now, !gnssFault};
    if(!gnssFault) lastGnss=now;
    const bool imuFault=(now>=360000000ULL && now<370000000ULL); const bool imuNan=(now>=720000000ULL && now<721000000ULL);
    in.imu={0.2f*sinf((float)now/15000000.0f),0.08f*sinf((float)now/11000000.0f),0.05f*cosf((float)now/13000000.0f),
            0.01f*cosf((float)now/15000000.0f),0.01f*cosf((float)now/11000000.0f),-0.01f*sinf((float)now/13000000.0f),now,!imuFault};
    if(imuNan){in.imu.yawRad=NAN;in.imu.valid=true;} if(!imuFault && !imuNan) lastImu=now;
    const bool tofFault=(now>=540000000ULL && now<550000000ULL); in.tof={ranges,status,8,now,!tofFault}; if(!tofFault) lastTof=now;
    in.waypoints=route; in.waypointCount=4;
    const proposal_min::Output o=controller.step(in,cfg); ++outputCount;
    SnapshotPayload s{}; s.timestampUs=now;s.cycle=(uint32_t)i;s.gnssAgeUs=lastGnss?(uint32_t)(now-lastGnss):UINT32_MAX;s.imuAgeUs=lastImu?(uint32_t)(now-lastImu):UINT32_MAX;s.tofAgeUs=lastTof?(uint32_t)(now-lastTof):UINT32_MAX;
    s.latitudeDeg=in.gnss.latitudeDeg;s.longitudeDeg=in.gnss.longitudeDeg;s.speedMps=in.gnss.speedMps;s.gnssCourseRad=in.gnss.courseRad;s.rollRad=in.imu.rollRad;s.pitchRad=in.imu.pitchRad;s.yawRad=in.imu.yawRad;s.rollRateRadS=in.imu.rollRateRadS;s.pitchRateRadS=in.imu.pitchRateRadS;s.yawRateRadS=in.imu.yawRateRadS;s.tofRawMm=o.tofRawMm;s.tofFilteredM=o.tofFilteredM;s.heightErrorM=o.heightErrorM;s.uHeight=o.u_height;s.uPitch=o.u_pitch;s.uRoll=o.u_roll;s.uYaw=o.u_yaw;s.frontCommon=o.frontCommon;s.frontDifferential=o.frontDifferential;s.leftFrontWing=o.left_front_wing;s.rightFrontWing=o.right_front_wing;s.rearYaw=o.rear_yaw;s.propulsion=o.propulsion;s.leftPrelimit=o.leftPrelimit;s.rightPrelimit=o.rightPrelimit;s.rearYawPrelimit=o.rearYawPrelimit;s.propulsionPrelimit=o.propulsionPrelimit;s.gnssValid=o.gnssValid;s.imuValid=o.imuValid;s.tofValid=o.tofValid;s.heightValid=o.heightValid;s.waypointReached=o.waypointReached;s.outputValid=o.inputValid;s.state=(uint8_t)o.safety;s.safetyReason=o.stopReason ? o.stopReason : (o.safety==proposal_min::Safety::EStop ? 2 : 0);s.mode=in.start ? 2 : 1;s.activeWaypoint=o.waypointIndex;
    writeRecord(out,63,sequence,now,&s,sizeof(s));
    InaPayload ina{}; ina.timestampUs=now+1; ina.ageUs=0; ina.valid=(now/1000000ULL)%240<180; ina.errorCode=ina.valid?0:1; ina.busVoltageV=ina.valid?12.0f+(float)(now/1000000ULL)*0.001f:NAN;ina.shuntVoltageV=ina.valid?0.01f:NAN;ina.currentA=ina.valid?1.0f:NAN;ina.powerW=ina.valid?12.0f:NAN;writeRecord(out,64,sequence,now+1,&ina,sizeof(ina));
    VescPayload v{};v.timestampUs=now+2;v.ageUs=0;v.valid=((now/1000000ULL)%300)<220;v.mechanicalRpmValid=0;v.inputVoltageV=v.valid?24.0f:NAN;v.motorCurrentA=v.valid?2.0f:NAN;v.inputCurrentA=v.valid?1.0f:NAN;v.duty=v.valid?0.1f:NAN;v.erpm=v.valid?1000.0f+(float)(now/1000000ULL):NAN;v.mosTempC=v.valid?40.0f:NAN;v.motorTempC=v.valid?42.0f:NAN;v.tachometer=v.valid?(int32_t)(now/1000000ULL):0;writeRecord(out,65,sequence,now+2,&v,sizeof(v));
    if((i%10000)==0) std::cerr << "step=" << i << "/" << steps << " state=" << (unsigned)s.state << "\n";
  }
  out.close();
  const auto& m=controller.metrics();
  std::cout << "MIN_SHADOW_LONG_PASS duration_s=1800 period_us=" << kPeriodUs << " steps=" << steps << " outputs=" << outputCount << " transitions=" << m.stateTransitions << " nan_inf=" << m.nanInf << " deadline_miss=" << m.task.deadlineMiss << "\n";
  return outputCount==steps ? 0 : 3;
}
