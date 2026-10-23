"""Reference pose then direct joint move to a downward-facing endpoint.
Joint firmware interpolation is not guaranteed Cartesian or collision-free.
"""
import math
from controller import numbers,angle_error
from kinematics import fk
from vertical_pose import solve_down,DOWN_ABC,validate_down
from staged_motion import solve_pose,Sequence

SEGMENT_MM=2.0

def step(name,xyz,q,p,down):
    return dict(name=name,pose=tuple(xyz)+tuple(DOWN_ABC if down else p[3:]),joints=q,expected=p,down=down)

def check_segment(q0,q1,start,end):
    if max(abs(a-b) for a,b in zip(q0,q1))>5:
        raise ValueError('保持朝下路径需要关节分支跳变，未发送')
    for k in range(11):
        t=k/10;p=fk(tuple(a+(b-a)*t for a,b in zip(q0,q1)))
        desired=tuple(a+(b-a)*t for a,b in zip(start,end))
        if math.dist(p[:3],desired)>.5 or angle_error(p[3:],DOWN_ABC)>.5:
            raise ValueError('保持朝下路径的模型插值偏差超限，未发送')

def line_steps(start,end,q):
    count=max(1,math.ceil(math.dist(start,end)/SEGMENT_MM));steps=[];previous=tuple(start)
    for i in range(1,count+1):
        xyz=tuple(a+(b-a)*i/count for a,b in zip(start,end))
        nextq,p=solve_down(xyz,q);check_segment(q,nextq,previous,xyz)
        steps.append(step('分段 '+str(i)+'/'+str(count),xyz,nextq,p,True))
        q=nextq;previous=xyz
    return steps

def plan_grasp(xyz,current):
    from reference_preset import PRESET_XYZ,preset_solution
    xyz=numbers(xyz,3);current=numbers(current,6)
    reference_q,reference_p=preset_solution()
    # Solve both endpoints before allowing any movement. No path constraint between them.
    target_q,target_p=solve_down(xyz,reference_q)
    steps=[step('先到参考下垂姿态',PRESET_XYZ,reference_q,reference_p,True),
           step('直接到输入XYZ，终点朝下',xyz,target_q,target_p,True)]
    return dict(steps=steps,baseline=current,target=xyz,clearance=0.,kind='grasp')


def plan_vertical(xyz,current):
    xyz=numbers(xyz,3);current=numbers(current,6);start=fk(current)
    if angle_error(start[3:],DOWN_ABC)>.2:
        raise ValueError('升降前须先完成一次参考朝下定位，避免带物时原地翻转')
    if math.dist(start[:2],xyz[:2])>.1:raise ValueError('升降目标只能改变Z')
    return dict(steps=line_steps(start[:3],xyz,current),baseline=current,target=xyz,clearance=0.,kind='vertical')

class MotionSequence(Sequence):
    def __init__(self,c,plan,speed):
        super().__init__(c,plan['steps'],speed);self.plan=plan
    def tick(self,now):
        if not self.waiting and self.index<len(self.steps):
            s=self.steps[self.index]
            if s.get('down'):validate_down(s['pose'][:3],s['joints'])
        super().tick(now)
