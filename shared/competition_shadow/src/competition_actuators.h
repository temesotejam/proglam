#pragma once
#include <stdint.h>
namespace competition_shadow {
struct ServoTuning { float minUs,neutralUs,maxUs,maxRateUsPerSecond; bool reversed; constexpr ServoTuning(float min=1480.0f,float neutral=1500.0f,float max=1520.0f,float rate=100.0f,bool reverse=false):minUs(min),neutralUs(neutral),maxUs(max),maxRateUsPerSecond(rate),reversed(reverse){} };
struct ServoResult { uint16_t pulseUs=1500; bool clamped=false,finite=true; };
class ServoMapper { public: explicit ServoMapper(const ServoTuning& tuning=ServoTuning{}); void reset(); ServoResult map(float normalized,float dtSeconds); uint16_t previousPulseUs()const{return previousUs_;} static bool valid(const ServoTuning& tuning); private: ServoTuning tuning_{}; uint16_t previousUs_=1500; };
}