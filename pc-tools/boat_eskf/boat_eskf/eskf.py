from dataclasses import dataclass, field
from math import cos, sqrt
from .coordinates import ned
from .quaternion import exp_so3, multiply, normalize
@dataclass
class ShadowEskf:
    p:list=field(default_factory=lambda:[0.,0.,0.]);v:list=field(default_factory=lambda:[0.,0.,0.]);q:tuple=(1.,0.,0.,0.);ba:list=field(default_factory=lambda:[0.,0.,0.]);bg:list=field(default_factory=lambda:[0.,0.,0.]);P:list=field(default_factory=lambda:[1.]*15);origin:tuple=None;last_fix:int=None;last_us:int=None;resets:int=0;time_reversals:int=0
    def reset(self): self.__dict__.update(ShadowEskf().__dict__);self.resets+=1
    def predict(self,t_us,gyro,accel):
        if self.last_us is None:self.last_us=t_us;return False
        if t_us<=self.last_us:self.time_reversals+=1;return False
        dt=min(.05,(t_us-self.last_us)/1e6);self.last_us=t_us
        w=[gyro[i]-self.bg[i] for i in range(3)];self.q=normalize(multiply(self.q,exp_so3(tuple(dt*x for x in w))))
        # FRD/NED: accelerometer is specific force, gravity is +down.
        a=[accel[0]-self.ba[0],accel[1]-self.ba[1],accel[2]-self.ba[2]+9.80665]
        for i in range(3):self.p[i]+=self.v[i]*dt+.5*a[i]*dt*dt;self.v[i]+=a[i]*dt;self.P[i]+=dt*self.P[i+3]+.01;self.P[i+3]+=.1*dt
        return True
    def gnss(self,t_us,fix,lat,lon,alt,speed,course,valid=True):
        if not valid or fix==self.last_fix:return False
        if self.origin is None:self.origin=(lat,lon,alt);self.last_fix=fix;return True
        z=ned(lat,lon,alt,self.origin);obs=[z[0],z[1],speed*cos(course),speed*__import__('math').sin(course)]
        x=[self.p[0],self.p[1],self.v[0],self.v[1]]
        for i in range(4):
            k=self.P[i]/(self.P[i]+(9 if i<2 else .64));x[i]+=k*(obs[i]-x[i]);self.P[i]*=(1-k)
        self.p[0],self.p[1],self.v[0],self.v[1]=x;self.last_fix=fix;return True
    def tof(self,range_m,spread,zones,beam_down=1.):
        if zones<4 or not .2<=range_m<=4 or spread>.25 or beam_down<=.2:return False
        pred=-self.p[2]/beam_down;nis=(range_m-pred)**2/(.15**2+self.P[2])
        if nis>6.63:return False
        k=self.P[2]/(self.P[2]+.15**2);self.p[2]+=k*(pred-range_m)*beam_down;self.P[2]*=1-k;return True