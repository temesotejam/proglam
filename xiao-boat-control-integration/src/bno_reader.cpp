#include "bno_reader.h"
#include "app_config.h"
#include <esp_timer.h>
#include <math.h>
using namespace app_config;
namespace { TwoWire bnoWire(1); }
namespace bno { namespace { uint64_t nowUs(){return (uint64_t)esp_timer_get_time();} void euler(Latest&x){float n=sqrtf(x.qx*x.qx+x.qy*x.qy+x.qz*x.qz+x.qw*x.qw);if(n<1e-6f)return;float qx=x.qx/n,qy=x.qy/n,qz=x.qz/n,qw=x.qw/n;x.roll=atan2f(2*(qw*qx+qy*qz),1-2*(qx*qx+qy*qy))*180.0f/M_PI;float p=fmaxf(-1.0f,fminf(1.0f,2*(qw*qy-qz*qx)));x.pitch=asinf(p)*180.0f/M_PI;x.yaw=atan2f(2*(qw*qz+qx*qy),1-2*(qy*qy+qz*qz))*180.0f/M_PI;} }
void Rate::add(uint64_t t,uint8_t s){if(count){uint64_t d=t-previousUs;if(d>maxGapUs)maxGapUs=d;missing+=(uint8_t)(s-previousSequence-1);}else firstUs=t;previousUs=lastUs=t;previousSequence=s;count++;}
void Reader::setFault(const char*s){snprintf(fault_,sizeof(fault_),"%s",s);}
PipelineMetrics Reader::metrics()const{PipelineMetrics copy{};portENTER_CRITICAL(&metricsMux_);copy=metrics_;portEXIT_CRITICAL(&metricsMux_);return copy;}
bool Reader::enableReports(){
#if BOAT_EXPERIMENT == 22
  const bool gyro=sensor_.enableReport(SH2_GYROSCOPE_CALIBRATED,10000UL);
  return gyro&&sensor_.enableReport(SH2_ACCELEROMETER,10000UL);
#elif BOAT_EXPERIMENT == 23 || BOAT_EXPERIMENT == 24
  const bool gyro=sensor_.enableReport(SH2_GYROSCOPE_CALIBRATED,10000UL);
  const bool accel=sensor_.enableReport(SH2_ACCELEROMETER,10000UL);
  return gyro&&accel&&sensor_.enableReport(SH2_MAGNETIC_FIELD_CALIBRATED,kMagneticIntervalUs);
#elif BOAT_EXPERIMENT == 21
  const bool gyro=sensor_.enableReport(SH2_GYROSCOPE_CALIBRATED,10000UL);
  return gyro&&sensor_.enableReport(SH2_GAME_ROTATION_VECTOR,10000UL);
#else
  const bool gyro=sensor_.enableReport(SH2_GYROSCOPE_CALIBRATED,kAccelGyroIntervalUs);
#if BOAT_EXPERIMENT == 20
  return gyro&&sensor_.enableReport(SH2_GAME_ROTATION_VECTOR,10000UL);
#elif BOAT_EXPERIMENT == 18
  return gyro&&sensor_.enableReport(SH2_GAME_ROTATION_VECTOR,kRotationIntervalUs);
#elif BOAT_EXPERIMENT == 19
  return gyro&&sensor_.enableReport(SH2_ACCELEROMETER,kAccelGyroIntervalUs);
#else
  return gyro&&sensor_.enableReport(SH2_GAME_ROTATION_VECTOR,kRotationIntervalUs)&&sensor_.enableReport(SH2_ACCELEROMETER,kAccelGyroIntervalUs);
#endif
#endif
}
void Reader::sensorCallback(void*cookie,sh2_SensorEvent_t*event){if(cookie)static_cast<Reader*>(cookie)->onSensorEvent(event);}
void Reader::onSensorEvent(sh2_SensorEvent_t*event){QueuedEvent queued{};if(sh2_decodeSensorEvent(&queued.value,event)!=SH2_OK){portENTER_CRITICAL(&metricsMux_);++metrics_.decodeErrors;portEXIT_CRITICAL(&metricsMux_);return;}queued.rxUs=nowUs();portENTER_CRITICAL(&metricsMux_);++metrics_.callbackEvents;if(queued.value.sensorId==SH2_ACCELEROMETER)++metrics_.accelCallbacks;else if(queued.value.sensorId==SH2_GYROSCOPE_CALIBRATED)++metrics_.gyroCallbacks;else if(queued.value.sensorId==SH2_MAGNETIC_FIELD_CALIBRATED)++metrics_.magneticCallbacks;else ++metrics_.otherCallbacks;portEXIT_CRITICAL(&metricsMux_);queued.queuePushUs=nowUs();if(!eventQueue_||xQueueSend(eventQueue_,&queued,0)!=pdPASS){portENTER_CRITICAL(&metricsMux_);++metrics_.eventQueueDrops;portEXIT_CRITICAL(&metricsMux_);return;}UBaseType_t used=uxQueueMessagesWaiting(eventQueue_);portENTER_CRITICAL(&metricsMux_);if(used>metrics_.eventQueueHighWater)metrics_.eventQueueHighWater=used;portEXIT_CRITICAL(&metricsMux_);}
bool Reader::registerCallback(){return sh2_setSensorCallback(sensorCallback,this)==SH2_OK;}
bool Reader::init(){ready_=false;address_=0;if(!eventQueue_){setFault("BNO event queue unavailable");return false;}xQueueReset(eventQueue_);bnoWire.begin(kBnoSdaPin,kBnoSclPin,kBnoI2cHz);bnoWire.setTimeOut(20);uint8_t a=0;for(uint8_t i=0x08;i<0x78;i++){bnoWire.beginTransmission(i);if(!bnoWire.endTransmission()&&(i==kBnoAddress||i==kBnoAlternateAddress)){a=i;break;}}if(!a){setFault("BNO08X not detected (0x4A/0x4B)");return false;}if(!sensor_.begin_I2C(a,&bnoWire)){setFault("begin_I2C failed");return false;}if(!registerCallback()||!enableReports()){setFault("callback/report setup failed");return false;}address_=a;ready_=true;latest_.lastUs=nowUs();setFault("none");return true;}
bool Reader::begin(){pinMode(kBnoRstPin,OUTPUT);digitalWrite(kBnoRstPin,LOW);delay(10);digitalWrite(kBnoRstPin,HIGH);delay(100);pinMode(kBnoIntPin,INPUT_PULLUP);eventQueue_=xQueueCreate(kBnoEventQueueDepth,sizeof(QueuedEvent));if(!eventQueue_){setFault("BNO event queue allocation failed");return false;}return init();}
void Reader::handle(const QueuedEvent&e,void(*out)(const Sample&)){Sample s{};s.accuracy=e.value.status&3;s.sequence=e.value.sequence;s.rxUs=e.rxUs;s.sensorUs=e.value.timestamp;s.callbackUs=e.rxUs;s.queuePushUs=e.queuePushUs;if(e.value.sensorId==SH2_ACCELEROMETER){s.type=EventType::Accel;s.v[0]=e.value.un.accelerometer.x;s.v[1]=e.value.un.accelerometer.y;s.v[2]=e.value.un.accelerometer.z;latest_.ax=s.v[0];latest_.ay=s.v[1];latest_.az=s.v[2];latest_.accelValid=true;latest_.accelUs=s.rxUs;accelRate_.add(s.rxUs,s.sequence);}else if(e.value.sensorId==SH2_GYROSCOPE_CALIBRATED){s.type=EventType::Gyro;s.v[0]=e.value.un.gyroscope.x;s.v[1]=e.value.un.gyroscope.y;s.v[2]=e.value.un.gyroscope.z;latest_.gx=s.v[0];latest_.gy=s.v[1];latest_.gz=s.v[2];latest_.gyroValid=true;latest_.gyroUs=s.rxUs;gyroRate_.add(s.rxUs,s.sequence);}else if(e.value.sensorId==SH2_GAME_ROTATION_VECTOR){s.type=EventType::Rotation;s.v[0]=e.value.un.gameRotationVector.i;s.v[1]=e.value.un.gameRotationVector.j;s.v[2]=e.value.un.gameRotationVector.k;s.v[3]=e.value.un.gameRotationVector.real;latest_.qx=s.v[0];latest_.qy=s.v[1];latest_.qz=s.v[2];latest_.qw=s.v[3];euler(latest_);s.v[4]=latest_.roll;s.v[5]=latest_.pitch;s.v[6]=latest_.yaw;latest_.rotationValid=true;latest_.rotationUs=s.rxUs;rotationRate_.add(s.rxUs,s.sequence);}else if(e.value.sensorId==SH2_MAGNETIC_FIELD_CALIBRATED){s.type=EventType::Magnetic;s.v[0]=e.value.un.magneticField.x;s.v[1]=e.value.un.magneticField.y;s.v[2]=e.value.un.magneticField.z;latest_.mx=s.v[0];latest_.my=s.v[1];latest_.mz=s.v[2];latest_.magneticValid=true;latest_.magneticUs=s.rxUs;magneticRate_.add(s.rxUs,s.sequence);}else return;latest_.lastUs=s.rxUs;out(s);}
void Reader::poll(void(*out)(const Sample&)){if(!ready_)return;if(sensor_.wasReset()){xQueueReset(eventQueue_);if(!registerCallback()||!enableReports()){ready_=false;setFault("callback/report re-enable failed");return;}}uint64_t started=nowUs();uint8_t calls=0;do{sh2_service();++calls;}while(digitalRead(kBnoIntPin)==LOW&&calls<kBnoServiceCallBudget);uint32_t elapsed=(uint32_t)(nowUs()-started);portENTER_CRITICAL(&metricsMux_);metrics_.serviceCalls+=calls;if(elapsed>metrics_.maxServiceUs)metrics_.maxServiceUs=elapsed;portEXIT_CRITICAL(&metricsMux_);QueuedEvent queued{};while(xQueueReceive(eventQueue_,&queued,0)==pdTRUE)handle(queued,out);}
void Reader::recover(){uint32_t n=millis();if(ready_&&n-(uint32_t)(latest_.lastUs/1000)<=kBnoNoDataTimeoutMs)return;if(n-lastReinitMs_<kReinitIntervalMs)return;lastReinitMs_=n;reinitCount_++;setFault(ready_?"BNO data timeout":"BNO init retry");init();}
void Reader::resetRunStats(){accelRate_={};gyroRate_={};rotationRate_={};magneticRate_={};portENTER_CRITICAL(&metricsMux_);metrics_={};portEXIT_CRITICAL(&metricsMux_);}
}