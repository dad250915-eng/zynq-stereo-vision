"""Separate camera window; shares existing robot controller, never auto-enables."""
import queue,time,threading
import tkinter as tk
from tkinter import ttk
from serial.tools import list_ports
from serial_link import SerialLink
from camera_mode_core import CameraCapture,load_calibration
from camera_pick_place import plan_pick_place as solve,CameraBatch
from motion_settings import DEFAULT_SPEED

class CameraWindow:
    def __init__(self,app,root_path):
        self.app=app;self.window=tk.Toplevel(app.root);self.window.title('摄像头三维控制模式');self.window.geometry('880x900')
        self.events=queue.Queue();self.link=SerialLink('FPGA_CAMERA',self.events)
        self.capture=CameraCapture(load_calibration(root_path/'camera_calibration.json'))
        self.batch=CameraBatch()
        self.results=queue.Queue();self.closed=False;self.started_controller=None;self.started_token=None
        self.port=tk.StringVar();self.speed=tk.StringVar(value=str(DEFAULT_SPEED));self.allow=tk.BooleanVar(value=False)
        self.live=tk.StringVar(value='实时相机 XYZ：—');self.fixed=tk.StringVar(value='锁定相机 XYZ：—')
        self.compensated=tk.StringVar(value='补偿后相机抓取 XYZ：—')
        self.target=tk.StringVar(value='转换机械臂 XYZ：—');self.status=tk.StringVar(value=self.capture.reason)
        p=ttk.Frame(self.window,padding=12);p.pack(fill='both',expand=True)
        row=ttk.Frame(p);row.pack(fill='x')
        self.box=ttk.Combobox(row,textvariable=self.port,state='readonly',width=55);self.box.pack(side='left')
        for text,fn in [('刷新',self.refresh),('连接/断开 FPGA',self.connect)]:ttk.Button(row,text=text,command=lambda f=fn:app.guard(f)).pack(side='left',padx=4)
        ttk.Label(p,text='FPGA 串口115200；先关闭旧控制台/Vitis终端对此串口的占用。机械臂连接和使能仍在主窗口。',wraplength=840).pack(anchor='w',pady=8)
        for var in (self.live,self.fixed,self.compensated,self.target):ttk.Label(p,textvariable=var,font=('Microsoft YaHei UI',13),wraplength=840).pack(anchor='w',pady=6)
        ttk.Label(p,text=f'抓取补偿：相机 Z {self.capture.depth_offset_mm:+g}mm 后转换；范围检查使用补偿后的相机点。',wraplength=840).pack(anchor='w')
        ttk.Label(p,text='稳定条件：连续5个不同帧、跨度至少2秒，各轴极差≤20mm；锁定后不随新帧改变；标定范围外扩20mm。\n实时反馈超过3秒未更新或偏离锁定点超过20mm时，暂停允许开始；重新采集可解除锁定。',wraplength=840).pack(anchor='w',pady=8)
        d=self.capture.cal
        ttk.Label(p,text=f"试验标定：{d.get('calibration_id','未标识')}，{len(d['camera_samples'])}组。拟合RMS {d['rmse_mm']:.2f}mm，最大残差 {d['max_error_mm']:.2f}mm；留组验证RMS {d.get('loo_rmse_mm',0):.2f}mm。尚未独立实测验收，输出为J5修正模型目标坐标。",wraplength=840,foreground='#a04000').pack(anchor='w',pady=6)
        ttk.Checkbutton(p,text='允许本窗口使用上述试验标定运动（已知误差，确认路径；不含避障）',variable=self.allow).pack(anchor='w')
        row=ttk.Frame(p);row.pack(fill='x',pady=10)
        ttk.Button(row,text='重新采集 / 解锁',command=lambda:app.guard(self.reset)).pack(side='left')
        ttk.Label(row,text='速度指令').pack(side='left',padx=8);ttk.Entry(row,textvariable=self.speed,width=5).pack(side='left')
        self.start=ttk.Button(row,text='开始两次抓放',command=lambda:app.guard(self.move));self.start.pack(side='left',padx=12)
        ttk.Button(row,text='STOP 停止',command=lambda:app.guard(app.stop)).pack(side='left')
        from hand_panel import add_hand_panel
        add_hand_panel(p,app)
        home=ttk.LabelFrame(p,text='记录 / 返回姿态（与主窗口共享）',padding=8);home.pack(fill='x',pady=6)
        row=ttk.Frame(home);row.pack(fill='x')
        ttk.Button(row,text='记录返回姿态',command=lambda:app.guard(app.record_return_pose)).pack(side='left',padx=4)
        ttk.Label(row,text='返回速度指令').pack(side='left',padx=5)
        ttk.Entry(row,textvariable=app.return_speed,width=5).pack(side='left')
        ttk.Button(row,text='返回记录姿态',command=lambda:app.guard(app.return_to_recorded)).pack(side='left',padx=10)
        ttk.Label(home,textvariable=app.return_text,wraplength=830).pack(anchor='w',pady=4)
        ttk.Label(home,text='记录不发送运动；机械臂断开重连后失效。返回不依赖摄像头标定，不含避障。',wraplength=830).pack(anchor='w')
        ttk.Label(home,textvariable=app.status,wraplength=830).pack(anchor='w',pady=4)
        ttk.Progressbar(home,variable=app.progress,maximum=100).pack(fill='x')
        ttk.Label(p,textvariable=self.status,wraplength=840).pack(anchor='w',pady=8)
        ttk.Label(p,text='第一件锁定后点击一次开始。抓取闭爪后回参考下垂姿态，到(0,-150,165)开爪、升到210，再自动解锁采集第二件；第二件放到(0,-220,165)开爪、升到210后结束。只解锁上位机，不发送FPGA暂停命令。途中不保证朝下或直线，无避障。',wraplength=840).pack(anchor='w')
        self.refresh();self.window.protocol('WM_DELETE_WINDOW',self.close);self.job=self.window.after(50,self.poll)
    def refresh(self):
        self.ports={f'{p.device} | {p.description}':p.device for p in list_ports.comports()};self.box['values']=list(self.ports)
    def cancel_plan(self):
        self.abort_batch('摄像头采集被取消')
        if self.started_controller is not None and self.app.planning and self.started_token==self.app.plan_token:
            self.app.plan_token+=1;self.app.planning=False
        self.started_controller=None
    def abort_batch(self,reason,stop_motion=True):
        if not self.batch.active:return
        c=self.batch.controller
        owned_sequence=getattr(self.app,'sequence',None)
        if owned_sequence is not None and owned_sequence.c is c and owned_sequence.plan['kind']=='camera_pick_place':
            self.app.sequence=None
        self.batch.abort(reason);self.capture.armed=False
        self.app.plan_token+=1;self.app.planning=False;self.started_controller=None
        if c is self.app.controller:
            c.hand_queued=None
            if stop_motion and not c.stop_sent:
                try:c.stop(time.monotonic())
                except Exception as exc:self.app.log('CAMERA_BATCH_STOP_ERROR '+str(exc))
        self.app.log('CAMERA_BATCH_ABORT '+reason)
    def check_batch_session(self):
        b=self.batch
        if (b.controller is not self.app.controller or b.token!=self.app.plan_token
                or not self.link.connected or b.generation!=self.link.generation
                or not self.allow.get()):raise ValueError('抓放期间连接、许可或会话改变')
        c=b.controller
        if c.fault or not c.enabled or c.stop_sent:raise ValueError('机械臂故障、失能或STOP')
    def on_sequence_done(self,seq,now):
        self.check_batch_session()
        self.batch.completed(seq.plan['slot'],self.capture,now)
        self.app.log('CAMERA_BATCH_CYCLE_DONE slot='+str(seq.plan['slot']+1)+' retract='+str(seq.plan['retract']))
        if self.batch.state=='waiting_second':
            self.fixed.set('锁定相机 XYZ：—');self.compensated.set('补偿后相机抓取 XYZ：—');self.target.set('转换机械臂 XYZ：—')
    def reset(self):
        self.cancel_plan();self.batch=CameraBatch();self.capture.reset();self.compensated.set('补偿后相机抓取 XYZ：—');self.fixed.set('锁定相机 XYZ：—');self.target.set('转换机械臂 XYZ：—')
    def connect(self):
        if self.link.connected:
            self.cancel_plan();self.link.close();self.capture.invalid('FPGA已断开');return
        if self.app.test_window is not None:raise ValueError('请关闭只读测试窗口')
        port=self.ports.get(self.port.get())
        if not port:raise ValueError('请选择 FPGA 串口')
        if self.app.link.connected and port.upper()==str(self.app.link.port_name).upper():raise ValueError('FPGA不能与机械臂共用串口')
        self.link.open(port,115200);self.reset();self.app.log('CAMERA_CONNECTED '+port+'（只读）')
    def move(self,automatic=False):
        CameraWindow.check_coordinate_model(self)
        if not self.allow.get():raise ValueError('当前为试验标定，请先阅读误差说明并勾选试验运动')
        if not self.link.connected:raise ValueError('FPGA未连接')
        if self.app.planning or getattr(self.app,'sequence',None) is not None:raise ValueError('当前运动或求解尚未完成')
        if self.batch.active and not (automatic and self.batch.state=='waiting_second'):
            raise ValueError('两次抓放流程尚未结束')
        xyz=self.capture.check(time.monotonic());c=self.app.require();c.check_auto_ready(time.monotonic())
        speed=self.batch.speed if automatic else int(self.speed.get())
        if not 1<=speed<=100:raise ValueError('速度须为1～100整数')
        baseline=c.joints;token=self.app.plan_token;gen=self.link.generation
        if automatic:
            self.check_batch_session();self.batch.plan_second()
        else:self.batch.begin(c,token,gen,speed)
        slot=self.batch.slot
        self.app.log('CAMERA_VERTICAL_REQUEST XYZ='+str(xyz)+' baseline='+str(baseline))
        self.app.planning=True;self.started_controller=c;self.started_token=token;self.status.set('正在求解；尚未发送运动')
        def work():
            try:result=solve(xyz,baseline,slot);error=None
            except Exception as exc:result=None;error=str(exc)
            self.results.put((c,token,gen,xyz,baseline,speed,result,error))
        threading.Thread(target=work,daemon=True).start()
    def check_coordinate_model(self):
        if self.capture.cal.get("coordinate_model") != "j5-reversed-v1":
            raise ValueError("J5方向已修正：旧相机标定坐标不再适用，需要在新模型下重新标定")

    def commit(self,item):
        c,token,gen,xyz,baseline,speed,result,error=item
        if token!=self.app.plan_token or c is not self.app.controller:return
        if self.capture.used:raise ValueError('本次目标已使用，忽略重复规划回报')
        self.app.planning=False;self.started_controller=None
        try:
            if error:raise ValueError(error)
            self.check_batch_session();CameraWindow.check_coordinate_model(self)
            if not self.allow.get() or not self.link.connected or gen!=self.link.generation:raise ValueError('求解期间连接/许可改变，未发送')
            if tuple(self.capture.check(time.monotonic()))!=tuple(xyz):raise ValueError('目标已经改变，未发送')
            self.app.require()
            self.capture.used=True # Never silently retry a used target.
            self.app.log('CAMERA_MOVE slot='+str(result['slot']+1)+' locked='+str(self.capture.locked)+' grasp_camera='+str(self.capture.grasp_camera)+' robot='+str(xyz))
            self.batch.started(result['slot'])
            self.app.start_reference_plan(c,result,baseline,speed)
        except Exception as exc:
            self.abort_batch(str(exc));raise
    def poll(self):
        if self.closed:return
        now=time.monotonic()
        try:
            if self.events.qsize()>3000:
                self.cancel_plan();self.link.close();self.capture.invalid('摄像头串口积压，请重连')
            for _ in range(1000):
                try:_,gen,kind,data,stamp=self.events.get_nowait()
                except queue.Empty:break
                if gen!=self.link.generation:continue
                if kind=='error':self.cancel_plan();self.link.close();self.capture.invalid('FPGA串口错误：'+data)
                elif kind=='line':
                    before=self.capture.locked;self.capture.feed(data,stamp,now)
                    if data.startswith('XYZ '):self.app.log('CAMERA_RX '+data)
                    if before is None and self.capture.locked is not None:
                        self.app.log('CAMERA_READY locked='+str(self.capture.locked)+' grasp_camera='+str(self.capture.grasp_camera)+' robot='+str(self.capture.target));self.window.bell()
                        if self.capture.frozen:self.app.log('CAMERA_SECOND_FROZEN 已锁定有效第二目标；后续相机坐标不再接收')
            while not self.results.empty():self.app.guard(lambda:self.commit(self.results.get_nowait()))
            if self.batch.active:
                try:self.check_batch_session()
                except ValueError as exc:self.abort_batch(str(exc))
            if self.batch.state=='waiting_second' and self.capture.locked is None:
                self.batch.reason='等待第二件：'+self.capture.reason
            if self.batch.state=='waiting_second' and self.capture.locked is not None:
                try:
                    self.capture.check(now)
                    # Communication recovery only pauses the second start.
                    c=self.batch.controller
                    if c.recovery is None and not c.await_fresh and c.pose_at is not None and c.joints_at is not None and max(now-c.pose_at,now-c.joints_at)<=1.5:
                        self.move(automatic=True)
                except ValueError as exc:
                    self.batch.reason='等待第二件有效目标：'+str(exc)
            if self.capture.frozen:
                self.live.set('第二次相机 XYZ 已冻结(mm)：'+str(tuple(round(float(v),3) for v in self.capture.locked)))
            else:
                try:s=self.capture.tracker.current(3,now);self.live.set('实时相机 XYZ(mm)：'+str(s.xyz))
                except ValueError as exc:self.live.set('实时相机 XYZ：无有效新数据；'+str(exc))
            if self.capture.locked is not None:
                self.fixed.set('锁定相机 XYZ(mm)：'+str(tuple(round(v,3) for v in self.capture.locked)))
                self.compensated.set('补偿后相机抓取 XYZ(mm)：'+str(tuple(round(v,3) for v in self.capture.grasp_camera)))
                self.target.set('转换机械臂 XYZ(mm)：'+str(tuple(round(v,3) for v in self.capture.target)))
            ready=False
            try:
                self.capture.check(now);ready=self.link.connected and self.allow.get() and not self.app.planning and not self.batch.active
                message='已准备，点击开始两次抓放' if ready else '坐标已锁定；等待试验运动许可或求解结束'
            except ValueError as exc:message=self.capture.reason if self.capture.locked is None else str(exc)
            if self.capture.used and self.app.controller:message='本次目标已使用；'+self.app.controller.status
            if self.batch.state!='idle':message=self.batch.reason+('；'+self.app.controller.status if self.batch.active and self.app.controller else '')
            if self.batch.state in ('done','aborted'):ready=False
            self.status.set(message);self.start.configure(state='normal' if ready else 'disabled')
        except Exception as exc:
            self.abort_batch('摄像头处理异常：'+str(exc))
            self.capture.invalid('摄像头处理异常：'+str(exc));self.app.log(self.capture.reason)
        self.job=self.window.after(50,self.poll)
    def close(self):
        self.cancel_plan();self.closed=True;self.window.after_cancel(self.job);self.link.close();self.window.destroy();self.app.camera_window=None
