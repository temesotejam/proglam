#include <cassert>
#include <cstdio>
#include <cmath>
#include "competition_actuators.h"
int main(){using namespace competition_shadow;ServoTuning t{};assert(ServoMapper::valid(t));ServoMapper m(t);assert(m.map(0,.02f).pulseUs==1500);assert(m.map(1,.02f).pulseUs==1502);for(int i=0;i<30;++i)m.map(1,.02f);assert(m.previousPulseUs()==1520);assert(m.map(100,.02f).clamped);assert(!m.map(NAN,.02f).finite);t.reversed=true;ServoMapper r(t);for(int i=0;i<3;++i)r.map(1,.10f);assert(r.previousPulseUs()==1480);std::puts("COMPETITION_ACTUATORS_HOST_PASS narrow_limits=ok rate_limit=ok reverse=ok");}