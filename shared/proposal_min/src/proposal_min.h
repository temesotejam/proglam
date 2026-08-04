#pragma once

#include <stdint.h>
#include <stddef.h>

namespace proposal_min {

enum class Safety : uint8_t { Disarmed=0, Running=1, EStop=2, Fault=3 };
enum class Saturation : uint8_t { Yaw=0, Roll=1, Height=2, LeftWing=3, RightWing=4, RearYaw=5, Propulsion=6, Count=7 };

struct GnssInput { double latitudeDeg=0, longitudeDeg=0; float speedMps=0, courseRad=0; uint64_t timestampUs=0; bool valid=false; };
struct ImuInput { float yawRad=0, rollRad=0, pitchRad=0, yawRateRadS=0; uint64_t timestampUs=0; bool valid=false; };
struct TofInput { const uint16_t* rangesMm=nullptr; const uint8_t* status=nullptr; uint8_t count=0; uint64_t timestampUs=0; bool valid=false; };
struct Waypoint { float northM; float eastM; Waypoint(float n=0,float e=0):northM(n),eastM(e){} };
struct Input { GnssInput gnss{}; ImuInput imu{}; TofInput tof{}; const Waypoint* waypoints=nullptr; uint8_t waypointCount=0; uint64_t nowUs=0; bool start=false, stop=false, estop=false, heartbeatOk=true; };
struct Output { float leftFront=0, rightFront=0, rearYaw=0, propulsion=0; Safety safety=Safety::Disarmed; bool finite=true, inputValid=true, saturated=false; uint8_t waypointIndex=0; };

struct Metric { uint32_t calls=0; uint64_t totalUs=0; uint32_t maxUs=0; uint32_t deadlineMiss=0; uint32_t nanInf=0; uint32_t invalid=0; uint32_t saturation=0; uint32_t samples=0; uint32_t sampleUs[64]{}; };
struct Metrics { Metric task; Metric operation[8]; uint32_t queueCurrent=0, queueHighWater=0, queueDrop=0; uint32_t uartBytes=0, uartDrop=0, uartGaps=0; uint32_t sdGenerated=0, sdWritten=0, sdBlocked=0, sdErrors=0; uint32_t sensorAgeGnssUs=0, sensorAgeGvrUs=0, sensorAgeGyroUs=0, sensorAgeTofUs=0, sensorAgeInaUs=0; uint32_t i2cTransactions=0, i2cErrors=0, watchdogResets=0, nanInf=0, stateTransitions=0; uint32_t stopCount=0, estopCount=0, heartbeatTimeout=0; uint32_t overheadUs=0; uint32_t shadowOutputCount=0; float shadowMin[4]={0,0,0,0}; float shadowMax[4]={0,0,0,0}; };

class Controller {
 public:
  Controller();
  void reset();
  Output step(const Input& input);
  const Metrics& metrics() const { return metrics_; }
  Metrics& metrics() { return metrics_; }
  Safety safety() const { return safety_; }
 private:
  void record(Metric& metric, uint32_t elapsedUs, uint32_t deadlineUs, bool invalid, bool saturated, bool finite);
  float clamp(float value, Saturation category);
  Safety safety_=Safety::Disarmed;
  uint8_t waypointIndex_=0;
  bool originSet_=false;
  double originLat_=0, originLon_=0;
  float heightM_=1.2f;
  uint64_t lastGnssUs_=0,lastGvrUs_=0,lastGyroUs_=0,lastTofUs_=0,lastStepUs_=0;
  Metrics metrics_{};
};

const char* safetyName(Safety state);

} // namespace proposal_min
