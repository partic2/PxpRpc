
import typing

zero32=bytes.fromhex('00000000')

import struct

def encodeToBytes(obj,addr32:int):
    t1=type(obj)
    if t1==int:
        return obj.to_bytes(8,'little')
    elif t1==float:
        return struct.pack('<d',obj)
    elif t1==bool:
        return bytes([0,0,0,1]) if obj else bytes([0,0,0,0])
    else:
        return addr32.to_bytes(4,'little')
    
