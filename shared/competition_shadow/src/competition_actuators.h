#pragma once
#include <stdint.h>
namespace competition_shadow {
struct ServoTuning { float minUs,neutralUs,maxUs,maxRateUsPerSecond; bool reversed; constexpr ServoTuning(float min=1480.0f,float neutral=1500.0f,float max=1520.0f,float rate=100.0f,bool reverse=false):minUs(min),neutralUs(neutral),maxUs(max),maxRateUsPerSecond(rate),reversed(reverse){} };
struct ServoResult { uint16_t pulseUs=1500; bool clamped=false,finite=true; };
class ServoMapper { public: explicit ServoMapper(const ServoTuning& tuning=ServoTuning{}); void reset(); ServoResult map(float normalized,float dtSeconds); uint16_t previousPulseUs()const{return previousUs_;} static bool valid(const ServoTuning& tuning); private: ServoTuning tuning_{}; uint16_t previousUs_=1500; };
class DutyRamp { public: constexpr DutyRamp(float maximum=0.03f,float riseSeconds=.5f,float fallSeconds=.5f):maximum_(maximum),riseSeconds_(riseSeconds),fallSeconds_(fallSeconds){} void setTarget(float duty); float step(float dtSeconds); void stopImmediate(){target_=applied_=0.0f;} float target()const{return target_;} float applied()const{return applied_;} bool active()const{return target_!=applied_;} private: float maximum_,riseSeconds_,fallSeconds_,target_=0.0f,applied_=0.0f; };
}