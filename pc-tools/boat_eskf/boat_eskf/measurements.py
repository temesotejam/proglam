def tof_quality(ranges,statuses):
    x=[r/1000 for r,s in zip(ranges,statuses) if 200<=r<=4000 and s in (5,9)]
    if not x:return 0,float('nan'),float('nan')
    m=sum(x)/len(x);return len(x),m,(sum((v-m)**2 for v in x)/len(x))**.5