"""One positive J5 direction observation per controller connection. Angles are model-space; the controller adapter maps the wire command."""
from kinematics import fk, LOW, HIGH
from motion_settings import DEFAULT_SPEED


def start_probe(c, now):
    c.check_auto_ready(now)
    if getattr(c, 'j5_probe_used', False):
        raise ValueError('本次连接已执行J5验证；请先保存观察结果，不允许连续累加试动')
    samples = list(c.joint_history)
    if len(samples) < 3 or now-samples[0][0] > 3 or samples[-1][0]-samples[0][0] < .4:
        raise ValueError('请等待连续三次新鲜、静止的关节反馈')
    if any(max(s[1][i] for s in samples)-min(s[1][i] for s in samples) > .1 for i in range(6)):
        raise ValueError('关节仍在变化，不能进行J5方向验证')
    baseline = tuple(c.joints)
    target = tuple(round(v + (3.0 if i == 4 else 0.0), 3) for i, v in enumerate(baseline))
    if any(not lo <= v <= hi for v, lo, hi in zip(target, LOW, HIGH)):
        raise ValueError('J5验证目标超出参考关节限位，未发送')
    expected = fk(target)
    c.log('J5_PROBE_BEFORE model_joints='+str(baseline)+' model_pose='+str(c.pose)+' target='+str(target)+' speed='+str(DEFAULT_SPEED))
    # Reserve before sending: an uncertain transport failure must never allow a retry.
    c.j5_probe_used = True
    c.j5_probe_active = True
    c.move_auto(expected[:3], target, expected, baseline, DEFAULT_SPEED, now)
    c.status = f'J5验证：模型J5增加3°，速度{DEFAULT_SPEED}；观察夹爪向上还是向下倾，必要时STOP。'
