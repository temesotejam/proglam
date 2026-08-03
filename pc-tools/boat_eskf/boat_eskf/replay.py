def replay(eskf,samples):
    for t,g,a in samples:eskf.predict(t,g,a)
    return eskf