#pragma once
#include <Arduino.h>
namespace boat {
constexpr uint8_t kVersion=1; constexpr size_t kMaxPayload=768,kMaxRaw=800,kMaxEncoded=820;
enum class Type:uint8_t{Hello=1,BnoAccel=2,BnoGyro=3,BnoQuaternion=4,TofFrame=5,InaSample=6,VescStatus=7,ActuatorState=8,SystemHealth=9,Event=10,TimeSyncReply=11,GnssRaw=12,GnssFix=13,GnssStatus=14,GnssNav=15,GnssProcessResult=16,CommandAck=17,TimeSyncRequest=18,LinkStatistics=19,Heartbeat=32,Arm=33,Disarm=34,StartTest=35,Stop=36,Estop=37,ClearEstop=38};
struct __attribute__((packed)) Header{uint8_t version,type;uint16_t length;uint32_t sequence,bootId;uint64_t sourceUs;uint16_t flags;};
struct Frame{Header header{};uint8_t payload[kMaxPayload]{};};
uint32_t crc32(const uint8_t*,size_t);
enum NavFlag:uint32_t{NavFixValid=1u<<0,NavNewFix=1u<<1,NavLatValid=1u<<2,NavLonValid=1u<<3,NavAltitudeValid=1u<<4,NavSpeedValid=1u<<5,NavCourseValid=1u<<6,NavHdopValid=1u<<7};
struct __attribute__((packed)) HeartbeatPayload{uint32_t uptimeMs,sequence;uint8_t safetyState,dryRun;uint16_t reserved;};
struct __attribute__((packed)) GnssNavPayload{uint32_t navSequence,fixSequence,flags,utcCentiseconds;int32_t latitudeE7,longitudeE7,altitudeMm,speedMmPerSec,courseE5Deg;uint16_t hdopCenti,satellites;uint8_t fixType,reserved[3];uint64_t generatedUs;uint32_t canonicalCrc;};
struct __attribute__((packed)) GnssProcessResultPayload{uint32_t navSequence,fixSequence,navCanonicalCrc,flags;uint64_t controlReceivedUs,controlSentUs;int32_t originLatitudeE7,originLongitudeE7,northMm,eastMm;int32_t speedMmPerSec,courseMilliRad;uint16_t errorCode,safetyState;uint8_t dryRun,reserved[3];uint32_t canonicalCrc;};
struct __attribute__((packed)) CommandPayload{uint32_t commandId;uint8_t commandType;uint8_t reserved[3];};
struct __attribute__((packed)) CommandAckPayload{uint32_t commandId;uint8_t commandType,disposition,safetyState,dryRun;uint64_t receivedUs,appliedUs;uint16_t reason,reserved;};
struct __attribute__((packed)) TimeSyncRequestPayload{uint32_t sequence;uint64_t t1Us;};
struct __attribute__((packed)) TimeSyncReplyPayload{uint32_t sequence;uint64_t t1Us,t2Us,t3Us;};
inline uint32_t canonicalCrc(const void*value,size_t n){return crc32(static_cast<const uint8_t*>(value),n);} size_t encode(const Header&,const uint8_t*,uint8_t*,size_t); class Decoder{public: bool feed(uint8_t,Frame&);uint32_t crcErrors=0,cobsErrors=0,lengthErrors=0;private:uint8_t encoded_[kMaxEncoded]{};size_t n_=0;};
}
