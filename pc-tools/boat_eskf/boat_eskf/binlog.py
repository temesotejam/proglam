import struct
MAGIC=0x424C4F47
HEADER='<BBHIIQH'; HEADER_SIZE=struct.calcsize(HEADER)
def records(data:bytes):
    i=0
    while i+12+HEADER_SIZE<=len(data):
        magic,=struct.unpack_from('<I',data,i)
        if magic!=MAGIC:i=data.find(struct.pack('<I',MAGIC),i+1);i=len(data) if i<0 else i;continue
        queued,=struct.unpack_from('<Q',data,i+4);ver,typ,length,seq,boot,source,flags=struct.unpack_from(HEADER,data,i+12);end=i+12+HEADER_SIZE+length
        if end>len(data):break
        yield {'queue_us':queued,'version':ver,'type':typ,'length':length,'sequence':seq,'boot_id':boot,'source_us':source,'flags':flags,'payload':data[i+12+HEADER_SIZE:end]};i=end