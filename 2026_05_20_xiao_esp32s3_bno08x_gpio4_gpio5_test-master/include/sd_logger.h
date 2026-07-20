#pragma once
#include <Arduino.h>
#include <SD.h>
#include "app_config.h"
namespace bnolog {
constexpr size_t kPayloadBytes=118;
enum class RecordType:uint8_t { Accel=1, Gyro=2, Rotation=3, System=4, Reinit=5, I2cStall=6, GnssRaw=7, GnssFix=8, GnssStatus=9 };
enum class State:uint8_t { NoCard, Ready, Logging, Stopping, Error };
struct Event { RecordType type=RecordType::System; uint8_t accuracy=0,sensorSequence=0; uint16_t faultCode=0; uint64_t rxUs=0; uint32_t deltaUs=0; uint64_t sensorUs=0; bool sensorTimeValid=false; uint8_t payload[kPayloadBytes]{}; };
struct Stats { uint32_t saved=0,dropped=0,writeErrors=0,queueUsed=0,queueHighWater=0; uint32_t maxWriteUs=0,lastFlushMs=0; bool mounted=false; uint8_t cardType=0; uint64_t cardBytes=0,usedBytes=0; char runName[16]=""; char fault[48]="not initialized"; };
class Logger { public: bool begin(); void requestStart(); void requestStop(); void service(); bool enqueue(const Event&); bool isLogging()const; State state()const; const Stats& stats()const; bool writeSummary(const char*); const char* stateName()const;
private: static void writerEntry(void*); void writerLoop(); bool pop(Event&); bool openRun(); void closeRun(); void setFault(const char*); void writeHeader(); void serializeRecord(uint8_t*,const Event&,uint32_t); uint32_t crc32(const uint8_t*,size_t)const; Event queue_[app_config::kLogQueueCapacity]; volatile uint16_t head_=0,tail_=0,count_=0; portMUX_TYPE mux_=portMUX_INITIALIZER_UNLOCKED; File file_; volatile bool writerBusy_=false,startRequested_=false,stopRequested_=false; State state_=State::NoCard; Stats stats_; uint32_t sequence_=0,lastFlushMs_=0; };
}