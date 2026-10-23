import time,queue,threading,math
from pathlib import Path
from tcp_calibration import load_profile
import tkinter as tk
from tkinter import ttk
from controller import numbers
from staged_motion import build_plan,Sequence
from motion_settings import DEFAULT_SPEED

class StagedWindow:
    def __init__(self,app):
        self.app=app;self.window=tk.Toplevel(app.root);self.window.title('分步定位：过渡 → 姿态 → 输入点');self.window.geometry('960x680')
        self.owner=None;self.token=None;self.baseline=None;self.steps=None;self.sequence=None;self.results=queue.Queue();self.closed=False
        self.abc=None;self.teach_owner=None;self.profile=None;self.auto_execute=False
        self.xyz=[tk.StringVar(value=v.get()) for v in app.fields[:3]]
        self.offset=[tk.StringVar(value='0') for _ in range(3)]
        self.clearance=tk.StringVar(value='100');self.side=tk.StringVar(value='30');self.speed=tk.StringVar(value=str(DEFAULT_SPEED))
        self.tcp=tk.BooleanVar(value=False);self.confirm_path=tk.BooleanVar(value=False)
        self.teach_text=tk.StringVar(value='尚未记录抓取朝向；先在原控制界面将夹爪调整为所需朝向并静止')
        self.state=tk.StringVar(value='只规划端点，不含避障；尚未发送运动')
        p=ttk.Frame(self.window,padding=12);p.pack(fill='both',expand=True)
        row=ttk.Frame(p);row.pack(fill='x')
        for label,var in zip('XYZ',self.xyz):ttk.Label(row,text=label+' mm').pack(side='left');ttk.Entry(row,textvariable=var,width=12).pack(side='left',padx=5)
        ttk.Button(row,text='读取主窗口目标',command=lambda:app.guard(self.copy_manual)).pack(side='left',padx=5)
        ttk.Button(row,text='读取摄像头锁定目标',command=lambda:app.guard(self.copy_camera)).pack(side='left',padx=5)
        ttk.Label(p,text='默认输入控制器末端XYZ，与原程序一致。最后到输入点本身；不自动夹爪。',wraplength=920).pack(anchor='w',pady=6)
        ttk.Button(p,text='加载已标定TCP（以后只需XYZ）',command=lambda:app.guard(self.load_tcp)).pack(anchor='w')
        ttk.Button(p,text='记录当前朝向为抓取朝向',command=lambda:app.guard(self.teach)).pack(anchor='w')
        ttk.Label(p,textvariable=self.teach_text,wraplength=920).pack(anchor='w',pady=6)
        row=ttk.Frame(p);row.pack(fill='x')
        for text,var in [('过渡高度mm',self.clearance),('过渡X偏移mm',self.side),('速度',self.speed)]:ttk.Label(row,text=text).pack(side='left');ttk.Entry(row,textvariable=var,width=8).pack(side='left',padx=5)
        ttk.Checkbutton(p,text='输入的是夹取中心坐标：启用下方工具偏移（工具局部坐标系，需实测）',variable=self.tcp).pack(anchor='w',pady=6)
        row=ttk.Frame(p);row.pack(fill='x')
        for text,var in zip(('工具dx mm','工具dy mm','工具dz mm'),self.offset):ttk.Label(row,text=text).pack(side='left');ttk.Entry(row,textvariable=var,width=8).pack(side='left',padx=5)
        ttk.Label(p,text='摄像头当前试验转换输出的是控制器末端目标，不应再当作已标定的夹取中心。记录朝向本身不验证夹爪是否向下。',wraplength=920).pack(anchor='w',pady=5)
        row=ttk.Frame(p);row.pack(fill='x')
        ttk.Button(row,text='计算并预览三步',command=lambda:app.guard(self.preview)).pack(side='left',padx=5)
        ttk.Button(row,text='TCP：按输入XYZ自动执行',command=lambda:app.guard(self.execute_tcp)).pack(side='left',padx=4)
        self.start=ttk.Button(row,text='开始分步定位',command=lambda:app.guard(self.start_run),state='disabled');self.start.pack(side='left',padx=5)
        ttk.Button(row,text='取消规划 / STOP',command=lambda:app.guard(self.stop)).pack(side='left',padx=5)
        ttk.Checkbutton(p,text='已核对三步目标及整个运动空间（关节运动不保证直线，不含桌面/自碰撞检查）',variable=self.confirm_path).pack(anchor='w',pady=8)
        self.output=tk.Text(p,height=9,wrap='word',state='disabled');self.output.pack(fill='both',expand=True)
        ttk.Label(p,textvariable=self.state,wraplength=920).pack(anchor='w',pady=5)
        ttk.Label(p,textvariable=app.status,wraplength=920).pack(anchor='w')
        self.camera_snapshot=None;self.signature=None;self.window.protocol('WM_DELETE_WINDOW',self.close);self.job=self.window.after(50,self.poll)
    def load_tcp(self):
        if self.app.planning:raise ValueError('请先完成或取消规划')
        self.profile=load_profile(Path(__file__).parent/'tcp_profile.json')
        self.abc=tuple(self.profile['down_abc_deg']);self.tcp.set(True)
        for var,x in zip(self.offset,self.profile['offset_mm']):var.set(str(x))
        self.teach_text.set('已加载经实物确认的TCP与向下朝向；输入为基座系夹取中心XYZ')

    def execute_tcp(self):
        self.load_tcp()
        self.clearance.set('100');self.side.set('30');self.speed.set(str(DEFAULT_SPEED))
        self.auto_execute=True
        try:self.preview()
        except Exception:self.auto_execute=False;raise

    def values(self):
        return (tuple(v.get() for v in self.xyz),self.clearance.get(),self.side.get(),self.speed.get(),self.tcp.get(),tuple(v.get() for v in self.offset))
    def copy_manual(self):
        for dst,src in zip(self.xyz,self.app.fields):dst.set(src.get())
        self.camera_snapshot=None
    def copy_camera(self):
        if self.profile is not None:raise ValueError('旧相机转换输出控制器末端坐标，不能用作TCP输入；需要重新做相机到TCP对应标定')
        w=self.app.camera_window
        if w is None or not w.link.connected:raise ValueError('先在摄像头窗口连接并锁定目标')
        if not w.allow.get():raise ValueError('先在摄像头窗口确认试验标定许可')
        target=w.capture.check(time.monotonic())
        for v,x in zip(self.xyz,target):v.set(str(float(x)))
        self.tcp.set(False);self.camera_snapshot=(w,w.link.generation,tuple(target))
    def check_camera(self):
        if self.camera_snapshot is None:return
        w,gen,xyz=self.camera_snapshot
        if w.closed or not w.link.connected or w.link.generation!=gen or not w.allow.get():raise ValueError('摄像头连接或许可已改变，请重新读取目标')
        if tuple(w.capture.check(time.monotonic()))!=xyz or numbers([v.get() for v in self.xyz],3)!=xyz:raise ValueError('摄像头目标已改变，请重新读取')
    def teach(self):
        if self.app.planning:raise ValueError('先取消规划或完成运动')
        c=self.app.require();c.check_auto_ready(time.monotonic())
        rows=list(c.joint_history)
        if len(rows)<3 or time.monotonic()-rows[0][0]>3 or any(max(x[1][j] for x in rows)-min(x[1][j] for x in rows)>.1 for j in range(6)):raise ValueError('等待三次稳定关节反馈再记录朝向')
        self.profile=None;self.auto_execute=False
        self.abc=tuple(c.pose[3:]);self.teach_owner=c;self.teach_text.set('已记录抓取ABC(°)：'+str(self.abc)+'；本次机械臂连接有效')
        self.app.log('STAGED_TEACH '+str(self.abc))
    def release(self,message):
        if self.token==self.app.plan_token:self.app.planning=False
        self.auto_execute=False
        self.steps=None;self.sequence=None;self.owner=None;self.start.configure(state='disabled');self.state.set(message)
    def preview(self):
        if self.app.planning:raise ValueError('已有规划或动作流程，请先完成或取消')
        if self.profile is not None and self.camera_snapshot is not None:raise ValueError('旧相机目标不能用于TCP模式，请输入基座系TCP目标并点读取主窗口目标清除旧来源')
        c=self.app.require();c.check_auto_ready(time.monotonic());self.check_camera()
        if self.abc is None or (self.profile is None and self.teach_owner is not c):raise ValueError('请先记录当前连接的抓取朝向')
        signature=self.values();target=numbers(signature[0],3);height,side=numbers(signature[1:3],2)
        speed=numbers([signature[3]],1)[0]
        if speed!=int(speed) or not 1<=speed<=100:raise ValueError('速度须为1～100整数')
        offset=numbers(signature[5],3) if self.tcp.get() else (0,0,0)
        self.baseline=c.joints;self.owner=c;self.token=self.app.plan_token;self.signature=signature;self.run_speed=speed
        self.app.planning=True;self.confirm_path.set(False);self.state.set('离线检查全部三步；尚未发送运动')
        baseline=self.baseline;abc=self.abc;token=self.token
        def work():
            try:steps=build_plan(target,baseline,abc,height,side,offset,False);error=None
            except Exception as exc:steps=None;error=str(exc)
            self.results.put((token,c,steps,error))
        threading.Thread(target=work,daemon=True).start()
    def start_run(self):
        if not self.steps or self.sequence is not None:raise ValueError('请先计算并预览三步')
        if self.values()!=self.signature:raise ValueError('参数已修改，请取消后重新规划')
        if not self.confirm_path.get():raise ValueError('请先核对三步目标和运动空间')
        c=self.app.require();c.check_auto_ready(time.monotonic());self.check_camera()
        if c is not self.owner or self.token!=self.app.plan_token:raise ValueError('连接/规划已失效')
        if max(abs(a-b) for a,b in zip(c.joints,self.baseline))>.5:raise ValueError('规划后起始关节已改变，请重新规划')
        if self.camera_snapshot:self.camera_snapshot[0].capture.used=True
        self.sequence=Sequence(c,self.steps,self.run_speed);self.start.configure(state='disabled');self.state.set('执行分步定位；每阶段等待三次到位反馈')
    def stop(self):
        if self.owner is not None:
            running=self.sequence is not None
            self.app.plan_token+=1;self.app.planning=False
            self.release('分步定位已取消，不自动续跑')
            if running:self.app.stop()
    def poll(self):
        if self.closed:return
        try:
            if self.owner is not None and (self.owner is not self.app.controller or self.token!=self.app.plan_token or not self.app.link.connected):self.release('会话改变或STOP，分步定位已取消')
            while not self.results.empty():
                token,c,steps,error=self.results.get_nowait()
                if c is not self.owner or token!=self.app.plan_token:continue
                if error:self.release(error);self.app.log('STAGED_PLAN_REJECTED '+error);continue
                self.steps=steps;self.output.configure(state='normal');self.output.delete('1.0','end')
                for i,s in enumerate(steps):self.output.insert('end',f"{i+1}. {s['name']}\n末端XYZABC：{tuple(round(float(v),3) for v in s['pose'])}\n关节：{s['joints']}\n")
                self.output.configure(state='disabled');self.start.configure(state='normal');self.state.set('三个端点求解成功，未验证路径无碰撞；核对后点击开始')
                if self.auto_execute:
                    self.auto_execute=False;self.confirm_path.set(True);self.start_run()
            if self.sequence is not None:
                self.sequence.tick(time.monotonic())
                if self.sequence.done:self.release('分步定位完成：已到输入目标；未闭爪');self.app.root.bell()
                else:self.state.set(f'分步定位 {min(3,self.sequence.index+1)}/3；'+self.owner.status)
        except Exception as exc:
            self.app.log('STAGED_ABORT '+str(exc));self.release('流程停止：'+str(exc)+'；不自动续跑')
        self.job=self.window.after(50,self.poll)
    def close(self):
        self.stop();self.closed=True;self.window.after_cancel(self.job);self.window.destroy();self.app.staged_window=None
