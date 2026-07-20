#include "ina226_reader.h"
#include <Wire.h>
#include <esp_timer.h>
#include "app_config.h"
using namespace cfg;
bool Ina226::read16(uint8_t r,uint16_t&v){Wire.beginTransmission(kAddr);Wire.write(r);if(Wire.endTransmission(false)!=0)return false;if(Wire.requestFrom((int)kAddr,2)!=2)return false;v=((uint16_t)Wire.read()<<8)|Wire.read();return true;}
bool Ina226::write16(uint8_t r,uint16_t v){Wire.beginTransmission(kAddr);Wire.write(r);Wire.write(v>>8);Wire.write(v);return Wire.endTransmission()==0;}
bool Ina226::present(){uint16_t x=0;return read16(0xFE,x)&&x==0x5449;}
bool Ina226::begin(){++reinit; uint16_t x=0;if(!read16(0xFE,manufacturer)||manufacturer!=0x5449||!read16(0xFF,die)||die!=0x2260)return false;if(!write16(0x00,kConfig)||!write16(0x05,kCalibration))return false;read16(0x00,configuration);read16(0x05,calibration);return (configuration & 0xBFFFu)==kConfig&&calibration==kCalibration;}
bool Ina226::read(InaSample&s){uint64_t t=esp_timer_get_time();uint16_t a,b,c,d,e;if(!(read16(0x01,a)&&read16(0x02,b)&&read16(0x03,c)&&read16(0x04,d)&&read16(0x06,e))){++errors;return false;}s.us=t;s.rawShunt=(int16_t)a;s.rawBus=b;s.rawPower=c;s.rawCurrent=(int16_t)d;s.mask=e;s.shuntV=s.rawShunt*2.5e-6f;s.busV=s.rawBus*1.25e-3f;s.currentShuntA=s.shuntV/kShuntOhm;s.currentRegA=s.rawCurrent*kCurrentLsbA;s.powerRegW=s.rawPower*kPowerLsbW;s.powerCalcW=s.busV*s.currentShuntA;s.i2cUs=(uint32_t)(esp_timer_get_time()-t);s.valid=true;return true;}