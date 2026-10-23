"""Hardware boundary: raw J5 has the opposite sign to the physical model.

Controller remains the raw protocol implementation used by protocol tests.
The application uses this adapter; planners only see model-space joints/poses.
"""
import math
from controller import Controller, numbers, angle_error
from kinematics import fk

def map_joints(values):
    q=list(numbers(values,6));q[4]=-q[4]
    return tuple(q)

class MappedController(Controller):
    coordinate_model='j5-reversed-v1'

    def __init__(self,send,log):
        super().__init__(send,log)
        self.raw_joints=self.raw_pose=None
        self.raw_joints_at=self.raw_pose_at=None

    def tx(self,command):
        # On this gripper HAND_C physically opens, HAND_O physically closes.
        # Keep controller state/labels logical; only map at the hardware boundary.
        if command in ('!HAND_O','!HAND_C'):
            logical=command
            command={'!HAND_O':'!HAND_C','!HAND_C':'!HAND_O'}[command]
            self.log('HAND_MAPPED_TX logical='+logical+' wire='+command)
        if command.startswith('@'):
            raise ValueError('J5修正模式不允许固件直接XYZ逆解，请使用软件关节规划')
        if command.startswith('>'):
            fields=command[1:].split(',')
            raw=map_joints(fields[:6])
            command='>'+','.join(f'{v:.3f}' for v in raw)+','+fields[6]
            self.log('J5_MAPPED_TX raw_joints='+str(raw))
        super().tx(command)

    def move(self,xyz,abc,speed,now):
        self.check_auto_ready(now)
        from staged_motion import solve_pose
        goal=numbers(xyz,3)+numbers(abc,3)
        q,p=solve_pose(goal,self.joints)
        self.move_auto(goal[:3],q,p,self.joints,speed,now)

    def check_auto_ready(self,now):
        super().check_auto_ready(now)
        if self.raw_joints_at is None or self.raw_pose_at is None or max(now-self.raw_joints_at,now-self.raw_pose_at)>1.5:
            raise ValueError('等待新鲜的控制器原始反馈')
        original=fk(self.raw_joints)
        if math.dist(original[:3],self.raw_pose[:3])>1 or angle_error(original[3:],self.raw_pose[3:])>2:
            raise ValueError('原始关节与控制器XYZ反馈不匹配或仍在运动，未发送')

    def receive(self,line,now):
        raw_ack=line.strip()
        if raw_ack in ('ok hand open','ok hand close'):
            logical_ack={'ok hand open':'ok hand close','ok hand close':'ok hand open'}[raw_ack]
            self.log('HAND_MAPPED_ACK wire='+raw_ack+' logical='+logical_ack)
            return super().receive(logical_ack,now)
        # Recovery validates the controller's raw protocol pairing, not physical FK.
        if self.recovery is not None or self.query is None:
            return super().receive(line,now)
        items=line.strip().split();kind,sent=self.query
        if len(items)!=7 or items[0]!='ok' or not 0<=now-sent<=2:
            return super().receive(line,now)
        try:raw=numbers(items[1:],6)
        except (ValueError,OverflowError):return super().receive(line,now)
        if kind=='joints':
            self.raw_joints=raw;self.raw_joints_at=now
            self.joints_query_sent=sent
            values=map_joints(raw)
        else:
            self.raw_pose=raw;self.raw_pose_at=now
            self.log('RAW_CONTROLLER_XYZABC '+str(raw))
            if self.joints_at is None or now-self.joints_at>1.5:
                self.raw_activity(now);self.rx_lines+=1
                self.query=None;self.query_kind='joints';self.next_query=now+.2
                self.pose_at=None;self.await_fresh=True
                return
            values=fk(self.joints)
            if self.target is not None and self.joints_query_sent<self.started:
                self.pose=values;self.pose_at=self.joints_at
                self.query=None;self.query_kind='joints';self.next_query=now+.2
                self.arrivals=0
                return
        super().receive('ok '+' '.join(str(float(v)) for v in values),now)
        if kind=='pose':self.pose_at=self.joints_at
