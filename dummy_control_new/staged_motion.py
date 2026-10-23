"""Offline endpoint planning and feedback-gated execution. Not collision planning."""
import math
import numpy as np
from kinematics import fk,LOW,HIGH
from controller import rotation,angle_error,numbers

def solve_pose(pose,current):
    goal=np.array(numbers(pose,6));base=np.array(numbers(current,6))
    rg=np.array(rotation(goal[3:]))
    def residual(q):
        p=fk(q);return np.r_[np.array(p[:3])-goal[:3],50*(np.array(rotation(p[3:]))-rg).ravel()]
    seeds=[np.clip(base,LOW,HIGH)]
    rng=np.random.default_rng(5641)
    seeds += [np.clip(base+rng.normal(0,k,6),LOW,HIGH) for k in (15,35,70)]
    for seed in seeds:
        q=seed.copy()
        for _ in range(150):
            e=residual(q)
            if np.linalg.norm(e)<.03:break
            jac=np.column_stack([(residual(q+np.eye(6)[j]*.02)-e)/.02 for j in range(6)])
            step=-np.linalg.solve(jac.T@jac+np.eye(6)*.03,jac.T@e)
            step*=min(1.,4/max(np.max(abs(step)),1e-9))
            for scale in (1,.5,.25,.1):
                trial=np.clip(q+scale*step,LOW,HIGH)
                if np.linalg.norm(residual(trial))<np.linalg.norm(e)-1e-9:q=trial;break
            else:break
        q=np.round(q,3);p=fk(q)
        if math.dist(p[:3],goal[:3])<=.1 and angle_error(p[3:],goal[3:])<=.2:return tuple(q),tuple(p)
    raise ValueError('固定姿态未找到限位内的解；不改成任意姿态，不发送任何步骤')

def build_plan(target,current,abc,clearance,side,offset,stop_above=True):
    target=np.array(numbers(target,3));abc=numbers(abc,3);offset=np.array(numbers(offset,3))
    clearance,side=numbers((clearance,side),2)
    if not 0<clearance<=300 or abs(side)>200:raise ValueError('间隙须为0～300mm，侧向过渡偏移绝对值≤200mm')
    start=fk(current);r0=np.array(rotation(start[3:]));r1=np.array(rotation(abc))
    tool_now=np.array(start[:3])+r0@offset
    near=target+np.array([side,0,clearance]);near[2]=max(near[2],tool_now[2])
    final=target+np.array([0,0,clearance if stop_above else 0])
    poses=[tuple(near-r0@offset)+tuple(start[3:]),tuple(near-r1@offset)+tuple(abc),tuple(final-r1@offset)+tuple(abc)]
    steps=[];q=current
    for label,p in zip(('到过渡点','调整为示教抓取姿态','保持姿态对准'),poses):
        q,expected=solve_pose(p,q);steps.append(dict(name=label,pose=tuple(p),joints=q,expected=expected))
    return steps

class Sequence:
    def __init__(self,controller,steps,speed):
        self.c=controller;self.steps=steps;self.speed=speed;self.index=0;self.waiting=False;self.count=controller.completed;self.done=False
    def tick(self,now):
        c=self.c
        if c.fault or not c.enabled or c.stop_sent:raise ValueError('分步定位已中止：通信故障、失能或STOP；不自动续跑')
        if c.recovery is not None or c.await_fresh:return
        if self.waiting:
            if c.target is not None:return
            if c.completed<=self.count:raise ValueError('上一阶段没有到位确认，已中止后续步骤')
            self.index+=1;self.waiting=False
        if self.index>=len(self.steps):self.done=True;return
        if c.pose_at is None or c.joints_at is None or now-c.pose_at>1.5 or now-c.joints_at>1.5:return
        c.check_auto_ready(now);s=self.steps[self.index]
        # Recheck full endpoint pose before every send.
        p=fk(s['joints'])
        if math.dist(p[:3],s['pose'][:3])>.1 or angle_error(p[3:],s['pose'][3:])>.2:raise ValueError('阶段位姿校验失败')
        self.count=c.completed
        c.move_auto(s['pose'][:3],s['joints'],s['expected'],c.joints,self.speed,now)
        c.log('STAGED_STEP '+str(self.index+1)+' '+s['name']+' '+str(s['pose']))
        if c.target is None:self.index+=1 # Same joint pose, no command required.
        else:self.waiting=True
