"""Independent Dummy XYZ terminal. Importing does not connect to hardware."""
from pathlib import Path
import sys,os,time,queue,logging,threading
from logging.handlers import RotatingFileHandler
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'vendor'))
if (ROOT/'vendor/tcl/tcl8.6/init.tcl').exists():
    os.environ['TCL_LIBRARY']=str(ROOT/'vendor/tcl/tcl8.6')
    os.environ['TK_LIBRARY']=str(ROOT/'vendor/tcl/tk8.6')
import tkinter as tk
from tkinter import ttk,messagebox
from serial.tools import list_ports
from controller import numbers
from motion_settings import DEFAULT_SPEED
from mapped_controller import MappedController as Controller
from serial_link import SerialLink
from usb_diagnostics import record as diag_record,enumerate_ports_async,VERSION


class App:
    def __init__(self,root):
        self.root=root;root.title('Dummy 独立XYZ控制与串口诊断');root.geometry('1080x940')
        self.events=queue.Queue();self.link=SerialLink('ROBOT',self.events);self.controller=None
        self.port=tk.StringVar();self.fields=[tk.StringVar() for _ in range(3)]
        self.plans=queue.Queue();self.plan_token=0;self.planning=False;self.sequence=None
        self.return_speed=tk.StringVar(value=str(DEFAULT_SPEED));self.return_text=tk.StringVar(value='尚未记录返回姿态（本次连接有效）')
        self.lift_distance=tk.StringVar(value='100');self.lift_speed=tk.StringVar(value=str(DEFAULT_SPEED));self.hand_text=tk.StringVar(value='夹爪尚未操作；无夹持到位反馈')
        self.speed=tk.StringVar(value=str(DEFAULT_SPEED))
        self.status=tk.StringVar(value='未连接。连接只读取诊断，不会自动使能或移动。')
        self.pose_text=tk.StringVar(value='XYZABC：未收到');self.joints_text=tk.StringVar(value='J1..J6：未收到')
        self.link_text=tk.StringVar();self.note=tk.StringVar();self.progress=tk.DoubleVar(value=0)
        self.done=0;self.last_pose_at=None;self.test_window=None;self.camera_window=None;self.staged_window=None;self.tcp_window=None;self.diag_at=0.;self.poll_at=time.monotonic();self.port_diag_at=-100.
        (ROOT/'logs').mkdir(exist_ok=True)
        self.handler=RotatingFileHandler(ROOT/'logs/console.log',maxBytes=4*1024*1024,backupCount=3,encoding='utf-8')
        self.handler.setFormatter(logging.Formatter('%(asctime)s %(message)s'))
        body=ttk.Frame(root,padding=12);body.pack(fill='both',expand=True)
        ports=ttk.LabelFrame(body,text='串口连接（115200 / 8N1）',padding=8);ports.pack(fill='x')
        self.box=ttk.Combobox(ports,textvariable=self.port,width=65,state='readonly');self.box.pack(side='left',padx=4)
        ttk.Button(ports,text='刷新端口',command=self.refresh).pack(side='left',padx=4)
        ttk.Button(ports,text='连接 / 断开',command=lambda:self.guard(self.connect)).pack(side='left',padx=4)
        ttk.Button(body,text='打开只读通信测试（需先断开控制串口）',command=lambda:self.guard(self.open_comm_test)).pack(anchor='w',pady=4)
        ttk.Button(body,text='摄像头三维控制模式',command=lambda:self.guard(self.open_camera_mode)).pack(anchor='w',pady=2)
        ttk.Label(body,textvariable=self.link_text,wraplength=1030).pack(anchor='w',pady=4)
        controls=ttk.LabelFrame(body,text='使能后直接输入目标并移动',padding=8);controls.pack(fill='x',pady=6)
        row=ttk.Frame(controls);row.pack(fill='x')
        ttk.Button(row,text='1. 使能电机 !START',command=lambda:self.guard(self.enable)).pack(side='left',padx=5)
        ttk.Button(row,text='STOP 停止',command=lambda:self.guard(self.stop)).pack(side='left',padx=12)
        ttk.Label(row,text='没有工作范围限制或二次确认。输入的是机械臂基座绝对坐标。').pack(side='left')
        row=ttk.Frame(controls);row.pack(fill='x',pady=7)
        for name,var in zip('XYZ',self.fields[:3]):
            ttk.Label(row,text=name+' (mm)').pack(side='left');ttk.Entry(row,textvariable=var,width=12).pack(side='left',padx=5)
        ttk.Label(row,text='速度指令 1～100').pack(side='left',padx=8)
        ttk.Entry(row,textvariable=self.speed,width=5).pack(side='left')
        ttk.Button(row,text='2. 开始XYZ抓取',command=lambda:self.guard(self.move)).pack(side='left',padx=12)
        row=ttk.Frame(controls);row.pack(fill='x',pady=4)
        ttk.Label(row,text='XYZ抓取：开爪 → 参考下垂 → 到输入XYZ朝下 → 闭爪后停住').pack(side='left')
        ttk.Button(row,text='把当前XYZ填入输入框',command=lambda:self.guard(self.fill_current)).pack(side='left',padx=8)
        ttk.Label(controls,text='终点姿态(0,180,0)。途中允许改变姿态、走弧线；无上方过渡、无避障，请确认路径有间隙。').pack(anchor='w')
        probe=ttk.Frame(controls);probe.pack(fill='x',pady=5)
        ttk.Button(probe,text=f'修正后J5验证：模型+3°（速度{DEFAULT_SPEED}）',command=lambda:self.guard(self.j5_probe)).pack(side='left',padx=4)
        ttk.Label(probe,text='发送原始J5 −3°；预期下倾。每次连接限一次，STOP可中止。').pack(side='left')
        preset=ttk.Frame(controls);preset.pack(fill='x',pady=5)
        ttk.Button(preset,text=f'到参考下垂姿态（速度{DEFAULT_SPEED}）',command=lambda:self.guard(self.reference_preset)).pack(side='left',padx=4)
        ttk.Label(preset,text='参考程序预设：XYZ=(160,0,270)，固定腕部分支；实物姿态待核对，不含避障。').pack(side='left')
        home=ttk.LabelFrame(body,text='返回记录姿态（不是 XYZ=0,0,0）',padding=6);home.pack(fill='x')
        row=ttk.Frame(home);row.pack(fill='x')
        ttk.Button(row,text='记录返回姿态',command=lambda:self.guard(self.record_return_pose)).pack(side='left',padx=5)
        ttk.Label(row,text='返回速度指令').pack(side='left')
        ttk.Entry(row,textvariable=self.return_speed,width=5).pack(side='left',padx=5)
        ttk.Button(row,text='返回记录姿态',command=lambda:self.guard(self.return_to_recorded)).pack(side='left',padx=5)
        ttk.Label(home,textvariable=self.return_text,wraplength=1000).pack(anchor='w')
        ttk.Label(home,text='保持上电使用；断开后记录失效。返回按记录关节角执行，可能不是垂直姿态；不含避障。').pack(anchor='w')
        from hand_panel import add_hand_panel
        add_hand_panel(body,self)
        info=ttk.LabelFrame(body,text='3. 反馈与执行状态',padding=8);info.pack(fill='x',pady=5)
        self.raw_text=tk.StringVar(value='J5方向修正已启用；原始反馈待接收')
        ttk.Label(info,textvariable=self.raw_text,wraplength=1000).pack(anchor='w')
        ttk.Label(info,textvariable=self.pose_text,font=('Consolas',11)).pack(anchor='w')
        ttk.Label(info,textvariable=self.joints_text,font=('Consolas',11)).pack(anchor='w',pady=4)
        ttk.Progressbar(info,variable=self.progress,maximum=100).pack(fill='x',pady=5)
        ttk.Label(info,textvariable=self.status,wraplength=1000).pack(anchor='w')
        ttk.Label(info,textvariable=self.note,wraplength=1000).pack(anchor='w',pady=4)
        ttk.Label(body,text='原始收发与诊断（自动保存 '+str(ROOT/'logs/console.log')+'）').pack(anchor='w',pady=3)
        logframe=ttk.Frame(body);logframe.pack(fill='both',expand=True)
        self.logs=tk.Text(logframe,height=10,state='disabled',wrap='word');self.logs.pack(side='left',fill='both',expand=True)
        scroll=ttk.Scrollbar(logframe,command=self.logs.yview);scroll.pack(side='right',fill='y');self.logs.configure(yscrollcommand=scroll.set)
        ttk.Label(body,text='连接后自动交替读取末端位姿与关节角；超67秒只提示未到位，不重发目标。通信故障时尝试STOP。\n不自动HOME、复位、切换模式或修改零位。软件STOP不等于硬件急停。',justify='left').pack(anchor='w',pady=5)
        self.refresh();self.log('独立程序启动；版本 '+VERSION+'；串口未打开。')
        root.protocol('WM_DELETE_WINDOW',self.close);root.after(50,self.poll)

    def log(self,text):
        if str(text).startswith(('FAULT ','BAD_FRAME ')):
            diag_record('APPLICATION_ALERT',detail=str(text),transport=self.link.snapshot())
            if time.monotonic()-self.port_diag_at>10:
                self.port_diag_at=time.monotonic();enumerate_ports_async(str(text))
        self.handler.handle(logging.LogRecord('dummy',logging.INFO,'',0,str(text),(),None))
        self.logs.configure(state='normal');self.logs.insert('end',time.strftime('%H:%M:%S ')+str(text)+'\n')
        if int(self.logs.index('end-1c').split('.')[0])>1200:self.logs.delete('1.0','200.0')
        self.logs.see('end');self.logs.configure(state='disabled')

    def guard(self,fn):
        try:fn()
        except Exception as exc:self.log('未执行：'+str(exc));messagebox.showerror('未执行',str(exc),parent=self.root)

    def refresh(self):
        self.port_info={f'{p.device} | {p.description}':p for p in list_ports.comports()}
        self.box['values']=list(self.port_info)

    def confirm_disconnect(self):
        if self.controller and self.controller.target is not None:
            return messagebox.askyesno('动作未确认完成','断开后将无法读取反馈。已现场确认停止，仍要断开？',parent=self.root)
        return True

    def open_tcp_calibration(self):
        raise ValueError('当前仅使用模型末端垂直姿态，不使用TCP补偿模式')

    def open_staged(self):
        raise ValueError('旧任意姿态分步入口已停用；请使用统一的垂直XYZ控制')

    def open_camera_mode(self):
        if self.camera_window is not None:self.camera_window.window.lift();return
        if self.test_window is not None:raise ValueError('请先关闭只读通信测试窗口')
        from camera_mode_ui import CameraWindow
        self.camera_window=CameraWindow(self,ROOT)

    def open_comm_test(self):
        if self.camera_window is not None:raise ValueError('请先关闭摄像头模式窗口，再做独立只读测试')
        if self.test_window is not None:self.test_window.window.lift();return
        if self.link.connected or self.planning or (self.controller and self.controller.target is not None):
            raise ValueError('请先现场确认机械臂停止，结束控制会话并断开控制串口，再开始只读测试')
        from comm_test_ui import TestWindow
        self.test_window=TestWindow(self.root,ROOT,lambda:setattr(self,'test_window',None))

    def connect(self):
        if self.test_window is not None:raise ValueError('请先关闭只读测试窗口，再连接运动控制')
        if self.link.connected:
            if not self.confirm_disconnect():return
            if self.camera_window is not None:self.camera_window.abort_batch('机械臂连接断开',stop_motion=False)
            self.plan_token+=1;self.planning=False;self.sequence=None
            self.link.close();self.controller=None;self.return_text.set('已断开，返回姿态记录失效');self.status.set('已断开。');return
        if self.port.get() not in self.port_info:raise ValueError('请刷新并选择机械臂串口；关闭其他占用此端口的程序')
        p=self.port_info[self.port.get()]
        if self.camera_window is not None and self.camera_window.link.connected and p.device.upper()==str(self.camera_window.link.port_name).upper():raise ValueError('机械臂不能使用已连接的FPGA串口')
        self.link.open(p.device,115200)
        self.controller=Controller(self.send,self.log);self.return_text.set('尚未记录返回姿态（本次连接有效）');self.done=0;self.last_pose_at=None
        self.log(f'连接 {p.device} | {p.description} | VID={p.vid} PID={p.pid} | 序号={p.serial_number} | HWID={p.hwid}')
        self.log('未发送运动；开始按顺序查询 GETLPOS / GETJPOS。')

    def send(self,command):
        self.link.send(command);self.log('TX '+repr(command+'\n'))

    def require(self):
        if self.test_window is not None:raise ValueError('只读测试窗口打开期间禁止控制指令')
        if not self.link.connected or self.controller is None:raise ValueError('请先连接机械臂串口')
        return self.controller

    def enable(self):self.require().enable(time.monotonic())

    def stop(self):
        if self.camera_window is not None:self.camera_window.abort_batch('用户STOP',stop_motion=False)
        self.plan_token+=1;self.planning=False;self.sequence=None
        self.require().stop(time.monotonic())

    def check_camera_batch_idle(self):
        if self.camera_window is not None and self.camera_window.batch.active:
            raise ValueError('摄像头两次抓放正在执行或等待第二件；需要中止时请点击STOP')

    def record_return_pose(self):
        if self.planning:raise ValueError('正在求解目标，请稍候')
        c=self.require();c.record_return_pose(time.monotonic())
        self.return_text.set('已记录 J1～J6 (°)：'+str(c.return_joints))

    def return_to_recorded(self):
        self.check_camera_batch_idle()
        if self.planning:raise ValueError('正在求解目标，请稍候')
        c=self.require();c.return_to_recorded(self.return_speed.get(),time.monotonic())
        if c.target is not None:self.progress.set(0)

    def hand_command(self,opening):
        self.check_camera_batch_idle()
        if self.planning:raise ValueError('正在求解运动目标，请稍候')
        self.require().hand(opening,time.monotonic())

    def j5_probe(self):
        self.check_camera_batch_idle()
        if self.planning or self.sequence is not None:
            raise ValueError('当前流程尚未结束；需要中止时请点击STOP')
        from j5_probe import start_probe
        start_probe(self.require(),time.monotonic())
        self.progress.set(0)

    def reference_preset(self):
        self.check_camera_batch_idle()
        if self.planning or self.sequence is not None:raise ValueError('当前流程尚未结束；需要中止时请点击STOP')
        c=self.require();c.check_auto_ready(time.monotonic())
        from reference_preset import PRESET_XYZ,PRESET_SPEED,preset_solution
        q,p=preset_solution();baseline=c.joints
        self.log('REFERENCE_PRESET_REQUEST XYZ='+str(PRESET_XYZ)+' J1..J6='+str(q)+' speed='+str(PRESET_SPEED))
        c.move_down(PRESET_XYZ,q,p,baseline,PRESET_SPEED,time.monotonic())
        if c.target is not None:
            self.progress.set(0);c.status='参考下垂预设已发送；固定腕部分支，等待到位；请核对实物朝向。'

    def vertical_move(self,direction):
        if self.planning:raise ValueError('正在求解运动目标，请稍候')
        distance=numbers([self.lift_distance.get()],1)[0]
        if distance<=0:raise ValueError('升降距离须为正数，单位mm')
        c=self.require();c.check_auto_ready(time.monotonic())
        self.queue_down((c.pose[0],c.pose[1],c.pose[2]+direction*distance),self.lift_speed.get(),vertical=True)

    def fill_current(self):
        c=self.require()
        if c.pose is None:raise ValueError('尚未收到末端位置')
        for v,x in zip(self.fields[:3],c.pose[:3]):v.set(f'{x:.2f}')

    def move(self):
        self.queue_down(numbers([v.get() for v in self.fields],3),self.speed.get())

    def queue_down(self,xyz,speed,vertical=False):
        self.check_camera_batch_idle()
        c=self.require()
        if self.planning:raise ValueError('正在求解目标，请稍候')
        c.check_auto_ready(time.monotonic())
        xyz=numbers(xyz,3);speed=numbers([speed],1)[0]
        if speed!=int(speed) or not 1<=speed<=100:raise ValueError('速度须为1～100整数')
        baseline=c.joints;token=self.plan_token;self.planning=True
        self.log('VERTICAL_REQUEST XYZ='+str(xyz)+' baseline='+str(baseline))
        c.status='正在校验运动目标；XYZ采用参考下垂后直接到终点，失败不发送，STOP仍可用。'
        def work():
            try:
                from reference_motion import plan_grasp,plan_vertical
                result=(plan_vertical if vertical else plan_grasp)(xyz,baseline);error=None
            except Exception as exc:result=None;error=str(exc)
            self.plans.put((c,token,xyz,baseline,speed,result,error))
        threading.Thread(target=work,daemon=True).start()

    def start_reference_plan(self,c,plan,baseline,speed):
        if c is not self.require():raise ValueError('控制会话改变，路径作废')
        c.check_auto_ready(time.monotonic())
        if max(abs(a-b) for a,b in zip(c.joints,baseline))>.5:raise ValueError('规划期间关节改变，请重新规划')
        from reference_motion import MotionSequence
        from grasp_sequence import GraspSequence
        from camera_pick_place import CameraPickPlaceSequence
        sequence_type={'grasp':GraspSequence,'camera_pick_place':CameraPickPlaceSequence}.get(plan['kind'],MotionSequence)
        self.sequence=sequence_type(c,plan,speed);self.sequence_token=self.plan_token;self.planning=True
        self.progress.set(0)
        self.log('REFERENCE_PLAN target='+str(plan['target'])+' above_mm='+str(plan['clearance'])+' steps='+str(len(plan['steps'])))
        c.status=('摄像头第'+str(plan['slot']+1)+'次抓放已开始；放置到位后开爪并上升。' if plan['kind']=='camera_pick_place' else '阶段目标验证完成；XYZ抓取先开爪，到位后闭爪；单独升降不自动开闭爪。')

    def tick_reference(self,now):
        seq=self.sequence
        if seq is None:return
        if seq.c is not self.controller or self.sequence_token!=self.plan_token:
            self.sequence=None;self.planning=False;return
        try:
            seq.tick(now)
            self.progress.set(100*seq.index/max(1,len(seq.steps)))
            if seq.done:
                self.sequence=None;self.planning=False
                if seq.plan['kind']=='camera_pick_place':
                    if self.camera_window is None:raise ValueError('摄像头窗口已关闭，不能继续下一次')
                    self.camera_window.on_sequence_done(seq,now)
                    seq.c.status=self.camera_window.batch.reason
                else:
                    seq.c.status=('目标到位，闭爪指令已确认；请观察是否抓稳，再手动抬起。' if seq.plan['kind']=='grasp' else '升降定位完成。')
                self.root.bell()
            else:
                name=seq.steps[min(seq.index,len(seq.steps)-1)]['name']
                seq.c.status='阶段 '+str(seq.index+1)+'/'+str(len(seq.steps))+' '+name+'；'+seq.c.status.split('；')[-1]
        except Exception as exc:
            self.sequence=None;self.planning=False;self.plan_token+=1
            if seq.plan['kind']=='camera_pick_place' and self.camera_window is not None:
                self.camera_window.abort_batch(str(exc),stop_motion=False)
            seq.c.hand_queued=None
            if (seq.c.target is not None or seq.plan['kind']=='camera_pick_place') and not seq.c.stop_sent:
                try:seq.c.stop(now)
                except Exception:pass
            seq.c.status='流程中止，不续跑：'+str(exc);self.log(seq.c.status)

    def poll(self):
        now=time.monotonic();poll_gap=now-self.poll_at;self.poll_at=now
        c=self.controller
        if c and now-self.diag_at>=2:
            self.diag_at=now
            diag_record('APPLICATION_HEARTBEAT',ui_poll_gap_s=poll_gap,query=c.query,query_age_s=None if c.query is None else now-c.query[1],target=c.target,fault=c.fault,recovering=c.recovery is not None,transport=self.link.snapshot())
        while not self.plans.empty():
            owner,token,xyz,baseline,speed,result,error=self.plans.get_nowait()
            if owner is not c or token!=self.plan_token:continue
            self.planning=False
            if error:
                c.status=error;self.log(error)
            else:
                self.guard(lambda:self.start_reference_plan(c,result,baseline,speed))
        try:
            if self.events.qsize()>3000:
                if c:c.fail('串口消息积压，无法保证诊断新鲜度')
                self.link.close()
            for _ in range(1000):
                try:_,gen,kind,line,stamp=self.events.get_nowait()
                except queue.Empty:break
                if gen!=self.link.generation:continue
                if kind=='raw':
                    self.log('RX_RAW hex='+line.hex(' ')+' bytes='+repr(line))
                    if c:c.raw_activity(stamp)
                    continue
                self.log(('RX ' if kind=='line' else 'ERROR ')+repr(line))
                if kind=='error':
                    if c:c.fail('串口读取失败：'+line)
                    self.link.close()
                elif c:c.receive(line,stamp)
            if c and self.link.connected:c.tick(time.monotonic())
            if self.sequence is not None:self.tick_reference(time.monotonic())
        except Exception as exc:
            if c:c.fail(str(exc))
            self.log('控制循环异常：'+str(exc))
        if c:
            now=time.monotonic();self.status.set(c.status);self.hand_text.set(c.hand_status)
            self.raw_text.set('控制器原始 J1..J6：'+str(c.raw_joints)+'；原始XYZABC：'+str(c.raw_pose))
            self.pose_text.set('修正模型 XYZABC：'+str(c.pose)+(f'  接收年龄 {now-c.pose_at:.1f}s' if c.pose_at is not None else ''))
            self.joints_text.set('模型关节 J1..J6（J5已反号）：'+str(c.joints)+(f'  接收年龄 {now-c.joints_at:.1f}s' if c.joints_at is not None else ''))
            self.link_text.set(f'串口：{"连接" if self.link.connected else "断开"}；使能确认：{("未知" if c.enabled is None else c.enabled)}；TX/RX行数 {c.tx_lines}/{c.rx_lines}；最近查询往返 {c.last_rtt or 0:.0f} ms；{c.ack}')
            self.note.set(c.observation)
            if self.sequence is None and c.target and c.position_error and c.source_pose:
                import math
                initial=math.dist(c.source_pose[:3],c.target[:3]);remaining=math.sqrt(sum(x*x for x in c.position_error))
                self.progress.set(max(0,min(99,100*(1-remaining/max(initial,.01)))))
            if c.completed>self.done:
                self.done=c.completed
                if self.sequence is None:self.progress.set(100)
        self.root.after(50,self.poll)

    def close(self):
        if self.test_window is not None:self.test_window.close()
        if self.controller and (self.controller.target is not None or self.sequence is not None):
            if not messagebox.askyesno('退出','请求软件STOP并退出？请在现场确认实体停止。',parent=self.root):return
            try:self.controller.stop(time.monotonic())
            except Exception:pass
        if self.tcp_window is not None:self.tcp_window.close()
        if self.staged_window is not None:self.staged_window.close()
        if self.camera_window is not None:self.camera_window.close()
        self.link.close();self.handler.close();self.root.destroy()


if __name__=='__main__':
    root=tk.Tk();app=App(root)
    if '--smoke-test' in sys.argv:root.after(300,app.close)
    root.mainloop()

