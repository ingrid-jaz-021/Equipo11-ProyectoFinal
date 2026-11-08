"""Generate a seamless 1024x2048 wood-plank PNG using only Python's stdlib."""
import math,struct,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
WIDTH,HEIGHT=1024,2048
def noise(x,y):
    n=(((x+7919)*374761393)^((y+127)*668265263))&0xffffffff
    n=((n^(n>>13))*1274126177)&0xffffffff
    return ((n^(n>>16))&0xffffffff)/4294967295
def chunk(name,data):
    return struct.pack('>I',len(data))+name+data+struct.pack('>I',zlib.crc32(name+data)&0xffffffff)
def main():
    pixels=bytearray();tau=2*math.pi
    for y in range(HEIGHT):
        pixels.append(0)
        t=y/HEIGHT
        for x in range(WIDTH):
            board=x//256;s=(x%256)/256;phase=board*1.731
            grain=math.sin(tau*(s*31+1.3*math.sin(tau*t+phase)+.4*math.sin(3*tau*t-phase)))
            fine=math.sin(tau*(s*139+.7*math.sin(2*tau*t+phase)))
            knots=math.sin(tau*(s*7+.4*math.sin(tau*t+phase)))
            light=grain*7+fine*2+knots*3+(noise(x,y)-.5)*6+math.sin(phase)*9
            seam=min(x%256,255-x%256);end=(y+board*683)%HEIGHT
            dark=.30 if seam<2 else .7 if seam<4 else .55 if end<3 or end>HEIGHT-4 else 1
            pixels.extend(max(0,min(255,int(value*dark))) for value in (143+light,86+light*.68,43+light*.4))
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',WIDTH,HEIGHT,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(pixels,9))+chunk(b'IEND',b'')
    (ROOT/'images/MuelleMadera.png').write_bytes(png)
if __name__=='__main__':main()
