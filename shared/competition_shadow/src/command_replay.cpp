#include "command_replay.h"
namespace competition_shadow {
bool CommandReplayWindow::newer(uint32_t a,uint32_t b){return static_cast<int32_t>(a-b)>0;}
bool CommandReplayWindow::ambiguous(uint32_t a,uint32_t b){return static_cast<uint32_t>(a-b)==0x80000000u;}
ReplayDecision CommandReplayWindow::inspect(uint32_t id,uint32_t seq,uint8_t type,uint8_t ver,uint16_t len,uint32_t fp,const ReplayEntry*& match)const{match=nullptr;for(const auto&e:entries_){if(!e.valid)continue;if(e.requestId==id&&e.sequence==seq&&e.type==type&&e.version==ver&&e.length==len&&e.fingerprint==fp){match=&e;++duplicateCount_;return ReplayDecision::Duplicate;}if(e.requestId==id||e.sequence==seq){++conflictCount_;return ReplayDecision::ProtocolConflict;}}if(hasHigh_&&(ambiguous(seq,high_)||!newer(seq,high_))){++staleCount_;return ReplayDecision::Stale;}return ReplayDecision::New;}
void CommandReplayWindow::store(uint32_t id,uint32_t seq,uint8_t type,uint8_t ver,uint16_t len,uint32_t fp,uint8_t disposition,uint16_t reason,uint64_t appliedUs){entries_[next_]={true,id,seq,fp,type,ver,len,reason,disposition,appliedUs};next_=(next_+1)%kCapacity;if(!hasHigh_||newer(seq,high_)){high_=seq;hasHigh_=true;}++appliedCount_;}
} // namespace competition_shadow
