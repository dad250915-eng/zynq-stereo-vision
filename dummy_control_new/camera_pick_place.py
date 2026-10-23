"""Offline camera pick/place planning and ACK/arrival-gated execution."""
import math
from controller import angle_error, numbers
from reference_motion import plan_grasp, MotionSequence, step
from reference_preset import PRESET_XYZ, preset_solution
from vertical_pose import solve_down
from motion_settings import DEFAULT_SPEED

PLACEMENTS = ((0., -150., 165.), (0., -220., 165.))
RETRACTS = ((0., -150., 210.), (0., -220., 210.))



def plan_pick_place(xyz, current, slot):
    if slot not in (0, 1):raise ValueError('本轮仅允许两次抓放')
    pickup = plan_grasp(xyz, current)
    reference_q, reference_p = preset_solution()
    # Preflight both fixed destinations before allowing the first pickup.
    placements = []
    for drop, retract in zip(PLACEMENTS, RETRACTS):
        q, p = solve_down(drop, reference_q)
        lift_q, lift_p = solve_down(retract, q)
        lift = step('保持开爪直接到上升位置', retract, lift_q, lift_p, True)
        placements.append((q, p, lift))
    drop_q, drop_p, lift = placements[slot]
    actions = []
    if slot == 0:actions.append(dict(name='开爪准备第一次抓取', op='hand', opening=True))
    actions += [dict(s, op='move') for s in pickup['steps']]
    actions.append(dict(name='抓取到位后闭爪', op='hand', opening=False,
                        goal=pickup['steps'][-1]))
    actions.append(dict(step('保持闭爪回到参考下垂姿态', PRESET_XYZ,
                             reference_q, reference_p, True), op='move'))
    drop_step = step('保持闭爪到放置点', PLACEMENTS[slot], drop_q, drop_p, True)
    actions.append(dict(drop_step, op='move'))
    actions.append(dict(name='放置到位后开爪', op='hand', opening=True, goal=drop_step))
    actions.append(dict(lift, op='move', speed=DEFAULT_SPEED))
    return dict(kind='camera_pick_place', slot=slot, target=numbers(xyz, 3),
                baseline=numbers(current, 6), clearance=0., steps=actions,
                placement=PLACEMENTS[slot], retract=RETRACTS[slot])


class CameraPickPlaceSequence:
    SETTLE_SECONDS = 1.0

    def __init__(self, controller, plan, speed):
        self.c=controller;self.plan=plan;self.steps=plan['steps'];self.speed=speed
        self.index=0;self.done=False;self.motion=None
        self.waiting=False;self.ack_count=0;self.acked_at=None

    def tick(self, now):
        c=self.c
        if c.fault or not c.enabled or c.stop_sent:
            c.hand_queued=None
            raise ValueError('抓放中止：故障、失能或STOP，不继续后续步骤')
        if c.recovery is not None or c.await_fresh:return
        if self.done:return
        action=self.steps[self.index]
        if action['op']=='move':
            if self.motion is None:
                self.motion=MotionSequence(c,dict(steps=[action]),action.get('speed',self.speed))
            self.motion.tick(now)
            if not self.motion.done:return
            self.motion=None;self.index+=1
        else:
            if not self.waiting:
                if c.pose_at is None or c.joints_at is None or max(now-c.pose_at,now-c.joints_at)>1.5:return
                c.check_auto_ready(now)
                goal=action.get('goal')
                if goal and (math.dist(c.pose[:3],goal['pose'][:3])>.5
                             or angle_error(c.pose[3:],goal['pose'][3:])>1
                             or max(abs(a-b) for a,b in zip(c.joints,goal['joints']))>.3):
                    raise ValueError('夹爪操作前目标反馈不符，取消后续抓放')
                self.ack_count=c.hand_completed
                c.hand(action['opening'],now);self.waiting=True
                c.log('CAMERA_HAND_QUEUED slot='+str(self.plan['slot']+1)+' '+action['name'])
                return
            if c.hand_completed<=self.ack_count:
                if c.hand_queued is None and c.pending is None:
                    raise ValueError('夹爪命令被通信恢复或取消清除，不能确认执行；不自动重发')
                return
            wanted='hand_open' if action['opening'] else 'hand_close'
            if c.last_hand_ack!=wanted:raise ValueError('夹爪回执与抓放阶段不符')
            if self.acked_at is None:self.acked_at=now
            if now-self.acked_at<self.SETTLE_SECONDS:return
            self.waiting=False;self.acked_at=None;self.index+=1
        if self.index==len(self.steps):self.done=True


class CameraBatch:
    """Two picks per explicit start; second acquisition begins after first lift."""
    def __init__(self):
        self.state='idle';self.slot=0;self.controller=None;self.token=None
        self.generation=None;self.speed=None;self.reason='等待第一次锁定并手动开始'

    @property
    def active(self):return self.state in ('planning','running','waiting_second')

    def begin(self, controller, token, generation, speed):
        if self.active:raise ValueError('两次抓放流程尚未结束')
        self.controller=controller;self.token=token;self.generation=generation
        self.speed=speed;self.slot=0;self.state='planning';self.reason='正在规划第一次抓放'

    def plan_second(self):
        if self.state!='waiting_second':raise ValueError('尚未完成第一次放置和上升')
        self.state='planning';self.reason='正在规划第二次抓放'

    def started(self, slot):
        if self.state!='planning' or slot!=self.slot:raise ValueError('抓放阶段已改变')
        self.state='running';self.reason='正在执行第'+str(slot+1)+'次抓放'

    def completed(self, slot, capture, now):
        if self.state!='running' or slot!=self.slot:raise ValueError('抓放完成回报与当前阶段不符')
        if slot==0:
            capture.rearm_after(now)
            self.slot=1;self.state='waiting_second';self.reason='第一次已放置并上升，等待第二次新坐标锁定'
        else:
            self.state='done';self.reason='两次抓取与放置完成，停在第二个上升位置并保持开爪'

    def abort(self, reason):
        self.state='aborted';self.reason='两次抓放已中止：'+reason
