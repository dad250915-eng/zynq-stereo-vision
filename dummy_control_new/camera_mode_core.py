"""Read-only camera capture and one-shot stable target. No hardware actions."""
import json, math
from collections import deque
from pathlib import Path
import numpy as np
from camera_xyz_parser import Tracker

CAMERA_DEPTH_OFFSET_MM = 40.0

class CameraCapture:
    def __init__(self, calibration):
        self.cal=calibration; self.tracker=Tracker();self.samples=deque(maxlen=256)
        self.depth_offset_mm=float(calibration.get('camera_depth_offset_mm',CAMERA_DEPTH_OFFSET_MM))
        if not math.isfinite(self.depth_offset_mm):raise ValueError('相机深度补偿必须是有限数值')
        self.locked=None;self.grasp_camera=None;self.target=None;self.armed=True;self.used=False
        self.reason='等待连续5帧稳定坐标';self.last_identity=None
        self.accept_after=None
        self.filter_candidates=False;self.freeze_on_lock=False;self.frozen=False
    def reset(self):
        self.tracker.reset();self.samples.clear();self.locked=self.grasp_camera=self.target=None
        self.armed=True;self.used=False;self.last_identity=None;self.reason='重新采集：等待连续5帧稳定坐标'
        self.accept_after=None
        self.filter_candidates=False;self.freeze_on_lock=False;self.frozen=False
    def rearm_after(self,now):
        self.reset();self.accept_after=now
        self.filter_candidates=True;self.freeze_on_lock=True
        self.reason='第一次放置完成：等待第二次的5帧新坐标'
    def within_calibration(self,point):
        lo=np.min(self.cal['camera_samples'],axis=(0,1))-20
        hi=np.max(self.cal['camera_samples'],axis=(0,1))+20
        return bool(np.all(np.asarray(point)>=lo) and np.all(np.asarray(point)<=hi))
    def invalid(self,reason):
        self.tracker.invalidate(reason);self.samples.clear();self.reason=reason
        # Keep frozen numbers visible for diagnosis, but disallow starting.
    def feed(self,line,stamp,now):
        if self.frozen:return # Second target is immutable until explicit reset/rearm.
        if self.accept_after is not None and stamp<=self.accept_after:return
        if not self.tracker.feed(line,stamp):return
        s=self.tracker.sample
        if s is None:
            self.samples.clear();self.reason=self.tracker.reason;return
        key=(self.tracker.epoch,s.frame)
        if key==self.last_identity:return
        self.last_identity=key
        if now-stamp>3:
            self.invalid('摄像头数据积压或过期');return
        if self.samples and stamp-self.samples[-1].received>3:self.samples.clear()
        if not self.armed:return
        if self.filter_candidates and not self.within_calibration((s.xyz[0],s.xyz[1],s.xyz[2]+self.depth_offset_mm)):
            self.samples.clear();self.reason='第二次候选点超出标定范围，已丢弃；继续等待范围内的稳定坐标'
            return
        self.samples.append(s)
        # Keep all frames over the latest >=2 second window (at least 5).
        # A five-frame-only buffer can never span 2 seconds above 2 fps.
        while len(self.samples)>5 and stamp-self.samples[1].received>=2:
            self.samples.popleft()
        if len(self.samples)<5:return
        a=np.array([v.xyz for v in self.samples]);span=np.ptp(a,axis=0)
        self.reason='采样波动 XYZ(mm)：'+str(tuple(float(v) for v in np.round(span,2)))
        if stamp-self.samples[0].received<2 or np.max(span)>20:return
        self.lock_target(tuple(np.mean(a,axis=0)))
    def lock_target(self,raw):
        self.locked=tuple(raw)
        # Offset in camera Z before transforming; keep raw lock for drift checks.
        self.grasp_camera=(self.locked[0],self.locked[1],self.locked[2]+self.depth_offset_mm)
        robot_xyz=np.asarray(self.cal['R'])@self.grasp_camera+np.asarray(self.cal['t'])
        if not np.isfinite(robot_xyz).all():raise ValueError('机械臂转换坐标必须为有限数值')
        self.target=tuple(float(v) for v in robot_xyz)
        self.frozen=self.freeze_on_lock
        self.armed=False;self.reason='坐标已准备（试验转换）；等待手动开始运动'
        if self.frozen:self.reason='第二次有效坐标已锁定；停止接收新坐标，等待自动抓取'
    def check(self,now):
        if self.locked is None or self.target is None:raise ValueError('尚未锁定稳定坐标')
        if self.used:raise ValueError('本次目标已使用，请重新采集')
        if not self.frozen:
            s=self.tracker.current(3,now)
            if math.dist(s.xyz,self.locked)>20:raise ValueError('实时目标偏离锁定值超过20mm，请重新采集')
        if not self.within_calibration(self.grasp_camera):
            raise ValueError('深度补偿后的抓取点超出标定采样范围向外扩展20mm后的范围，请补充标定；此限制不代表范围内精度合格')
        return self.target

def load_calibration(path):
    d=json.loads(Path(path).read_text(encoding='utf-8'))
    r=np.asarray(d['R']);t=np.asarray(d['t'])
    if r.shape!=(3,3) or t.shape!=(3,) or not np.isfinite(r).all() or not np.isfinite(t).all():raise ValueError('标定矩阵无效')
    if not np.allclose(r.T@r,np.eye(3),atol=1e-6) or abs(np.linalg.det(r)-1)>1e-6:raise ValueError('标定旋转无效')
    return d
