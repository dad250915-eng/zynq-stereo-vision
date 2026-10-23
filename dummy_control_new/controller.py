"""Dummy ASCII protocol controller. No camera, coordinate transform or workspace bounds."""
import math
import re
from collections import deque


def numbers(values,count):
    result=tuple(float(v) for v in values)
    if len(result)!=count or not all(math.isfinite(v) and abs(v)<=3.4e38 for v in result):
        raise ValueError(f'请输入 {count} 个有限数字')
    return result


def rotation(abc):
    a,b,c=map(math.radians,abc)
    ca,sa,cb,sb,cc,sc=math.cos(a),math.sin(a),math.cos(b),math.sin(b),math.cos(c),math.sin(c)
    return ((cc*cb,cc*sb*sa-sc*ca,cc*sb*ca+sc*sa),
            (sc*cb,sc*sb*sa+cc*ca,sc*sb*ca-cc*sa),(-sb,cb*sa,cb*ca))


def angle_error(left,right):
    a,b=rotation(left),rotation(right)
    trace=sum(a[i][j]*b[i][j] for i in range(3) for j in range(3))
    return math.degrees(math.acos(max(-1.,min(1.,(trace-1)/2))))


class Controller:
    def __init__(self,send,log):
        self.send=send;self.log=log;self.enabled=False;self.fault=None
        self.hand_completed=0;self.last_hand_ack=None
        self.hand_queued=None;self.hand_status="夹爪尚未操作；无夹持到位反馈"
        self.pending=None;self.query=None;self.query_kind='pose';self.next_query=0.
        self.pose=None;self.joints=None;self.pose_at=None;self.joints_at=None
        self.target=None;self.source_pose=None;self.started=None;self.arrivals=0
        self.completed=0;self.queue_space=None;self.last_rtt=None;self.ack='尚未收到'
        self.status='已连接；正在读取诊断信息。';self.observation=''
        self.position_error=None;self.orientation_error=None;self.rx_lines=0;self.tx_lines=0
        self.start_joints=None;self.no_change_reported=False;self.long_wait_reported=False
        self.return_joints=None;self.joint_history=deque(maxlen=3)
        self.stop_sent=False;self.had_motion=False;self.joint_target=None
        self.recovery=None;self.last_rx=0.;self.recovery_count=0;self.await_fresh=False

    def tx(self,command):
        try:self.send(command);self.tx_lines+=1
        except Exception as exc:
            self.fail('串口发送失败：'+str(exc));raise

    def fail(self,reason):
        from usb_diagnostics import record
        record('CONTROLLER_FAULT',reason=reason,query=self.query,pending_control=self.pending,target=self.target,pose=self.pose,joints=self.joints,last_rtt_ms=self.last_rtt,enabled_confirmation=self.enabled,recovery=self.recovery)
        self.log('FAULT '+reason)
        self.hand_queued=None
        self.fault=reason;self.status=reason;self.enabled=None;self.query=None;self.pending=None;self.recovery=None;self.await_fresh=False
        if self.target is not None and not self.stop_sent:
            self.stop_sent=True
            try:self.send('!STOP');self.log('故障时请求STOP；不能据此确认实体已停止')
            except Exception as exc:self.log('STOP发送失败：'+str(exc))

    def hand(self,opening,now):
        self.check_auto_ready(now)
        self.hand_queued='hand_open' if opening else 'hand_close'
        self.hand_status='等待当前查询结束后发送'+('开爪' if opening else '闭爪')
        self.status=self.hand_status

    def move_vertical(self,distance,speed,now):
        self.check_auto_ready(now)
        delta=numbers([distance],1)[0]
        if delta==0:raise ValueError('升降距离不能为0')
        from vertical_pose import solve_down
        xyz=(self.pose[0],self.pose[1],self.pose[2]+delta)
        baseline=self.joints;result=solve_down(xyz,baseline)
        self.move_down(xyz,*result,baseline,speed,now)

    def move_down(self,xyz,joints,expected,baseline,speed,now):
        # Recompute from the rounded command; never trust the caller's predicted pose.
        from vertical_pose import validate_down
        validate_down(xyz,joints)
        self.move_auto(xyz,joints,expected,baseline,speed,now)
        if self.target is not None:
            self.status='垂直向下目标已发送；等待XYZ、完整姿态及关节反馈同时到位。'


    def enable(self,now):
        if self.hand_queued is not None:raise ValueError("夹爪命令等待发送，请稍候")
        if self.fault:raise ValueError('通信故障：'+self.fault+'；请现场确认状态后重新连接')
        if self.recovery is not None or self.await_fresh:raise ValueError('正在恢复查询对应关系，暂不允许新运动或使能指令')
        if self.fault:raise ValueError('通信状态异常，请现场确认停止后断开重连')
        if self.target is not None:raise ValueError('正在执行目标，无需重复使能')
        if self.pending:raise ValueError('正在等待上一条控制命令回复')
        if self.enabled:return
        self.pending=('start',now);self.tx('!START');self.status='等待 Started ok…'

    def move(self,xyz,abc,speed,now):
        if self.hand_queued is not None:raise ValueError("夹爪命令等待发送，请稍候")
        if self.fault:raise ValueError('通信故障：'+self.fault+'；请现场确认状态后重新连接')
        if self.recovery is not None or self.await_fresh:raise ValueError('正在恢复查询对应关系，暂不允许新运动或使能指令')
        if self.fault:raise ValueError('通信状态异常，请重新连接')
        if not self.enabled:raise ValueError('请先使能电机并等待 Started ok')
        if self.target is not None:raise ValueError('上一目标未完成；需要中止时请点击STOP')
        if self.pending:raise ValueError("正在等待上一条控制命令回复")
        xyz=numbers(xyz,3);abc=numbers(abc,3);speed=numbers([speed],1)[0]
        if speed!=int(speed) or not 1<=speed<=100:raise ValueError('速度指令须为1～100的整数')
        pose=tuple(float(f'{v:.1f}') for v in xyz+abc)
        command='@'+','.join(f'{v:.1f}' for v in pose)+f',{int(speed)}'
        if len(command)>100:raise ValueError('数值过长，超出本程序允许的单条协议长度')
        if self.pose is not None and all(abs(a-b)<.051 for a,b in zip(pose[:3],self.pose[:3])) and angle_error(pose[3:],self.pose[3:])<.05:
            self.status='目标与当前位置相同，无需发送零位移命令。';return
        self.joint_target=None
        self.target=pose;self.source_pose=self.pose;self.start_joints=self.joints;self.started=now
        self.arrivals=0;self.position_error=None;self.orientation_error=None;self.ack='等待接收回复'
        self.no_change_reported=False;self.long_wait_reported=False;self.stop_sent=False;self.had_motion=True
        self.tx(command);self.status='目标已发送，正在观察末端与关节反馈。'

    def check_auto_ready(self,now):
        if self.hand_queued is not None:raise ValueError("夹爪命令等待发送，请稍候")
        if self.fault:raise ValueError('通信故障：'+self.fault+'；请现场确认状态后重新连接')
        if self.recovery is not None or self.await_fresh:raise ValueError('正在恢复查询对应关系，暂不允许新运动或使能指令')
        if self.fault or not self.enabled:raise ValueError('请先确认通信正常并使能电机')
        if self.target is not None or self.pending:raise ValueError('上一命令尚未完成，需要中止时请点击STOP')
        if self.pose_at is None or self.joints_at is None or now-self.pose_at>1.5 or now-self.joints_at>1.5:
            raise ValueError('需要新鲜的末端和关节反馈；请等待查询完成')
        from kinematics import fk
        expected=fk(self.joints)
        if math.dist(expected[:3],self.pose[:3])>1.0 or angle_error(expected[3:],self.pose[3:])>2:
            raise ValueError('参考运动学与控制器反馈不匹配，或机械臂仍在移动；未发送。需核对机构参数/零位')

    def move_auto(self,xyz,joints,expected,baseline,speed,now):
        self.check_auto_ready(now)
        from kinematics import fk,LOW,HIGH
        xyz=numbers(xyz,3); joints=numbers(joints,6); speed=numbers([speed],1)[0]
        if speed!=int(speed) or not 1<=speed<=100:raise ValueError('速度指令须为1～100的整数')
        if max(abs(a-b) for a,b in zip(self.joints,baseline))>.5:
            raise ValueError('求解期间关节发生变化，请停止后重新发送目标')
        joints=tuple(round(v,3) for v in joints); expected=fk(joints)
        if any(v<lo or v>hi for v,lo,hi in zip(joints,LOW,HIGH)) or math.dist(xyz,expected[:3])>.1:
            raise ValueError('关节解校验失败，未发送')
        if max(abs(a-b) for a,b in zip(joints,self.joints))<.01:
            self.status='目标已在当前位置，无需移动。';return
        self.target=xyz+tuple(expected[3:]);self.joint_target=joints
        self.source_pose=self.pose;self.start_joints=self.joints;self.started=now
        self.arrivals=0;self.position_error=None;self.orientation_error=None;self.ack='等待接收回复'
        self.no_change_reported=False;self.long_wait_reported=False;self.stop_sent=False;self.had_motion=True
        self.log('关节执行 XYZ='+str(xyz)+' → J1..J6='+str(joints)+'；预测 ABC='+str(expected[3:]))
        self.tx('>'+','.join(f'{v:.3f}' for v in joints)+f',{int(speed)}')
        self.status='关节目标已发送，正在核对位置、姿态和关节反馈。'

    def record_return_pose(self,now):
        self.check_auto_ready(now)
        from kinematics import LOW,HIGH
        samples=list(self.joint_history)
        if len(samples)<3 or now-samples[0][0]>3 or samples[-1][0]-samples[0][0]<.4:
            raise ValueError('请等待连续三次新鲜关节反馈后再记录')
        if any(max(row[1][i] for row in samples)-min(row[1][i] for row in samples)>.1 for i in range(6)):
            raise ValueError('关节仍在变化，请静止后再记录返回姿态')
        joints=tuple(round(v,3) for v in self.joints)
        if any(v<lo or v>hi for v,lo,hi in zip(joints,LOW,HIGH)):
            raise ValueError('当前关节超出参考限位，不能记录为返回目标；请先核对零位/限位，不会自动修正角度')
        self.return_joints=joints
        self.status='返回姿态已记录（本次连接有效）；记录操作不发送运动。'
        self.log('RETURN_POSE_RECORDED J1..J6='+str(joints)+' XYZABC='+str(self.pose))

    def return_to_recorded(self,speed,now):
        if self.return_joints is None:raise ValueError('请先记录返回姿态；重新连接后需重新记录')
        from kinematics import fk
        expected=fk(self.return_joints)
        self.move_auto(expected[:3],self.return_joints,expected,self.joints,speed,now)
        if self.target is not None:
            self.status='正在返回记录的关节姿态；等待连续三次到位反馈。'
            self.log('RETURN_POSE_SENT J1..J6='+str(self.return_joints))

    def stop(self,now):
        if getattr(self,'j5_probe_active',False):
            self.log('J5_PROBE_STOP_REQUEST latest_joints='+str(self.joints)+'；停止后的反馈见后续RX记录')
            self.j5_probe_active=False
        # STOP remains available even when a query or movement is outstanding.
        self.hand_queued=None
        self.stop_sent=True;self.enabled=False;self.pending=('stop',now)
        self.tx('!STOP');self.status='已请求停止，等待 Stopped ok。'

    def raw_activity(self,now):
        self.last_rx=max(self.last_rx,now)

    def bad_frame(self,reason,now):
        self.hand_queued=None
        self.joint_history.clear()
        self.log('BAD_FRAME '+reason)
        self.arrivals=0;self.position_error=None;self.orientation_error=None
        self.pose_at=None;self.joints_at=None;self.query=None;self.await_fresh=True
        if self.recovery is None:
            self.recovery_count+=1
            self.recovery={'deadline':now+30.,'attempt':0,'stage':0,'samples':[], 'phase':'drain'}
        r=self.recovery;r['attempt']+=1;r['stage']=0;r['samples']=[];r['phase']='drain'
        self.last_rx=now
        self.status='坏帧已丢弃；暂停新运动，等待接收静默后重新核对关节/末端反馈。已有运动未自动中止。'
        if r['attempt']>3:self.fail('查询恢复连续失败；实际运动/使能状态未知')

    def recovery_reply(self,line,now):
        r=self.recovery
        if r['phase']=='drain':
            self.log('RECOVERY 丢弃静默等待期回复：'+repr(line));return
        # Normal motion acknowledgements carry no position information.
        if line=='ok' or re.fullmatch(r'\d+\s*(ok)?',line):return
        items=line.split()
        try:
            if len(items)!=7 or items[0]!='ok':raise ValueError()
            v=numbers(items[1:],6)
            if not self.query or now-self.query[1]>2:raise ValueError()
        except (ValueError,OverflowError):self.bad_frame('恢复探测回复无效/迟到：'+repr(line),now);return
        r['samples'].append(v);r['stage']+=1;self.query=None
        r['deadline']=now+30.  # Watchdog measures lack of valid probe progress, not motion duration.
        self.log('RECOVERY 有效探测 '+str(r['stage'])+'/4 '+str(v))
        if r['stage']<4:r['phase']='drain';return
        j1,j2,p1,p2=r['samples']
        from kinematics import fk
        model=fk(j2)
        moving=(max(abs(a-b) for a,b in zip(j1,j2))>.3 or math.dist(p1[:3],p2[:3])>.5
                or angle_error(p1[3:],p2[3:])>1)
        if moving:
            r['stage']=0;r['samples']=[];r['phase']='drain'
            self.status='通信探测有效，位置仍在变化；继续等待稳定，禁止新动作，不重发原目标。'
            self.log('RECOVERY_WAIT_MOTION '+self.status)
            return
        if math.dist(model[:3],p2[:3])>1 or angle_error(model[3:],p2[3:])>2:
            self.bad_frame('稳定探测的关节/末端运动学对应不符',now);return
        # Do not credit recovery samples toward movement completion or publish old samples.
        self.recovery=None;self.query_kind='joints';self.next_query=now+2.5
        self.status='查询对应关系已恢复；等待新鲜反馈。未重发运动，也未改变使能。'
        self.log('RECOVERY_OK '+self.status)

    def receive(self,line,now):
        self.rx_lines+=1;self.raw_activity(now)
        original=line;line=line.strip()
        if any(t in line for t in ('ets Jun','SPI_FAST_FLASH_BOOT','flash read err','RTCWDT_RTC_RESET')):
            self.fail('收到ESP32启动日志，疑似接错控制通道或ESP32重启；检查机械臂端USB方向。');return
        # Even a faulted session must observe its STOP acknowledgement. Do not clear the fault
        # or silently enable motion: an acknowledgement is not physical standstill evidence.
        if line=='Stopped ok' and self.stop_sent:
            self.pending=None;self.enabled=False;self.ack=line;self.target=None;self.joint_target=None
            if self.fault:
                self.status='已收到 Stopped ok；故障仍保留：'+self.fault+'。请现场确认停止后重新连接。'
            else:self.status='控制器回复 Stopped ok；请现场确认停止。'
            self.log('STOP_ACK '+self.status)
            return
        if self.fault:return
        if any(ord(ch)>127 or (ord(ch)<32 and ch not in '\r\n') for ch in original):
            self.bad_frame('非ASCII或控制字节：'+repr(original),now);return
        if self.pending:
            kind,sent=self.pending
            wanted={'start':'Started ok','stop':'Stopped ok','hand_open':'ok hand open','hand_close':'ok hand close'}.get(kind)
            if line==wanted:
                if now-sent>2:self.fail('控制命令回复迟到');return
                if kind in ('hand_open','hand_close'):
                    self.hand_completed+=1;self.last_hand_ack=kind
                    self.pending=None;self.ack=line
                    self.hand_status=('开爪' if kind=='hand_open' else '闭爪')+'命令已确认；请现场确认完成，无夹稳反馈'
                    self.status=self.hand_status;self.log('HAND_ACK '+self.hand_status);return
                self.pending=None;self.enabled=kind=='start';self.ack=line
                if kind=='stop':self.target=None;self.status='控制器回复 Stopped ok；请现场确认停止。'
                else:self.stop_sent=False;self.status='使能已确认，可以输入XYZ并发送。'
                return
        if line in ('ok hand open','ok hand close'):
            self.log('未关联的夹爪回执，未作完成确认：'+line);return
        if line.lower().startswith('error'):
            self.fail('控制器报告错误：'+line);return
        if self.recovery is not None:
            self.recovery_reply(line,now);return
        queue_reply=re.fullmatch(r'(\d+)\s*(ok)?',line)
        if queue_reply:
            self.queue_space=int(queue_reply.group(1))
            self.ack=f'队列回复 {self.queue_space}'+(' + ok' if queue_reply.group(2) else '')+'（不是到位确认）'
            if self.queue_space==255:self.fail('控制器队列拒绝命令（255）')
            return
        if line=='ok':self.ack='收到 ok（不是到位确认）';return
        items=line.split()
        if len(items)!=7 or items[0]!='ok':
            if self.query and line:self.bad_frame('查询回复格式不符：'+repr(line),now)
            return
        try:values=numbers(items[1:],6)
        except (ValueError,OverflowError):self.bad_frame('六项位置反馈含无效数值：'+repr(line),now);return
        if self.query is None:
            self.log('未关联的六项回复，未用作末端/关节反馈：'+line);return
        kind,sent=self.query
        if now<sent or now-sent>2:self.fail('查询回复过期；停止交替查询以免混淆关节角与末端位置');return
        self.query=None;self.last_rtt=(now-sent)*1000;self.next_query=now+.2
        self.query_kind='joints' if kind=='pose' else 'pose'
        if kind=='joints':
            self.joints=values;self.joints_at=now
            self.joint_history.append((now,values))
            if self.pose_at is not None:self.await_fresh=False
            self.log('解析 J1..J6（度）：'+str(values))
            if values[1]<=-74.5 or values[2]>=179.5:
                self.observation='J2/J3接近资料参考限位（-75°/180°）；实物固件限位待确认，可能影响可达性。'
            else:self.observation=''
            return
        self.pose=values;self.pose_at=now
        if self.joints_at is not None:self.await_fresh=False
        self.log('解析 XYZABC：'+str(values))
        if self.target is None:return
        # Reject samples from a query sent before the motion command.
        if sent<self.started:return
        self.position_error=tuple(t-p for p,t in zip(values[:3],self.target[:3]))
        self.orientation_error=angle_error(values[3:],self.target[3:])
        # The threshold never expands automatically when an arm fails to move.
        requested=math.dist(self.source_pose[:3],self.target[:3]) if self.source_pose else 1.
        tolerance=min(.2,max(.02,requested*.2))
        ok=max(map(abs,self.position_error))<=tolerance and self.orientation_error<=1
        if self.joint_target is not None:
            ok=(self.orientation_error<=1 and math.dist(values[:3],self.target[:3])<=.5 and self.joints_at is not None and self.joints_at>=self.started and now-self.joints_at<1.5 and max(abs(a-b) for a,b in zip(self.joints,self.joint_target))<=.3)
        self.arrivals=self.arrivals+1 if ok else 0
        self.status=f'等待到位 {now-self.started:.1f}s；位置误差 {tuple(round(v,3) for v in self.position_error)} mm；姿态误差 {self.orientation_error:.2f}°；合格 {self.arrivals}/3'
        if self.arrivals>=3:
            if getattr(self,'j5_probe_active',False):
                self.log('J5_PROBE_AFTER model_joints='+str(self.joints)+' model_pose='+str(values)+'；仅确认控制器反馈，实物方向需人工观察')
                self.j5_probe_active=False
            self.target=None;self.completed+=1;self.status=('移动完成：连续三次XYZ、姿态与关节反馈符合目标。' if self.joint_target is not None else '移动完成：连续三次位置与姿态反馈符合目标。')
            self.log(self.status)

    def tick(self,now):
        if self.fault:return
        if self.pending and now-self.pending[1]>2:self.fail('控制命令回复超时（使能/停止/夹爪）；未自动重发');return
        if self.recovery is not None:
            r=self.recovery
            if now>r['deadline']:self.fail('查询恢复连续30秒无有效探测进展，实际运动/使能状态未知');return
            if self.pending:return
            if r['phase']=='probe' and self.query and now-self.query[1]>2:
                self.bad_frame('恢复探测超时',now);return
            if r['phase']=='drain' and now-self.last_rx>=2.5:
                kind='joints' if r['stage']<2 else 'pose'
                r['phase']='probe';self.query=(kind,now)
                self.log('RECOVERY_QUERY '+kind)
                self.tx('#GETJPOS' if kind=='joints' else '#GETLPOS')
            return
        if self.query and now-self.query[1]>2:self.fail('串口查询超时；未继续交替查询，避免把迟到回复误分类');return
        if self.target is not None:
            elapsed=now-self.started
            if elapsed>=5 and not self.no_change_reported and self.pose and self.source_pose:
                if math.dist(self.pose[:3],self.source_pose[:3])<.05 and angle_error(self.pose[3:],self.source_pose[3:])<.1:
                    self.no_change_reported=True
                    self.log('诊断：发令5秒后末端反馈未明显变化；可能逆解/限位拒绝、执行链路或反馈更新异常，不能仅凭ok判断。')
                    if self.start_joints is not None and self.joints is not None:
                        changes=tuple(round(b-a,3) for a,b in zip(self.start_joints,self.joints))
                        self.log('同期关节角变化 J1..J6（度）：'+str(changes))
            if elapsed>=67 and not self.long_wait_reported:
                self.long_wait_reported=True
                self.log('观察已超过67秒仍未到位：继续采集反馈，不重发目标、不自动中止；需要中止请点击STOP。')
        if self.hand_queued is not None and self.query is None and self.pending is None:
            kind=self.hand_queued;self.hand_queued=None
            self.pending=(kind,now);self.tx('!HAND_O' if kind=='hand_open' else '!HAND_C')
            self.hand_status='夹爪命令已发送，等待控制器回执；不代表已夹稳'
            self.status=self.hand_status;return
        if self.query is None and self.pending is None and now>=self.next_query:
            self.query=(self.query_kind,now)
            self.tx('#GETLPOS' if self.query_kind=='pose' else '#GETJPOS')
