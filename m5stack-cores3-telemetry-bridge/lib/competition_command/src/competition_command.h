#pragma once
#include <stdint.h>
#include "boat_protocol.h"
namespace competition_command {
enum class State:uint8_t { Empty=0,Queued=1,WaitingAck=2,AckApplied=3,AckRejected=4,AckDuplicate=5,ProtocolError=6,TimedOut=7 };
struct Config { uint32_t ackTimeoutUs=100000; uint8_t maxTransmissions=3; uint8_t slots=8; };
struct PendingCommand { bool valid=false; State state=State::Empty; uint8_t type=0; uint32_t requestId=0,commandSequence=0,frameSequence=0; uint64_t sourceUs=0,initialTxUs=0,lastTxUs=0,ackDeadlineUs=0,xiaoAppliedUs=0; uint8_t transmissions=0,ackDisposition=0; uint16_t ackReason=0,payloadLength=0,wireLength=0; uint8_t payload[sizeof(boat::ManualCommandPayload)]{}; uint8_t wire[boat::kMaxEncoded]{}; };
struct Diagnostics { uint32_t requests=0,validationRejects=0,queued=0,initialTx=0,retryTx=0,ackReceived=0,ackMatched=0,ackUnmatched=0,ackMalformed=0,ackDuplicate=0,ackLate=0,ackApplied=0,ackRejected=0,protocolConflict=0,stale=0,timeouts=0,queueOverflow=0,manualInputStale=0,manualSamples=0,manualRetries=0,physicalWrites=0; };
class Manager {
 public:
  static constexpr uint8_t kSlots=8;
  explicit Manager(Config c=Config{}):config_(c){}
  bool queueMode(uint8_t mode,uint32_t requestId,uint32_t sequence,uint32_t frameSequence,uint64_t sourceUs,uint32_t bootId);
  bool queueManual(float left,float right,float rear,float propulsion,uint32_t requestId,uint32_t sequence,uint32_t frameSequence,uint64_t sourceUs,uint32_t bootId);
  bool queueHeading(float targetYaw,uint32_t requestId,uint32_t sequence,uint32_t frameSequence,uint64_t sourceUs,uint32_t bootId);
  bool service(uint64_t nowUs,size_t(*write)(const uint8_t*,size_t,void*),void* context);
  bool handleAck(const boat::Frame& frame,uint64_t nowUs);
  const Diagnostics& diagnostics()const{return diagnostics_;}
  const PendingCommand* entries()const{return entries_;}
  PendingCommand* entries(){return entries_;}
  bool hasPendingManual()const; void noteManualInputStale(){++diagnostics_.manualInputStale;}
 private:
  bool queue(uint8_t type,const void* payload,uint16_t payloadLength,uint32_t requestId,uint32_t sequence,uint32_t frameSequence,uint64_t sourceUs,uint32_t bootId);
  PendingCommand* allocate();
  Config config_{}; PendingCommand entries_[kSlots]{}; Diagnostics diagnostics_{};
};
const char* stateName(State state);
} // namespace competition_command