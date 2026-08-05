#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <boat_protocol.h>
#include <proposal_min.h>
#include <waypoint_apply.h>
#include "bin_record_writer.h"

namespace {
using boat_test::writeRecord;

using SnapshotPayload = boat::ControlSnapshotPayload;
using InaPayload = boat::InaStatusPayload;
using VescPayload = boat::VescTelemetryPayload;
static_assert(sizeof(SnapshotPayload) == 190, "snapshot wire size");
static_assert(sizeof(InaPayload) == 32, "ina wire size");
static_assert(sizeof(VescPayload) == 48, "vesc wire size");

struct OutputStats {
  float min = std::numeric_limits<float>::infinity();
  float max = -std::numeric_limits<float>::infinity();
  uint32_t nonNeutral = 0, changes = 0, safe = 0, range = 0, slew = 0, nonfinite = 0;
};
void observe(OutputStats& s, float value, bool safe, bool running, bool hasPrevious, float previous, double dt, float lo, float hi) {
  if (!std::isfinite(value)) { ++s.nonfinite; return; }
  s.min = std::min(s.min, value); s.max = std::max(s.max, value);
  if (std::fabs(value) > 1e-5f) ++s.nonNeutral;
  if (safe && std::fabs(value) <= 1e-5f) ++s.safe;
  if (value < lo - 1e-5f || value > hi + 1e-5f) ++s.range;
  if (hasPrevious && std::fabs(value - previous) > 1e-5f) ++s.changes;
  if (running && hasPrevious && std::fabs(value - previous) > 50.0 * dt + 1e-5) ++s.slew;
}
}
int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "min_shadow_30min.BIN";
  constexpr uint64_t durationUs = 1800ULL * 1000000ULL, periodUs = 20000ULL;
  constexpr uint64_t steps = durationUs / periodUs;
  std::ofstream out(path, std::ios::binary); if (!out) return 2;
  proposal_min::Controller controller;
  proposal_min::ControlConfig cfg{};
  cfg.leftFrontWing = proposal_min::ActuatorConfig{0, -1, 1, 1, 1, false};
  cfg.rightFrontWing = proposal_min::ActuatorConfig{0, -1, 1, 1, 1, false};
  cfg.rearYaw = proposal_min::ActuatorConfig{0, -1, 1, 1, 1, false};
  cfg.propulsion = proposal_min::ActuatorConfig{0, 0, 1, 1, 0.2f, false};
  cfg.propulsionCommand = 0.4f; cfg.propulsionStop = 0;
  proposal_min::Waypoint route[] = {{100000.0f, 0.0f}, {100001.0f, 0.0f}, {100002.0f, 1.0f}, {100003.0f, 0.0f}};
  uint16_t ranges[8] = {1200,1201,1199,1202,1200,1201,1198,1200};
  uint8_t status[8] = {5,5,5,5,5,5,5,5};
  boat::WaypointSetPayload waypointSet{};
  waypointSet.requestId = 77; waypointSet.revision = 1; waypointSet.action = 1; waypointSet.count = 4; waypointSet.reachRadiusM = 0.5f;
  waypointSet.points[0] = {35.0,139.0}; waypointSet.points[1] = {35.00001,139.0};
  waypointSet.points[2] = {35.00002,139.00001}; waypointSet.points[3] = {35.00003,139.0};
  waypointSet.canonicalCrc = boat::canonicalCrc(&waypointSet, offsetof(boat::WaypointSetPayload, canonicalCrc));
  proposal_min::WaypointGeo geo[4]{};
  for (int i = 0; i < 4; ++i) { geo[i].latitudeDeg = waypointSet.points[i].latitudeDeg; geo[i].longitudeDeg = waypointSet.points[i].longitudeDeg; }
  proposal_min::WaypointStore store{};
  proposal_min::WaypointRequest request{waypointSet.requestId, waypointSet.revision, waypointSet.action, waypointSet.count, waypointSet.reachRadiusM, geo};
  const auto waypointResult = proposal_min::applyWaypointSet(proposal_min::WaypointSafetyState::Disarmed, request, store);
  assert(waypointResult.status == proposal_min::WaypointApplyStatus::Accepted && store.count == 4);
  boat::WaypointAckPayload waypointAck{};
  waypointAck.requestId = waypointSet.requestId; waypointAck.revision = store.revision; waypointAck.status = static_cast<uint8_t>(waypointResult.status);
  waypointAck.reason = static_cast<uint8_t>(waypointResult.reason); waypointAck.activeIndex = store.activeIndex; waypointAck.count = store.count;
  waypointAck.canonicalCrc = boat::canonicalCrc(&waypointAck, offsetof(boat::WaypointAckPayload, canonicalCrc));
  uint32_t sequence = 0;
  writeRecord(out, boat::Type::WaypointSet, sequence, 0, &waypointSet, sizeof(waypointSet));
  writeRecord(out, boat::Type::WaypointAck, sequence, 1, &waypointAck, sizeof(waypointAck));
  uint64_t lastGnss = 0, lastImu = 0, lastTof = 0, outputCount = 0, runningNonzero = 0, safeZero = 0;
  uint32_t resetRecoveries = 0;
  uint32_t starts = 0, stops = 0, estops = 0, heartbeatFaults = 0, sensorFaults = 0;
  uint32_t gnssInvalid = 0, gnssFrozen = 0, imuInvalid = 0, imuFrozen = 0, tofInvalid = 0, tofFrozen = 0;
  uint32_t inaNormal = 0, inaInvalid = 0, inaMissing = 0, inaFrozen = 0, vescNormal = 0, vescInvalid = 0, vescMissing = 0, vescFrozen = 0, vescFault = 0;
  uint32_t courseWrapSamples = 0; OutputStats stats[4]{}; float previous[4]{}; bool previousValid = false;
  for (uint64_t i = 0; i < steps; ++i) {
    const uint64_t now = (i + 1) * periodUs;
    const bool stop = now == 220000000ULL || now == 400000000ULL || now == 580000000ULL || now == 740000000ULL || now == 900000000ULL || now == 1230000000ULL || now == 1430000000ULL;
    const bool start = i == 0 || now == 240000000ULL || now == 420000000ULL || now == 600000000ULL || now == 760000000ULL || now == 920000000ULL || now == 1250000000ULL || now == 1450000000ULL;
    const bool estop = now >= 1200000000ULL && now < 1210000000ULL;
    const bool heartbeatOk = !(now >= 1400000000ULL && now < 1410000000ULL);
    const bool gnssBad = now >= 180000000ULL && now < 185000000ULL;
    const bool gnssFreeze = now >= 185000000ULL && now < 190000000ULL;
    const bool imuBad = now >= 360000000ULL && now < 365000000ULL;
    const bool imuFreeze = now >= 365000000ULL && now < 370000000ULL;
    const bool imuNan = now >= 720000000ULL && now < 721000000ULL;
    const bool tofBad = now >= 540000000ULL && now < 545000000ULL;
    const bool tofFreeze = now >= 545000000ULL && now < 550000000ULL;
    if (start) ++starts;
    if (stop) ++stops;
    if (estop) ++estops;
    if (!heartbeatOk) ++heartbeatFaults;
    if (gnssBad || gnssFreeze || imuBad || imuFreeze || imuNan || tofBad || tofFreeze) ++sensorFaults;
    if (gnssBad) ++gnssInvalid;
    if (gnssFreeze) ++gnssFrozen;
    if (imuBad) ++imuInvalid;
    if (imuFreeze) ++imuFrozen;
    if (tofBad) ++tofInvalid;
    if (tofFreeze) ++tofFrozen;
    proposal_min::Input in{};
    in.nowUs = now; in.start = start; in.stop = stop; in.estop = estop; in.heartbeatOk = heartbeatOk;
    const float wrapCourse = (now >= 100000000ULL && now < 100200000ULL) ? -3.13f : ((now >= 100200000ULL && now < 100400000ULL) ? 3.13f : 0.15f);
    in.gnss = {35.0 + (now / 1000000.0) * 0.000001, 139.0, 1.2f, wrapCourse, now, true};
    if (gnssBad) in.gnss.valid = false;
    if (gnssFreeze) in.gnss.timestampUs = lastGnss;
    if (in.gnss.valid && !gnssFreeze) lastGnss = in.gnss.timestampUs;
    in.imu = {0.2f*sinf(static_cast<float>(now)/15000000.0f), 0.08f*sinf(static_cast<float>(now)/11000000.0f), 0.05f*cosf(static_cast<float>(now)/13000000.0f), 0.01f, 0.01f, -0.01f, now, true};
    if (imuBad) in.imu.valid = false;
    if (imuFreeze) in.imu.timestampUs = lastImu;
    if (imuNan) in.imu.yawRad = NAN;
    if (in.imu.valid && !imuFreeze && !imuNan) lastImu = in.imu.timestampUs;
    in.tof = {ranges, status, 8, now, true}; if (tofBad) in.tof.valid = false; if (tofFreeze) in.tof.timestampUs = lastTof; if (in.tof.valid && !tofFreeze) lastTof = in.tof.timestampUs;
    in.waypoints = route; in.waypointCount = 4;
    if (start && (now == 240000000ULL || now == 420000000ULL || now == 600000000ULL || now == 760000000ULL || now == 920000000ULL || now == 1250000000ULL || now == 1450000000ULL)) { controller.reset(); ++resetRecoveries; }
    const proposal_min::Output o = controller.step(in, cfg); ++outputCount;
    if (o.safety == proposal_min::Safety::Running && o.propulsion > 0) ++runningNonzero;
    if (o.safety != proposal_min::Safety::Running && o.propulsion == 0) ++safeZero;
    const bool safe = o.safety != proposal_min::Safety::Running; const double dt = periodUs / 1e6;
    const float values[4] = {o.left_front_wing,o.right_front_wing,o.rear_yaw,o.propulsion};
    const float lows[4] = {-1,-1,-1,0}, highs[4] = {1,1,1,1};
    for (int k=0;k<4;++k) observe(stats[k], values[k], safe, o.safety==proposal_min::Safety::Running, previousValid, previous[k], dt, lows[k], highs[k]);
    if (previousValid) for (int k=0;k<4;++k) previous[k]=values[k]; else { for (int k=0;k<4;++k) previous[k]=values[k]; previousValid=true; }
    if (std::fabs(o.courseErrorRad) > 3.0f) ++courseWrapSamples;
    SnapshotPayload snap{};
    snap.timestampUs=now; snap.cycle=static_cast<uint32_t>(i); snap.waypointRevision=store.revision; snap.gnssAgeUs=lastGnss?(uint32_t)(now-lastGnss):UINT32_MAX; snap.imuAgeUs=lastImu?(uint32_t)(now-lastImu):UINT32_MAX; snap.tofAgeUs=lastTof?(uint32_t)(now-lastTof):UINT32_MAX;
    snap.latitudeDeg=in.gnss.latitudeDeg; snap.longitudeDeg=in.gnss.longitudeDeg; snap.targetWaypointLatitudeDeg=store.points[o.waypointIndex<store.count?o.waypointIndex:0].latitudeDeg; snap.targetWaypointLongitudeDeg=store.points[o.waypointIndex<store.count?o.waypointIndex:0].longitudeDeg; snap.speedMps=in.gnss.speedMps; snap.gnssCourseRad=in.gnss.courseRad; snap.courseErrorRad=o.courseErrorRad; snap.localNorthM=0; snap.localEastM=0; snap.targetBearingRad=o.targetCourseRad; snap.waypointDistanceM=o.waypointDistanceM; snap.rollRad=in.imu.rollRad; snap.pitchRad=in.imu.pitchRad; snap.yawRad=in.imu.yawRad; snap.rollRateRadS=in.imu.rollRateRadS; snap.pitchRateRadS=in.imu.pitchRateRadS; snap.yawRateRadS=in.imu.yawRateRadS; snap.tofRawMm=o.tofRawMm; snap.tofFilteredM=o.tofFilteredM; snap.heightErrorM=o.heightErrorM; snap.uHeight=o.u_height; snap.uPitch=o.u_pitch; snap.uRoll=o.u_roll; snap.uYaw=o.u_yaw; snap.frontCommon=o.frontCommon; snap.frontDifferential=o.frontDifferential; snap.leftFrontWing=o.left_front_wing; snap.rightFrontWing=o.right_front_wing; snap.rearYaw=o.rear_yaw; snap.propulsion=o.propulsion; snap.leftPrelimit=o.leftPrelimit; snap.rightPrelimit=o.rightPrelimit; snap.rearYawPrelimit=o.rearYawPrelimit; snap.propulsionPrelimit=o.propulsionPrelimit; snap.gnssValid=o.gnssValid; snap.imuValid=o.imuValid; snap.tofValid=o.tofValid; snap.heightValid=o.heightValid; snap.waypointReached=o.waypointReached; snap.outputValid=o.inputValid; snap.state=static_cast<uint8_t>(o.safety); snap.safetyReason=o.stopReason; snap.mode=start?2:1; snap.activeWaypoint=o.waypointIndex;
    writeRecord(out, boat::Type::ControlSnapshot, sequence, now, &snap, sizeof(snap));
    const bool inaMissingNow = now >= 300000000ULL && now < 305000000ULL; const bool inaInvalidNow = now >= 320000000ULL && now < 325000000ULL; const bool inaFrozenNow = now >= 340000000ULL && now < 345000000ULL;
    if (inaMissingNow) ++inaMissing; else { InaPayload ina{}; ina.timestampUs=inaFrozenNow?lastGnss:now; ina.ageUs=inaFrozenNow?static_cast<uint32_t>(now-ina.timestampUs):0; ina.valid=!inaInvalidNow; ina.errorCode=inaInvalidNow?2:0; ina.busVoltageV=ina.valid?12.0f:NAN; ina.shuntVoltageV=ina.valid?0.01f:NAN; ina.currentA=ina.valid?1.0f:NAN; ina.powerW=ina.valid?12.0f:NAN; writeRecord(out,boat::Type::InaStatus,sequence,now,&ina,sizeof(ina)); if(inaInvalidNow)++inaInvalid; else if(inaFrozenNow)++inaFrozen; else ++inaNormal; }
    const bool vescMissingNow = now >= 800000000ULL && now < 805000000ULL; const bool vescInvalidNow = now >= 820000000ULL && now < 825000000ULL; const bool vescFrozenNow = now >= 840000000ULL && now < 845000000ULL; const bool vescFaultNow = now >= 860000000ULL && now < 865000000ULL;
    if (vescMissingNow) ++vescMissing; else { VescPayload v{}; v.timestampUs=vescFrozenNow?lastImu:now; v.ageUs=vescFrozenNow?static_cast<uint32_t>(now-v.timestampUs):0; v.valid=!vescInvalidNow; v.mechanicalRpmValid=0; v.fault=vescFaultNow?7:0; v.inputVoltageV=v.valid?24.0f:NAN; v.motorCurrentA=v.valid?2.0f:NAN; v.inputCurrentA=v.valid?1.0f:NAN; v.duty=v.valid?0.1f:NAN; v.erpm=v.valid?1000.0f:NAN; v.mosTempC=v.valid?40.0f:NAN; v.motorTempC=v.valid?42.0f:NAN; v.tachometer=0; writeRecord(out,boat::Type::VescTelemetry,sequence,now,&v,sizeof(v)); if(v.valid&&!vescFaultNow&&!vescFrozenNow)++vescNormal; if(vescInvalidNow)++vescInvalid; if(vescFrozenNow)++vescFrozen; if(vescFaultNow)++vescFault; }
  }
  out.close(); const auto& m=controller.metrics();
  std::cout << "MIN_SHADOW_LONG_PASS duration_s=1800 period_us=20000 steps=90000 outputs=" << outputCount
            << " starts=" << starts << " stops=" << stops << " estops=" << estops
            << " heartbeat_fault_samples=" << heartbeatFaults << " sensor_fault_samples=" << sensorFaults
            << " gnss_invalid=" << gnssInvalid << " gnss_frozen=" << gnssFrozen << " imu_invalid=" << imuInvalid << " imu_frozen=" << imuFrozen
            << " tof_invalid=" << tofInvalid << " tof_frozen=" << tofFrozen
            << " ina_normal=" << inaNormal << " ina_invalid=" << inaInvalid << " ina_missing=" << inaMissing << " ina_frozen=" << inaFrozen
            << " vesc_normal=" << vescNormal << " vesc_invalid=" << vescInvalid << " vesc_missing=" << vescMissing << " vesc_frozen=" << vescFrozen << " vesc_fault=" << vescFault
            << " course_wrap_samples=" << courseWrapSamples << " running_nonzero_propulsion=" << runningNonzero << " safe_zero_outputs=" << safeZero
            << " reset_recoveries=" << resetRecoveries << " transitions_last_segment=" << m.stateTransitions << " nan_inf=" << m.nanInf << " deadline_miss=" << m.task.deadlineMiss
            << " output_stats=left:" << stats[0].min << "/" << stats[0].max << "/" << stats[0].nonNeutral << "/" << stats[0].changes << "/" << stats[0].safe
            << ",right:" << stats[1].min << "/" << stats[1].max << "/" << stats[1].nonNeutral << "/" << stats[1].changes << "/" << stats[1].safe
            << ",rear:" << stats[2].min << "/" << stats[2].max << "/" << stats[2].nonNeutral << "/" << stats[2].changes << "/" << stats[2].safe
            << ",propulsion:" << stats[3].min << "/" << stats[3].max << "/" << stats[3].nonNeutral << "/" << stats[3].changes << "/" << stats[3].safe
            << " waypoint_ack=accepted\n";
  return outputCount==steps && runningNonzero>0 && safeZero>0 && courseWrapSamples>0 ? 0 : 3;
}
