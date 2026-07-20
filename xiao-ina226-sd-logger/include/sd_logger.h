#pragma once
#include <Arduino.h>
#include <SD.h>
#include "app_config.h"
namespace vlog {
enum class Type:uint8_t{InaConfig=1,InaMeasurement=2,InaStatus=3,SystemStatus=4,LogEnd=9}; enum class State:uint8_t{NoCard,Ready,Logging,Stopping,Error};
struct Event{Type type=Type::SystemStatus;uint8_t flags=0;uint64_t rxUs=0,requestUs=0;uint32_t roundTripUs=0,packetSequence=0;uint8_t payload[cfg::kPayloadBytes]{};};
struct Stats{uint32_t saved=0,dropped=0,writeErrors=0,queueUsed=0,queueHighWater=0;bool mounted=false;char runName[16]="";char fault[48]="not initialized";};
class Logger{public:bool begin();void requestStart();void requestStop();void service();bool enqueue(const Event&);bool isLogging()const{return state_==State::Logging;}State state()const{return state_;}const char*stateName()const;const Stats&stats()const{return stats_;}bool writeSummary(const char*text);private:static void task(void*);void writer();bool pop(Event&);bool openRun();void closeRun();void header();void record(uint8_t*,const Event&,uint32_t);void setFault(const char*);uint32_t crc(const uint8_t*,size_t)const;Event q_[cfg::kLogQueueCapacity];volatile uint16_t head_=0,tail_=0,count_=0;portMUX_TYPE mux_=portMUX_INITIALIZER_UNLOCKED;File file_;volatile bool busy_=false,start_=false,stop_=false;State state_=State::NoCard;Stats stats_{};uint32_t seq_=0,lastFlushMs_=0;};
}