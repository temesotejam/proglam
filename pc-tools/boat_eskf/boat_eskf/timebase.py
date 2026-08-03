def estimate_offset(t1,t2,t3,t4):
    rtt=max(0,(t4-t1)-(t3-t2));return ((t2-t1)+(t3-t4))//2,rtt,rtt//2