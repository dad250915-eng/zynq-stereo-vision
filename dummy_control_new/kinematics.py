"""Position-only numerical IK; geometry/FK from supplied Dummy linear-gripper source.
Units: mm and degrees. This is endpoint kinematics, NOT collision/path planning.
"""
import math
import numpy as np

LOW=np.array([-170.,-75.,35.,-180.,-120.,-180.])
HIGH=np.array([170.,90.,180.,180.,120.,180.])

def fk(joints):
    q=np.radians(joints)+[0,-math.pi/2,math.pi/2,0,0,0]
    alpha=[-math.pi/2,0,math.pi/2,-math.pi/2,math.pi/2,0]
    r=np.eye(3); pos=np.zeros(3)
    for i,(t,a) in enumerate(zip(q,alpha)):
        c,s,ca,sa=math.cos(t),math.sin(t),math.cos(a),math.sin(a)
        r=r@np.array([[c,-ca*s,sa*s],[s,ca*c,-sa*c],[0,sa,ca]])
        if i==0:pos+=r@np.array([35.,-134.,0])
        if i==1:pos+=r@np.array([146.,0,0])
        if i==2:pos+=r@np.array([-52.,0,117.])
        if i==5:pos+=r@np.array([0.,0,62.5])
    b=math.atan2(-r[2,0],math.hypot(r[0,0],r[1,0]))
    if abs(math.cos(b))<1e-7:
        a=0.; c=math.atan2(-r[0,1],r[1,1])
    else:a=math.atan2(r[2,1],r[2,2]); c=math.atan2(r[1,0],r[0,0])
    return tuple(pos)+tuple(np.degrees([a,b,c]))

def solve(xyz,current):
    goal=np.asarray(xyz,dtype=float); current=np.asarray(current,dtype=float)
    if goal.shape!=(3,) or current.shape!=(6,) or not np.isfinite(goal).all() or not np.isfinite(current).all():
        raise ValueError('求解输入必须为有限 XYZ 和六个关节角')
    if np.linalg.norm(goal)>700:raise ValueError('目标超出参考机构最大长度，未发送')
    rng=np.random.default_rng(5640)
    seeds=[np.clip(current,LOW,HIGH)]
    seeds += [np.clip(current+rng.normal(0,scale,6),LOW,HIGH) for scale in (10,20,35,50,70,90)]
    solutions=[]; best=float('inf')
    for seed in seeds:
        q=seed.copy(); q[5]=np.clip(current[5],LOW[5],HIGH[5])
        for _ in range(100):
            pos=np.array(fk(q)[:3]); e=goal-pos; err=np.linalg.norm(e); best=min(best,err)
            if err<.03:break
            jac=np.column_stack([(np.array(fk(q+np.eye(6)[j]*.02)[:3])-pos)/.02 for j in range(5)])
            step=jac.T@np.linalg.solve(jac@jac.T+np.eye(3)*.08,e)
            step*=min(1.,5./max(np.max(np.abs(step)),1e-9))
            improved=False
            for scale in (1.,.5,.25,.1):
                trial=q.copy();trial[:5]=np.clip(q[:5]+scale*step,LOW[:5],HIGH[:5])
                if np.linalg.norm(goal-np.array(fk(trial)[:3]))<err-1e-8:
                    q=trial;improved=True;break
            if not improved:break
        # Validate exactly the rounded values that will be sent to the controller.
        q=np.round(q,3); residual=np.linalg.norm(goal-np.array(fk(q)[:3]))
        if residual<=.1 and np.all(q>=LOW) and np.all(q<=HIGH):
            solutions.append((np.max(np.abs(q-current))+.05*np.linalg.norm(q-current),q))
    if not solutions:raise ValueError(f'自动姿态未找到限位内的解（最小残差 {best:.2f} mm）；未发送。这不证明所有姿态均不可达。')
    q=min(solutions,key=lambda item:item[0])[1]
    return tuple(float(v) for v in q),fk(q)
