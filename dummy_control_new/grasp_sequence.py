"""Open, reference pose, final pose, close; never auto-lift or auto-retry."""
import math
from controller import angle_error
from reference_motion import MotionSequence

class GraspSequence:
    SETTLE_SECONDS=1.0  # Dwell after ACK; not a physical open/closed sensor.

    def __init__(self,c,plan,speed):
        self.c=c;self.plan=plan;self.motion=MotionSequence(c,plan,speed)
        self.steps=[{'name':'开爪，等待回执'}]+plan['steps']+[{'name':'闭爪，等待回执'}]
        self.index=0;self.done=False;self.phase='open';self.waiting=False
        self.ack_count=0;self.acked_at=None

    def tick(self,now):
        c=self.c
        if c.fault or not c.enabled or c.stop_sent:
            c.hand_queued=None
            raise ValueError('抓取中止：故障、失能或STOP，不继续闭爪')
        if c.recovery is not None or c.await_fresh:return
        if self.done:return
        if self.phase=='move':
            self.motion.tick(now);self.index=1+self.motion.index
            if not self.motion.done:return
            self.phase='close';self.waiting=False;self.acked_at=None
        if self.phase in ('open','close'):
            opening=self.phase=='open'
            if not self.waiting:
                if c.pose_at is None or c.joints_at is None or now-c.pose_at>1.5 or now-c.joints_at>1.5:return
                c.check_auto_ready(now)
                if not opening:
                    goal=self.plan['steps'][-1]
                    if math.dist(c.pose[:3],goal['pose'][:3])>.5 or angle_error(c.pose[3:],goal['pose'][3:])>1 or max(abs(a-b) for a,b in zip(c.joints,goal['joints']))>.3:
                        raise ValueError('闭爪前目标反馈不符，取消闭爪')
                self.ack_count=c.hand_completed
                c.hand(opening,now);self.waiting=True
                c.log('AUTO_GRASP_'+('OPEN' if opening else 'CLOSE')+'_QUEUED')
                return
            if c.hand_completed<=self.ack_count:return
            if c.last_hand_ack!=('hand_open' if opening else 'hand_close'):
                raise ValueError('夹爪回执与阶段不符')
            if self.acked_at is None:self.acked_at=now
            if now-self.acked_at<self.SETTLE_SECONDS:return
            if opening:self.phase='move';self.index=1
            else:self.done=True;self.index=len(self.steps)
