#include "eskf.h"

#include <math.h>
#include <string.h>

#include "app_config.h"

namespace eskf {
namespace {
constexpr float kG = 9.80665f;
constexpr float kPi = 3.14159265358979323846f;
uint32_t ageUs(uint64_t then, uint64_t now) { return then && now >= then ? static_cast<uint32_t>(min<uint64_t>(now - then, UINT32_MAX)) : UINT32_MAX; }
bool allFinite(const float* v, size_t n) { for (size_t i=0;i<n;++i) if (!isfinite(v[i])) return false; return true; }
float clampPositive(float x) { return isfinite(x) && x > 1e-8f ? x : 1e-8f; }
void ecef(double latDeg, double lonDeg, double alt, double out[3]) {
  constexpr double a=6378137.0, e2=6.69437999014e-3;
  const double lat=latDeg*kPi/180.0, lon=lonDeg*kPi/180.0, s=sin(lat), c=cos(lat);
  const double n=a/sqrt(1.0-e2*s*s);
  out[0]=(n+alt)*c*cos(lon); out[1]=(n+alt)*c*sin(lon); out[2]=(n*(1.0-e2)+alt)*s;
}
void nedFromGeodetic(double lat, double lon, double alt, double lat0, double lon0, double alt0, float out[3]) {
  double e[3], e0[3]; ecef(lat,lon,alt,e); ecef(lat0,lon0,alt0,e0);
  const double p=lat0*kPi/180.0,l=lon0*kPi/180.0,dx=e[0]-e0[0],dy=e[1]-e0[1],dz=e[2]-e0[2];
  out[0]=-sin(p)*cos(l)*dx-sin(p)*sin(l)*dy+cos(p)*dz;
  out[1]=-sin(l)*dx+cos(l)*dy;
  out[2]=-cos(p)*cos(l)*dx-cos(p)*sin(l)*dy-sin(p)*dz;
}
}  // namespace

void Shadow::initialiseCovariance() {
  memset(P_,0,sizeof(P_));
  const float diagonal[15]={25,25,25,4,4,4,0.25f,0.25f,0.5f,1,1,1,0.04f,0.04f,0.04f};
  for(uint8_t i=0;i<15;++i) P_[i][i]=diagonal[i];
}
void Shadow::reset(uint8_t reason) {
  portENTER_CRITICAL(&mux_);
  memset(p_,0,sizeof(p_)); memset(v_,0,sizeof(v_)); memset(ba_,0,sizeof(ba_)); memset(bg_,0,sizeof(bg_));
  q_[0]=1;q_[1]=q_[2]=q_[3]=0; memset(gyroSum_,0,sizeof(gyroSum_)); memset(accelSum_,0,sizeof(accelSum_));
  lastImuUs_=lastGnssUs_=lastTofUs_=alignStartUs_=0; lastFixSequence_=0; alignSamples_=0; observationMask_=0;
  ++resetCount_; imuDiag_={}; updateDiag_={}; lastResetReason_=reason; runState_=boat::EskfRunState::Resetting; publicHealth_=boat::EstimateHealth::Invalid;
  initialiseCovariance(); innovation_={}; innovation_.reason=(uint8_t)boat::EskfRejectReason::None;
  portEXIT_CRITICAL(&mux_);
}
void Shadow::normalizeQ() { const float n=sqrtf(q_[0]*q_[0]+q_[1]*q_[1]+q_[2]*q_[2]+q_[3]*q_[3]); if(n<1e-6f){q_[0]=1;q_[1]=q_[2]=q_[3]=0;}else for(float& x:q_)x/=n; }
void Shadow::rotation(float r[3][3]) const {
  const float w=q_[0],x=q_[1],y=q_[2],z=q_[3];
  r[0][0]=1-2*(y*y+z*z); r[0][1]=2*(x*y-z*w); r[0][2]=2*(x*z+y*w);
  r[1][0]=2*(x*y+z*w); r[1][1]=1-2*(x*x+z*z); r[1][2]=2*(y*z-x*w);
  r[2][0]=2*(x*z-y*w); r[2][1]=2*(y*z+x*w); r[2][2]=1-2*(x*x+y*y);
}
void Shadow::setFailure(uint8_t reason) { lastResetReason_=reason; runState_=boat::EskfRunState::Invalid; publicHealth_=boat::EstimateHealth::Invalid; }
void Shadow::rejectImu(uint8_t kind){
  ++imuDiag_.rejectCount; imuDiag_.lastPredictRejectReason=kind;
  if(kind==1)++imuDiag_.rejectDuplicate; else if(kind==2)++imuDiag_.rejectOutOfOrder;
  else if(kind==3)++imuDiag_.rejectStale; else if(kind==4)++imuDiag_.rejectBadDt;
  else if(kind==5)++imuDiag_.rejectNonfinite; else if(kind==6)++imuDiag_.rejectBadNorm;
  else if(kind==7)++imuDiag_.rejectQueueFull;
}
void Shadow::noteImuCandidate(uint64_t receivedUs){portENTER_CRITICAL(&mux_);++imuDiag_.candidateCount;imuDiag_.lastCandidateUs=receivedUs;portEXIT_CRITICAL(&mux_);}
void Shadow::noteImuPair(){portENTER_CRITICAL(&mux_);++imuDiag_.pairCreatedCount;portEXIT_CRITICAL(&mux_);}
void Shadow::rejectImuStale(){portENTER_CRITICAL(&mux_);rejectImu(3);portEXIT_CRITICAL(&mux_);}
ImuDiagnostics Shadow::diagnostics() const {portENTER_CRITICAL(&mux_);ImuDiagnostics d=imuDiag_;portEXIT_CRITICAL(&mux_);return d;}
UpdateDiagnostics Shadow::updateDiagnostics() const {portENTER_CRITICAL(&mux_);UpdateDiagnostics d=updateDiag_;portEXIT_CRITICAL(&mux_);return d;}
bool Shadow::finite() const { return allFinite(p_,3)&&allFinite(v_,3)&&allFinite(q_,4)&&allFinite(ba_,3)&&allFinite(bg_,3)&&allFinite(&P_[0][0],225); }

void Shadow::predict(uint64_t receivedUs,const float gyro[3],const float accel[3],uint32_t reportSeq,uint64_t sensorUs) {
  portENTER_CRITICAL(&mux_);
  ++imuDiag_.predictCallCount; imuDiag_.lastPredictCoreUs=receivedUs;
  imuDiag_.lastPredictSensorUs=sensorUs; imuDiag_.lastPredictReportSeq=reportSeq; imuDiag_.lastPredictRejectReason=0;
  auto badDt=[&](uint8_t reason,uint64_t previous,uint64_t delta){
    if(imuDiag_.firstBadDtReason==0){imuDiag_.firstBadDtReason=reason;imuDiag_.firstBadDtCoreUs=receivedUs;imuDiag_.firstBadDtPreviousUs=previous;imuDiag_.firstBadDtSensorUs=sensorUs;imuDiag_.firstBadDtReportSeq=reportSeq;}
    if(reason==1)++imuDiag_.dtZero; else if(reason==2){++imuDiag_.dtNegative;++imuDiag_.timestampWrapUnhandled;}
    else if(reason==3)++imuDiag_.dtTooSmall; else if(reason==4)++imuDiag_.dtTooLarge; else if(reason==5)++imuDiag_.nonfiniteDt;
    (void)delta;
  };
  if(!allFinite(gyro,3)||!allFinite(accel,3)){rejectImu(5);portEXIT_CRITICAL(&mux_);return;}
  if(!lastImuUs_){++imuDiag_.firstSampleWithoutPrevious;lastImuUs_=receivedUs;imuDiag_.lastInputUs=lastImuUs_;alignStartUs_=receivedUs;runState_=boat::EskfRunState::Aligning;++imuDiag_.predictAcceptCount;imuDiag_.lastAcceptedUs=receivedUs;portEXIT_CRITICAL(&mux_);return;}
  if(receivedUs==lastImuUs_){badDt(1,lastImuUs_,0);rejectImu(1);portEXIT_CRITICAL(&mux_);return;}
  if(receivedUs<lastImuUs_){badDt(2,lastImuUs_,0);++timeReversals_;rejectImu(2);portEXIT_CRITICAL(&mux_);return;}
  const uint64_t dtUs=receivedUs-lastImuUs_;
  if(dtUs<200){badDt(3,lastImuUs_,dtUs);rejectImu(4);lastImuUs_=receivedUs;imuDiag_.lastInputUs=lastImuUs_;portEXIT_CRITICAL(&mux_);return;}
  if(dtUs>100000){badDt(4,lastImuUs_,dtUs);++imuGaps_;rejectImu(4);lastImuUs_=receivedUs;imuDiag_.lastInputUs=lastImuUs_;portEXIT_CRITICAL(&mux_);return;}
  const float dt=dtUs*1e-6f; if(!isfinite(dt)){badDt(5,lastImuUs_,dtUs);rejectImu(5);portEXIT_CRITICAL(&mux_);return;}
  lastImuUs_=receivedUs;imuDiag_.lastInputUs=lastImuUs_;++imuDiag_.predictAcceptCount;imuDiag_.lastAcceptedUs=receivedUs;imuDiag_.lastPredictDtUs=dtUs;
  const float gn=sqrtf(gyro[0]*gyro[0]+gyro[1]*gyro[1]+gyro[2]*gyro[2]),an=sqrtf(accel[0]*accel[0]+accel[1]*accel[1]+accel[2]*accel[2]);
  if(runState_==boat::EskfRunState::Aligning){imuDiag_.alignmentElapsedUs=receivedUs-alignStartUs_;if(gn<0.08f&&fabsf(an-kG)<0.5f){for(uint8_t i=0;i<3;++i){gyroSum_[i]+=gyro[i];accelSum_[i]+=accel[i];}++alignSamples_;imuDiag_.alignmentSampleCount=alignSamples_;}else{rejectImu(6);memset(gyroSum_,0,sizeof(gyroSum_));memset(accelSum_,0,sizeof(accelSum_));alignSamples_=0;imuDiag_.alignmentSampleCount=0;alignStartUs_=receivedUs;imuDiag_.alignmentElapsedUs=0;}if(receivedUs-alignStartUs_>=app_config::kEskfAlignmentUs&&alignSamples_>40){for(uint8_t i=0;i<3;++i)bg_[i]=gyroSum_[i]/alignSamples_;initialiseCovariance();runState_=boat::EskfRunState::Running;publicHealth_=app_config::kEskfMountValid?boat::EstimateHealth::Valid:boat::EstimateHealth::Degraded;}portEXIT_CRITICAL(&mux_);return;}
  if(runState_==boat::EskfRunState::Invalid){portEXIT_CRITICAL(&mux_);return;}
  float w[3],f[3];for(uint8_t i=0;i<3;++i){w[i]=gyro[i]-bg_[i];f[i]=accel[i]-ba_[i];}
  const float qw=q_[0],qx=q_[1],qy=q_[2],qz=q_[3],h=0.5f*dt;
  q_[0]+=(-qx*w[0]-qy*w[1]-qz*w[2])*h;q_[1]+=(qw*w[0]+qy*w[2]-qz*w[1])*h;q_[2]+=(qw*w[1]-qx*w[2]+qz*w[0])*h;q_[3]+=(qw*w[2]+qx*w[1]-qy*w[0])*h;normalizeQ();
  float R[3][3];rotation(R);float a[3]={0,0,kG};for(uint8_t i=0;i<3;++i)for(uint8_t j=0;j<3;++j)a[i]+=R[i][j]*f[j];memcpy(imuDiag_.lastNedAccel,a,sizeof(a));
  for(uint8_t i=0;i<3;++i){p_[i]+=v_[i]*dt+0.5f*a[i]*dt*dt;v_[i]+=a[i]*dt;}
  float F[15][15]{};for(uint8_t i=0;i<15;++i)F[i][i]=1;for(uint8_t i=0;i<3;++i){F[i][i+3]=dt;F[i+3][i+9]=-dt;F[i+6][i+12]=-dt;}float tmp[15][15]{},next[15][15]{};for(uint8_t i=0;i<15;++i)for(uint8_t j=0;j<15;++j)for(uint8_t k=0;k<15;++k)tmp[i][j]+=F[i][k]*P_[k][j];for(uint8_t i=0;i<15;++i)for(uint8_t j=0;j<15;++j)for(uint8_t k=0;k<15;++k)next[i][j]+=tmp[i][k]*F[j][k];for(uint8_t i=0;i<15;++i)for(uint8_t j=0;j<15;++j)P_[i][j]=0.5f*(next[i][j]+next[j][i]);for(uint8_t i=0;i<3;++i){P_[i+3][i+3]+=app_config::kEskfAccelNoise*app_config::kEskfAccelNoise*dt;P_[i+6][i+6]+=app_config::kEskfGyroNoise*app_config::kEskfGyroNoise*dt;P_[i+9][i+9]+=app_config::kEskfAccelBiasRw*app_config::kEskfAccelBiasRw*dt;P_[i+12][i+12]+=app_config::kEskfGyroBiasRw*app_config::kEskfGyroBiasRw*dt;}if(!finite())setFailure((uint8_t)boat::EskfRejectReason::Numerical);portEXIT_CRITICAL(&mux_);
}
bool Shadow::updateLinear(const float* H,const float* residual,const float* noise,uint8_t m,float gate,uint8_t observation,uint64_t measurementUs){
  ++updateDiag_.innovationGenerated;
  float S[4][4]{},inv[4][4]{},K[15][4]{};for(uint8_t i=0;i<m;++i)for(uint8_t j=0;j<m;++j){for(uint8_t a=0;a<15;++a)for(uint8_t b=0;b<15;++b)S[i][j]+=H[i*15+a]*P_[a][b]*H[j*15+b];if(i==j)S[i][j]+=noise[i];inv[i][j]=(i==j);}
  for(uint8_t c=0;c<m;++c){uint8_t pivot=c;for(uint8_t r=c+1;r<m;++r)if(fabsf(S[r][c])>fabsf(S[pivot][c]))pivot=r;if(fabsf(S[pivot][c])<1e-8f){innovation_.reason=(uint8_t)boat::EskfRejectReason::Numerical;++updateDiag_.innovationRejected;++updateDiag_.numerical;return false;}if(pivot!=c)for(uint8_t j=0;j<m;++j){float t=S[c][j];S[c][j]=S[pivot][j];S[pivot][j]=t;t=inv[c][j];inv[c][j]=inv[pivot][j];inv[pivot][j]=t;}float d=S[c][c];for(uint8_t j=0;j<m;++j){S[c][j]/=d;inv[c][j]/=d;}for(uint8_t r=0;r<m;++r)if(r!=c){float f=S[r][c];for(uint8_t j=0;j<m;++j){S[r][j]-=f*S[c][j];inv[r][j]-=f*inv[c][j];}}}
  float nis=0;for(uint8_t i=0;i<m;++i)for(uint8_t j=0;j<m;++j)nis+=residual[i]*inv[i][j]*residual[j];
  innovation_={};innovation_.measurementUs=measurementUs;innovation_.processedUs=lastImuUs_;innovation_.observation=observation;innovation_.dimension=m;innovation_.nis=nis;innovation_.gate=gate;for(uint8_t i=0;i<m;++i)innovation_.residual[i]=residual[i];
  if(!isfinite(nis)||nis>gate){innovation_.reason=(uint8_t)boat::EskfRejectReason::Nis;++updateDiag_.innovationRejected;++updateDiag_.nis;return false;}
  float PHt[15][4]{};for(uint8_t a=0;a<15;++a)for(uint8_t j=0;j<m;++j)for(uint8_t b=0;b<15;++b)PHt[a][j]+=P_[a][b]*H[j*15+b];for(uint8_t a=0;a<15;++a)for(uint8_t i=0;i<m;++i)for(uint8_t j=0;j<m;++j)K[a][i]+=PHt[a][j]*inv[j][i];
  float dx[15]{};for(uint8_t a=0;a<15;++a)for(uint8_t i=0;i<m;++i)dx[a]+=K[a][i]*residual[i];
  for(uint8_t i=0;i<3;++i){p_[i]+=dx[i];v_[i]+=dx[i+3];ba_[i]+=dx[i+9];bg_[i]+=dx[i+12];}
  const float dq[4]={1,0.5f*dx[6],0.5f*dx[7],0.5f*dx[8]},qw=q_[0],qx=q_[1],qy=q_[2],qz=q_[3];q_[0]=qw*dq[0]-qx*dq[1]-qy*dq[2]-qz*dq[3];q_[1]=qw*dq[1]+qx*dq[0]+qy*dq[3]-qz*dq[2];q_[2]=qw*dq[2]-qx*dq[3]+qy*dq[0]+qz*dq[1];q_[3]=qw*dq[3]+qx*dq[2]-qy*dq[1]+qz*dq[0];normalizeQ();
  float A[15][15]{};for(uint8_t i=0;i<15;++i){A[i][i]=1;for(uint8_t j=0;j<m;++j)for(uint8_t k=0;k<15;++k)A[i][k]-=K[i][j]*H[j*15+k];}float AP[15][15]{},N[15][15]{};for(uint8_t i=0;i<15;++i)for(uint8_t j=0;j<15;++j)for(uint8_t k=0;k<15;++k)AP[i][j]+=A[i][k]*P_[k][j];for(uint8_t i=0;i<15;++i)for(uint8_t j=0;j<15;++j)for(uint8_t k=0;k<15;++k)N[i][j]+=AP[i][k]*A[j][k];for(uint8_t i=0;i<15;++i)for(uint8_t l=0;l<15;++l)for(uint8_t j=0;j<m;++j)N[i][l]+=K[i][j]*noise[j]*K[l][j];for(uint8_t i=0;i<15;++i)for(uint8_t j=0;j<15;++j)P_[i][j]=0.5f*(N[i][j]+N[j][i]);
  observationMask_|=observation;innovation_.accepted=1;innovation_.reason=(uint8_t)boat::EskfRejectReason::None;++updateDiag_.innovationAccepted;return finite();
}

bool Shadow::updateGnss(uint64_t measurementUs,uint32_t fixSequence,double lat,double lon,float alt,float speed,float course,bool valid){portENTER_CRITICAL(&mux_);++updateDiag_.gnssCalls;if(!valid||!isfinite(lat)||!isfinite(lon)||!isfinite(speed)||!isfinite(course)){++updateDiag_.gnssRejected;++updateDiag_.invalid;innovation_={};innovation_.observation=boat::EskfObservationGnss;innovation_.reason=(uint8_t)boat::EskfRejectReason::Invalid;portEXIT_CRITICAL(&mux_);return false;}if(fixSequence==lastFixSequence_){++updateDiag_.gnssRejected;++updateDiag_.duplicate;innovation_={};innovation_.observation=boat::EskfObservationGnss;innovation_.reason=(uint8_t)boat::EskfRejectReason::Duplicate;portEXIT_CRITICAL(&mux_);return false;}if(!isfinite(originLatDeg_)){originLatDeg_=lat;originLonDeg_=lon;originAltitudeM_=alt;lastFixSequence_=fixSequence;lastGnssUs_=measurementUs;portEXIT_CRITICAL(&mux_);return true;}float zpos[3];nedFromGeodetic(lat,lon,alt,originLatDeg_,originLonDeg_,originAltitudeM_,zpos);float H[4*15]{},res[4]={zpos[0]-p_[0],zpos[1]-p_[1],speed*cosf(course)-v_[0],speed*sinf(course)-v_[1]},R[4]={app_config::kEskfGnssPositionNoiseM*app_config::kEskfGnssPositionNoiseM,app_config::kEskfGnssPositionNoiseM*app_config::kEskfGnssPositionNoiseM,app_config::kEskfGnssVelocityNoiseMps*app_config::kEskfGnssVelocityNoiseMps,app_config::kEskfGnssVelocityNoiseMps*app_config::kEskfGnssVelocityNoiseMps};H[0]=1;H[16]=1;H[2*15+3]=1;H[3*15+4]=1;bool ok=updateLinear(H,res,R,4,app_config::kEskfGnssNisGate,boat::EskfObservationGnss,measurementUs);if(ok){++updateDiag_.gnssAccepted;lastFixSequence_=fixSequence;lastGnssUs_=measurementUs;}else ++updateDiag_.gnssRejected;portEXIT_CRITICAL(&mux_);return ok;}
bool Shadow::updateTof(uint64_t measurementUs,float range,float spread,uint8_t validZones,bool valid){portENTER_CRITICAL(&mux_);++updateDiag_.tofCalls;if(!valid||validZones<4||range<app_config::kEskfTofMinM||range>app_config::kEskfTofMaxM||spread>app_config::kEskfTofMaxSpreadM){++updateDiag_.tofRejected;++updateDiag_.invalid;innovation_={};innovation_.observation=boat::EskfObservationTof;innovation_.reason=(uint8_t)boat::EskfRejectReason::Invalid;portEXIT_CRITICAL(&mux_);return false;}float Rnb[3][3];rotation(Rnb);const float bz=Rnb[2][0]*app_config::kEskfTofBeamBody[0]+Rnb[2][1]*app_config::kEskfTofBeamBody[1]+Rnb[2][2]*app_config::kEskfTofBeamBody[2];if(bz<0.2f){++updateDiag_.tofRejected;++updateDiag_.geometry;innovation_={};innovation_.observation=boat::EskfObservationTof;innovation_.reason=(uint8_t)boat::EskfRejectReason::Geometry;portEXIT_CRITICAL(&mux_);return false;}const float predicted=(app_config::kEskfWaterPlaneDownM-p_[2])/bz;float H[15]{},res[1]={range-app_config::kEskfTofOffsetM-predicted},noise[1]={app_config::kEskfTofNoiseM*app_config::kEskfTofNoiseM};H[2]=-1.0f/bz;bool ok=updateLinear(H,res,noise,1,app_config::kEskfTofNisGate,boat::EskfObservationTof,measurementUs);if(ok){++updateDiag_.tofAccepted;lastTofUs_=measurementUs;}else ++updateDiag_.tofRejected;portEXIT_CRITICAL(&mux_);return ok;}

boat::EskfStatePayload Shadow::state(uint64_t now) const { boat::EskfStatePayload s{};portENTER_CRITICAL(&mux_);s.estimateUs=now;memcpy(s.positionNedM,p_,sizeof(p_));memcpy(s.velocityNedMps,v_,sizeof(v_));memcpy(s.qNb,q_,sizeof(q_));memcpy(s.accelBiasMps2,ba_,sizeof(ba_));memcpy(s.gyroBiasRadS,bg_,sizeof(bg_));for(uint8_t i=0;i<15;++i)s.stddev[i]=sqrtf(clampPositive(P_[i][i]));s.imuAgeUs=ageUs(lastImuUs_,now);s.gnssAgeUs=ageUs(lastGnssUs_,now);s.tofAgeUs=ageUs(lastTofUs_,now);s.resetCount=resetCount_;s.runState=(uint8_t)runState_;s.health=(uint8_t)((s.imuAgeUs<=app_config::kEskfImuStaleUs&&finite())?publicHealth_:boat::EstimateHealth::Invalid);s.observationMask=observationMask_;s.mountValid=app_config::kEskfMountValid;s.shadowOnly=app_config::kShadowOnly;s.actuatorOutputEnabled=app_config::kActuatorOutputEnabled;s.secondaryBnoState=0;s.inaState=0;portEXIT_CRITICAL(&mux_);return s;}
boat::EskfHealthPayload Shadow::health(uint64_t now) const { boat::EskfHealthPayload h{};portENTER_CRITICAL(&mux_);h.reportUs=now;h.imuAgeUs=ageUs(lastImuUs_,now);h.gnssAgeUs=ageUs(lastGnssUs_,now);h.tofAgeUs=ageUs(lastTofUs_,now);h.resetCount=resetCount_;h.imuGaps=imuGaps_;h.timeReversals=timeReversals_;h.runState=(uint8_t)runState_;h.health=(uint8_t)((h.imuAgeUs<=app_config::kEskfImuStaleUs&&finite())?publicHealth_:boat::EstimateHealth::Invalid);h.primaryBnoState=1;h.secondaryBnoState=0;h.inaState=0;h.covarianceValid=finite();h.finite=finite();h.lastResetReason=lastResetReason_;portEXIT_CRITICAL(&mux_);return h;}
boat::EskfInnovationPayload Shadow::takeInnovation(){portENTER_CRITICAL(&mux_);boat::EskfInnovationPayload x=innovation_;innovation_={};portEXIT_CRITICAL(&mux_);return x;}
boat::EskfInnovationPayload Shadow::lastInnovation() const {portENTER_CRITICAL(&mux_);boat::EskfInnovationPayload x=innovation_;portEXIT_CRITICAL(&mux_);return x;}
}  // namespace eskf