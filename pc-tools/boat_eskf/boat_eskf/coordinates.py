from math import cos, pi, sin, sqrt
A=6378137.0; E2=6.69437999014e-3
def ecef(lat_deg,lon_deg,alt):
    p=lat_deg*pi/180; l=lon_deg*pi/180; n=A/sqrt(1-E2*sin(p)**2)
    return ((n+alt)*cos(p)*cos(l),(n+alt)*cos(p)*sin(l),(n*(1-E2)+alt)*sin(p))
def ned(lat,lon,alt,origin):
    x,y,z=ecef(lat,lon,alt);x0,y0,z0=ecef(*origin);p=origin[0]*pi/180;l=origin[1]*pi/180;dx,dy,dz=x-x0,y-y0,z-z0
    return (-sin(p)*cos(l)*dx-sin(p)*sin(l)*dy+cos(p)*dz,-sin(l)*dx+cos(l)*dy,-cos(p)*cos(l)*dx-cos(p)*sin(l)*dy-sin(p)*dz)