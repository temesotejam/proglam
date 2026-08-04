#include "proposal_min.h"
#include <math.h>

namespace proposal_min {
namespace {
constexpr float kPi=3.14159265358979323846f;
constexpr float kEarthMPerDeg=111320.0f;
constexpr uint32_t kGnssStaleUs=500000, kImuStaleUs=100000, kTofStaleUs=250000;
float wrapPi(float x){ while(x>kPi)x-=2*kPi; while(x<-kPi)x+=2*kPi; return x; }
float finiteOr(float x,float fallback=0){return isfinite(x)?x:fallback;}
}
Controller::Controller(){reset();}
void Controller::reset(){safety_=Safety::Disarmed; waypointIndex_=0; originSet_=false; originLat_=originLon_=0; heightM_=1.2f; lastGnssUs_=lastGvrUs_=lastGyroUs_=lastTofUs_=lastStepUs_=0; metrics_={};}
void Controller::record(Metric& m,uint32_t elapsed,uint32_t deadline,bool invalid,bool saturated,bool finite){
  ++m.calls; m.totalUs+=elapsed; if(elapsed>m.maxUs)m.maxUs=elapsed; if(elapsed>deadline)++m.deadlineMiss; if(invalid)++m.invalid; if(!finite)++m.nanInf; if(saturated)++m.saturation; if(m.samples<64)m.sampleUs[m.samples++]=elapsed;
}
float Controller::clamp(float v,Saturation c){if(!isfinite(v)){++metrics_.nanInf;return 0;}const float o=v; if(v>1)v=1; if(v<-1)v=-1; if(v!=o){metrics_.operation[(uint8_t)c].saturation++;metrics_.operation[(uint8_t)c].calls++;} return v;}
Output Controller::step(const Input& in){
  const uint64_t t0=in.nowUs; Output out{}; bool invalid=false;
  if(in.stop){if(safety_!=Safety::Disarmed){safety_=Safety::Disarmed;++metrics_.stateTransitions;}++metrics_.stopCount;}
  if(in.estop){if(safety_!=Safety::EStop){safety_=Safety::EStop;++metrics_.stateTransitions;}++metrics_.estopCount;}
  if(!in.heartbeatOk){if(safety_!=Safety::Fault){safety_=Safety::Fault;++metrics_.stateTransitions;}++metrics_.heartbeatTimeout;}
  if(in.start && safety_==Safety::Disarmed){safety_=Safety::Running;++metrics_.stateTransitions;}
  float north=0,east=0,bearing=0; bool gnssOk=in.gnss.valid&&isfinite(in.gnss.latitudeDeg)&&isfinite(in.gnss.longitudeDeg);
  if(gnssOk){if(!originSet_){originSet_=true;originLat_=in.gnss.latitudeDeg;originLon_=in.gnss.longitudeDeg;} north=(float)((in.gnss.latitudeDeg-originLat_)*kEarthMPerDeg); east=(float)((in.gnss.longitudeDeg-originLon_)*kEarthMPerDeg*cos(originLat_*kPi/180.0));lastGnssUs_=in.gnss.timestampUs;}
  invalid|=!gnssOk; record(metrics_.operation[0],0,100000, !gnssOk,false,isfinite(north)&&isfinite(east));
  Waypoint target{}; if(in.waypoints&&in.waypointCount){target=in.waypoints[waypointIndex_<in.waypointCount?waypointIndex_:in.waypointCount-1]; const float d=hypotf(target.northM-north,target.eastM-east); if(d<.5f&&waypointIndex_+1<in.waypointCount)++waypointIndex_; target=in.waypoints[waypointIndex_]; bearing=atan2f(target.eastM-east,target.northM-north);} else bearing=isfinite(in.gnss.courseRad)?in.gnss.courseRad:0;
  record(metrics_.operation[1],0,100000,false,false,isfinite(bearing));
  const float los=wrapPi(bearing-atan2f(east,fmaxf(.001f,north))); const float align=wrapPi(bearing-(in.imu.valid?in.imu.yawRad:0));
  const bool cogOk=in.gnss.valid&&isfinite(in.gnss.speedMps)&&isfinite(in.gnss.courseRad)&&in.gnss.speedMps>=.5f&&(in.nowUs>=in.gnss.timestampUs?in.nowUs-in.gnss.timestampUs:UINT64_MAX)<=kGnssStaleUs;
  float yaw=clamp(.8f*(los+align),Saturation::Yaw); record(metrics_.operation[2],0,50000,!cogOk,false,isfinite(yaw));
  const float roll=in.imu.valid?finiteOr(in.imu.rollRad):0; const float rollRate=in.imu.valid?finiteOr(in.imu.yawRateRadS):0; const float rollCmd=clamp(-1.4f*roll-.25f*rollRate,Saturation::Roll); record(metrics_.operation[3],0,20000,!in.imu.valid,false,isfinite(rollCmd));
  float distance=heightM_; bool tofOk=false; if(in.tof.valid&&in.tof.rangesMm&&in.tof.status&&in.tof.count){uint16_t values[64]{};uint8_t n=0;for(uint8_t i=0;i<in.tof.count&&i<64;++i)if((in.tof.status[i]==5||in.tof.status[i]==9)&&in.tof.rangesMm[i]>=200&&in.tof.rangesMm[i]<=4000)values[n++]=in.tof.rangesMm[i];if(n>=4){for(uint8_t i=1;i<n;++i){uint16_t x=values[i];uint8_t j=i;while(j&&values[j-1]>x){values[j]=values[j-1];--j;}values[j]=x;}distance=values[n/2]/1000.0f;tofOk=true;lastTofUs_=in.tof.timestampUs;}} record(metrics_.operation[4],0,100000,!tofOk,false,isfinite(distance));
  distance*=cosf(in.imu.valid?in.imu.rollRad:0)*cosf(in.imu.valid?in.imu.pitchRad:0); heightM_=.9f*heightM_+.1f*distance; const float heightCmd=clamp(.7f*(1.2f-heightM_),Saturation::Height); record(metrics_.operation[5],0,20000,!tofOk,false,isfinite(heightCmd));
  float left=clamp(rollCmd+ yaw,Saturation::LeftWing), right=clamp(-rollCmd+yaw,Saturation::RightWing); record(metrics_.operation[6],0,20000,false,false,isfinite(left)&&isfinite(right));
  if(safety_!=Safety::Running||!cogOk||!in.imu.valid||!gnssOk){if(safety_==Safety::Running){safety_=Safety::Fault;++metrics_.stateTransitions;}left=right=yaw=0;}
  out.leftFront=left;out.rightFront=right;out.rearYaw=clamp(yaw,Saturation::RearYaw);out.propulsion=0;out.safety=safety_;out.waypointIndex=waypointIndex_;out.inputValid=!invalid;out.finite=isfinite(left)&&isfinite(right)&&isfinite(out.rearYaw);out.saturated=metrics_.operation[(uint8_t)Saturation::Yaw].saturation||metrics_.operation[(uint8_t)Saturation::Roll].saturation;
  if(lastStepUs_&&in.nowUs>=lastStepUs_) { metrics_.task.deadlineMiss += (in.nowUs-lastStepUs_>20000); }
  lastStepUs_=in.nowUs;
  metrics_.sensorAgeGnssUs=lastGnssUs_?(uint32_t)(in.nowUs-lastGnssUs_):UINT32_MAX;
  metrics_.sensorAgeGvrUs=in.imu.valid?(uint32_t)(in.nowUs-in.imu.timestampUs):UINT32_MAX;
  metrics_.sensorAgeGyroUs=metrics_.sensorAgeGvrUs;
  metrics_.sensorAgeTofUs=lastTofUs_?(uint32_t)(in.nowUs-lastTofUs_):UINT32_MAX;
  metrics_.overheadUs+=(uint32_t)(in.nowUs-t0);
  const float values[4]={out.leftFront,out.rightFront,out.rearYaw,out.propulsion}; if(metrics_.shadowOutputCount==0){for(uint8_t i=0;i<4;++i)metrics_.shadowMin[i]=metrics_.shadowMax[i]=values[i];}else{for(uint8_t i=0;i<4;++i){if(values[i]<metrics_.shadowMin[i])metrics_.shadowMin[i]=values[i];if(values[i]>metrics_.shadowMax[i])metrics_.shadowMax[i]=values[i];}} ++metrics_.shadowOutputCount; return out;
}
const char* safetyName(Safety s){switch(s){case Safety::Disarmed:return "DISARMED";case Safety::Running:return "RUNNING";case Safety::EStop:return "E_STOP";default:return "FAULT";}}
}
