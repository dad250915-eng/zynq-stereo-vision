"""Pure calculations/protocol state. Importing this module never opens a port."""
from dataclasses import dataclass, field
import math
import re
import time

NUM = r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"
XYZ = re.compile(
    rf"^XYZ RIGHT_RECT frame=([0-9a-fA-F]{{8}}) valid=1 uv=\((\d+),(\d+)\) "
    rf"d_q4=(\d+) X_mm=({NUM}) Y_mm=({NUM}) Z_mm=({NUM}) "
    rf"n=(\d+)/(\d+) support=(\d+)/1000 iqr_q4=(\d+) calc_us=(\d+)\s*$")

def finite(values, name="参数"):
    result = tuple(float(x) for x in values)
    if not all(math.isfinite(x) for x in result):
        raise ValueError(f"{name}必须是有限数字")
    return result

def rotation(abc):
    """Degrees. Rz(C) Ry(B) Rx(A), column-vector convention."""
    a,b,c = map(math.radians, finite(abc, "旋转角"))
    ca,sa,cb,sb,cc,sc = math.cos(a),math.sin(a),math.cos(b),math.sin(b),math.cos(c),math.sin(c)
    return ((cc*cb,cc*sb*sa-sc*ca,cc*sb*ca+sc*sa),
            (sc*cb,sc*sb*sa+cc*ca,sc*sb*ca-cc*sa),(-sb,cb*sa,cb*ca))

def matvec(r,p):
    return tuple(sum(r[i][j]*p[j] for j in range(3)) for i in range(3))

def checked_rotation(values):
    v=finite(values,"旋转矩阵")
    if len(v)!=9: raise ValueError("旋转矩阵需要9个数字")
    r=tuple(tuple(v[i*3:i*3+3]) for i in range(3))
    for i in range(3):
        for j in range(3):
            if abs(sum(r[i][k]*r[j][k] for k in range(3))-(1 if i==j else 0))>0.002:
                raise ValueError("R必须是正交旋转矩阵；轴映射不能缩放或重复使用同一轴")
    det=(r[0][0]*(r[1][1]*r[2][2]-r[1][2]*r[2][1])
         -r[0][1]*(r[1][0]*r[2][2]-r[1][2]*r[2][0])
         +r[0][2]*(r[1][0]*r[2][1]-r[1][1]*r[2][0]))
    if abs(det-1)>0.002: raise ValueError("R的行列式必须为+1，不能使用镜像坐标系")
    return r

@dataclass(frozen=True)
class Sample:
    frame: int
    xyz: tuple
    uv: tuple
    received: float
    disparity: float
    support: int
    iqr: float

class Tracker:
    def __init__(self): self.reset()
    def reset(self):
        self.epoch=getattr(self,'epoch',0)+1
        self.sample=None; self.last_frame=None; self.reason="尚未收到有效XYZ"
    def invalidate(self,reason):
        self.epoch+=1;self.sample=None;self.reason=reason
    def feed(self,line,now=None):
        now=time.monotonic() if now is None else now
        line=line.strip().replace("\\_","_")
        if any(x in line for x in ("Application stopped", "STREAM STOP", "STREAM PAUSED", "OVERLAY HW STOP")):
            self.invalidate(line);return True
        if "Waiting for rectification maps" in line:
            self.reset();return True
        if not line.startswith("XYZ "): return False
        match=XYZ.fullmatch(line)
        if not match:
            self.invalidate("FPGA报告无效XYZ或数据格式不匹配："+line[:180]);return True
        f,u,v,q,x,y,z,n,total,support,iqr,_=match.groups()
        f=int(f,16); u,v,q,n,total,support,iqr=map(int,(u,v,q,n,total,support,iqr))
        try: xyz=finite((x,y,z),"XYZ")
        except ValueError as exc: self.invalidate(str(exc));return True
        if not (0<=u<1280 and 0<=v<720 and 16<=q<=254*16
                and q/16<=u and 0<n<=total==121 and 800<=support<=1000
                and support==n*1000//total and 0<=iqr<=64 and xyz[2]>0
                and max(map(abs,xyz))<1000000):
            self.invalidate("XYZ范围/窗口质量字段不合格");return True
        if self.last_frame is not None:
            delta=(f-self.last_frame)&0xffffffff
            if delta==0: return True # A replay must not refresh the target age.
            if delta>0x7fffffff:
                self.invalidate("帧号倒退：请重新连接FPGA串口或等待完整启动日志");return True
        self.last_frame=f
        self.sample=Sample(f,xyz,(u,v),now,q/16,support,iqr/16)
        self.reason="有效"
        return True
    def current(self,max_age,now=None):
        now=time.monotonic() if now is None else now
        if self.sample is None: raise ValueError(self.reason)
        if now-self.sample.received>max_age: raise ValueError("XYZ已过期，等待FPGA新帧")
        return self.sample

