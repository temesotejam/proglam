from math import acos, cos, sin, sqrt

def normalize(q):
    n=sqrt(sum(x*x for x in q))
    if n < 1e-12: return (1.,0.,0.,0.)
    return tuple(x/n for x in q)
def multiply(a,b):
    w,x,y,z=a; W,X,Y,Z=b
    return (w*W-x*X-y*Y-z*Z,w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W)
def inverse(q):
    w,x,y,z=q; n=w*w+x*x+y*y+z*z
    return (w/n,-x/n,-y/n,-z/n)
def exp_so3(v):
    a=sqrt(sum(x*x for x in v))
    return (1.,v[0]/2,v[1]/2,v[2]/2) if a<1e-9 else (cos(a/2),sin(a/2)*v[0]/a,sin(a/2)*v[1]/a,sin(a/2)*v[2]/a)
def log_so3(q):
    w,x,y,z=normalize(q); a=2*acos(max(-1.,min(1.,w))); s=sqrt(max(1e-16,1-w*w))
    return (0.,0.,0.) if a<1e-9 else (a*x/s,a*y/s,a*z/s)
def rotate(q,v):
    return multiply(multiply(q,(0.,*v)),inverse(q))[1:]
def wrap_pi(x):
    from math import pi
    while x>pi:x-=2*pi
    while x<=-pi:x+=2*pi
    return x