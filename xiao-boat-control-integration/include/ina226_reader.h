#pragma once
#include <Arduino.h>
struct InaSample{uint64_t us=0;int16_t rawShunt=0,rawCurrent=0;uint16_t rawBus=0,rawPower=0,mask=0;float shuntV=NAN,busV=NAN,currentShuntA=NAN,currentRegA=NAN,powerRegW=NAN,powerCalcW=NAN;uint32_t i2cUs=0;bool valid=false;};
class Ina226 {public: bool begin();bool read(InaSample&);bool present();uint16_t manufacturer=0,die=0,configuration=0,calibration=0;uint32_t errors=0,reinit=0;private:bool read16(uint8_t,uint16_t&);bool write16(uint8_t,uint16_t);};