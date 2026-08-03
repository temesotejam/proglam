#pragma once

#include <Arduino.h>
#include <boat_protocol.h>

namespace eskf {

struct ImuDiagnostics {
  uint32_t candidateCount=0,pairCreatedCount=0,predictCallCount=0,predictAcceptCount=0,rejectCount=0,
    rejectDuplicate=0,rejectOutOfOrder=0,rejectStale=0,rejectBadDt=0,rejectNonfinite=0,
    rejectBadNorm=0,rejectQueueFull=0,alignmentSampleCount=0;
  uint32_t dtZero=0,dtNegative=0,dtTooSmall=0,dtTooLarge=0,timestampWrapUnhandled=0,
    timestampSourceChanged=0,firstSampleWithoutPrevious=0,nonfiniteDt=0;
  uint64_t lastCandidateUs=0,lastAcceptedUs=0,lastPredictDtUs=0,alignmentElapsedUs=0,
    lastInputUs=0,lastPredictCoreUs=0,lastPredictSensorUs=0,firstBadDtCoreUs=0,
    firstBadDtPreviousUs=0,firstBadDtSensorUs=0;
  uint32_t lastPredictReportSeq=0,firstBadDtReportSeq=0;
  uint8_t lastPredictRejectReason=0,firstBadDtReason=0;
  float lastNedAccel[3]{};
};
struct UpdateDiagnostics {
  uint32_t gnssCalls=0,gnssAccepted=0,gnssRejected=0,tofCalls=0,tofAccepted=0,tofRejected=0,
    innovationGenerated=0,innovationAccepted=0,innovationRejected=0;
  uint32_t invalid=0,duplicate=0,geometry=0,nis=0,numerical=0;
};

// Nominal state: p_n, v_n, q_nb, b_a, b_g. Error state: [dp,dv,dtheta,dba,dbg].
class Shadow {
 public:
  void reset(uint8_t reason = 0);
  void noteImuCandidate(uint64_t receivedUs); void noteImuPair(); void rejectImuStale();
  void predict(uint64_t receivedUs, const float gyroRadS[3], const float accelMps2[3], uint32_t reportSeq=0, uint64_t sensorUs=0);
  ImuDiagnostics diagnostics() const; UpdateDiagnostics updateDiagnostics() const;
  bool updateGnss(uint64_t measurementUs, uint32_t fixSequence, double latitudeDeg,
                  double longitudeDeg, float altitudeM, float speedMps,
                  float courseRad, bool valid);
  bool updateTof(uint64_t measurementUs, float rangeM, float spreadM,
                 uint8_t validZones, bool valid);
  boat::EskfStatePayload state(uint64_t nowUs) const;
  boat::EskfHealthPayload health(uint64_t nowUs) const;
  boat::EskfInnovationPayload takeInnovation();
  boat::EskfInnovationPayload lastInnovation() const;

 private:
  void initialiseCovariance();
  void normalizeQ();
  void rotation(float r[3][3]) const;
  void setFailure(uint8_t reason);
  bool updateLinear(const float* h, const float* residual, const float* r,
                    uint8_t dimension, float gate, uint8_t observation,
                    uint64_t measurementUs);
  bool finite() const; void rejectImu(uint8_t kind);

  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  float p_[3]{}, v_[3]{}, q_[4]{1,0,0,0}, ba_[3]{}, bg_[3]{};
  float P_[15][15]{};
  uint64_t lastImuUs_=0, lastGnssUs_=0, lastTofUs_=0, alignStartUs_=0;
  uint32_t lastFixSequence_=0, resetCount_=0, imuGaps_=0, timeReversals_=0;
  double originLatDeg_=NAN, originLonDeg_=NAN; float originAltitudeM_=NAN;
  float gyroSum_[3]{}, accelSum_[3]{}; uint16_t alignSamples_=0;
  boat::EskfRunState runState_=boat::EskfRunState::Resetting;
  boat::EstimateHealth publicHealth_=boat::EstimateHealth::Invalid;
  uint8_t lastResetReason_=0, observationMask_=0;
  boat::EskfInnovationPayload innovation_{}; ImuDiagnostics imuDiag_{}; UpdateDiagnostics updateDiag_{};
};

}  // namespace eskf