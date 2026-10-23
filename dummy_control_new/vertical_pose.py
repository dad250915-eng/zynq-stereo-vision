"""Reference source attitude R/P/Y=(0,180,0), no tool-position offset.
This matches the supplied reference program, not a verified physical calibration.
"""
import math
from controller import numbers,angle_error,rotation
import numpy as np
from kinematics import fk,LOW,HIGH

DOWN_ABC=(0.,180.,0.)
GRIPPER_AXIS=(0.,0.,1.)

def validate_down(xyz,joints):
    xyz=numbers(xyz,3);q=tuple(round(v,3) for v in numbers(joints,6));p=fk(q)
    if any(v<lo or v>hi for v,lo,hi in zip(q,LOW,HIGH)) or math.dist(xyz,p[:3])>.1 or angle_error(p[3:],DOWN_ABC)>.2:
        raise ValueError('垂直姿态校验失败：不发送倾斜或错误位置的目标')
    return q,p

def solve_down(xyz,current):
    from staged_motion import solve_pose
    xyz=numbers(xyz,3);current=numbers(current,6)
    if math.dist(xyz,(0,0,0))>700:raise ValueError('垂直目标超出参考机构长度，未发送')
    valid=[]
    for candidate in down_branches(xyz):
        try:valid.append(validate_down(xyz,candidate))
        except ValueError:pass
    if valid:
        # Prefer minimum largest joint travel among all verified branches.
        return min(valid,key=lambda result:max(abs(a-b) for a,b in zip(result[0],current))+.05*math.dist(result[0],current))
    try:q,p=solve_pose(xyz+DOWN_ABC,current)
    except ValueError as exc:
        raise ValueError(f'XYZ={xyz}：'+limit_diagnostic(xyz)+'。不放宽限位、不改为倾斜姿态。') from exc
    return validate_down(xyz,q)


def down_branches(xyz):
    """Geometric branches for this module's fixed rotation, no joint clipping.
    Shoulder/elbow triangle follows supplied Dummy SolveIK geometry.
    Every candidate is independently checked using the existing full FK.
    Singular base-axis targets are left to the numerical solver.
    """
    x,y,z=numbers(xyz,3);rho=math.hypot(x,y)
    if rho<1e-6:return []
    a=146.;b=math.hypot(117.,52.);out=[]
    def wrap(v):return (v+180.)%360.-180.
    for j1,rad in ((math.degrees(math.atan2(y,x)),rho),(math.degrees(math.atan2(-y,-x)),-rho)):
        u=rad-35.;v=134.-(z+62.5);d=math.hypot(u,v)
        if d<1e-8 or d>a+b+1e-8 or d<abs(a-b)-1e-8:continue
        ac=lambda v:math.acos(max(-1.,min(1.,v)))
        alpha=ac((a*a+d*d-b*b)/(2*a*d));elbow=ac((a*a+b*b-d*d)/(2*a*b));phi=math.atan2(v,u)
        for sgn in (-1,1):
            j2=wrap(math.degrees(phi+sgn*alpha+math.pi/2))
            j3=wrap(math.degrees(math.atan2(52.,117.)+sgn*(elbow-math.pi)))
            r03=np.asarray(rotation(fk((j1,j2,j3,0,0,0))[3:]))
            w=r03.T@np.asarray(rotation(DOWN_ABC))
            beta=math.acos(max(-1.,min(1.,w[2,2])))
            if abs(math.sin(beta))<1e-8:
                if w[2,2]<0:continue # Outside the existing J5 range.
                wrists=[(0.,0.,math.degrees(math.atan2(w[1,0],w[0,0])))]
            else:
                wrists=[(math.degrees(math.atan2(sign*w[1,2],sign*w[0,2])),sign*math.degrees(beta),math.degrees(math.atan2(sign*w[2,1],-sign*w[2,0]))) for sign in (1,-1)]
            for wrist in wrists:
                q=(j1,j2,j3)+tuple(wrap(v) for v in wrist);p=fk(q)
                if math.dist(p[:3],(x,y,z))<1e-5 and angle_error(p[3:],DOWN_ABC)<1e-4:out.append(q)
    return out

def limit_diagnostic(xyz):
    branches=down_branches(xyz)
    def excess(q):return sum(max(lo-v,0.,v-hi) for v,lo,hi in zip(q,LOW,HIGH))
    if not branches:return '未找到有效几何分支；需核对目标、模型及安装方向'
    q=min(branches,key=excess)
    if excess(q)<1e-7:return '几何分支存在限位内解，但数值求解未收敛；未发送'
    details=[f'J{i+1}需要{v:.2f}°，范围[{lo:g}, {hi:g}]°' for i,(v,lo,hi) in enumerate(zip(q,LOW,HIGH)) if v<lo-1e-7 or v>hi+1e-7]
    return '已核对肩/肘/腕分支，当前姿态均不满足限位；最接近的分支：'+'；'.join(details)
