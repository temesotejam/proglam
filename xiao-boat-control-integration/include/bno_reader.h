#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>
#include <freertos/queue.h>
namespace bno {
enum class EventType:uint8_t{Accel=1,Gyro=2,Rotation=3,Magnetic=4};
struct Sample { EventType type; uint8_t accuracy=0,sequence=0; uint64_t rxUs=0,sensorUs=0,callbackUs=0,queuePushUs=0; float v[7]{}; };
struct Rate { uint32_t count=0,missing=0; uint8_t previousSequence=0; uint64_t firstUs=0,lastUs=0,previousUs=0,maxGapUs=0; float hz()const{return count>1?1000000.0f*(count-1)/(lastUs-firstUs):0.0f;} void add(uint64_t,uint8_t); };
struct Latest { bool accelValid=false,gyroValid=false,rotationValid=false,magneticValid=false; float ax=0,ay=0,az=0,gx=0,gy=0,gz=0,mx=0,my=0,mz=0,qx=0,qy=0,qz=0,qw=1,roll=0,pitch=0,yaw=0; uint64_t accelUs=0,gyroUs=0,rotationUs=0,magneticUs=0,lastUs=0; };
struct PipelineMetrics { uint32_t callbackEvents=0,decodeErrors=0,eventQueueDrops=0,serviceCalls=0,maxServiceUs=0,accelCallbacks=0,gyroCallbacks=0,magneticCallbacks=0,otherCallbacks=0; uint16_t eventQueueHighWater=0; };
class Reader { public: bool begin(); void poll(void(*)(const Sample&)); void recover(); bool ready()const{return ready_;} uint8_t address()const{return address_;} uint32_t reinitCount()const{return reinitCount_;} const char* fault()const{return fault_;} const Latest& latest()const{return latest_;} const Rate& accelRate()const{return accelRate_;} const Rate& gyroRate()const{return gyroRate_;} const Rate& rotationRate()const{return rotationRate_;} const Rate& magneticRate()const{return magneticRate_;} PipelineMetrics metrics()const; void resetRunStats();
private: struct QueuedEvent { sh2_SensorValue_t value{}; uint64_t rxUs=0,queuePushUs=0; }; static void sensorCallback(void*,sh2_SensorEvent_t*); void onSensorEvent(sh2_SensorEvent_t*); bool registerCallback(); bool enableReports(); bool init(); void handle(const QueuedEvent&,void(*)(const Sample&)); void setFault(const char*); Adafruit_BNO08x sensor_{-1}; QueueHandle_t eventQueue_=nullptr; bool ready_=false; uint8_t address_=0; uint32_t lastReinitMs_=0,reinitCount_=0; char fault_[48]="not initialized"; Latest latest_{}; Rate accelRate_{},gyroRate_{},rotationRate_{},magneticRate_{}; mutable portMUX_TYPE metricsMux_=portMUX_INITIALIZER_UNLOCKED; PipelineMetrics metrics_{}; };
}