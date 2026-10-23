import tkinter as tk
from tkinter import ttk,messagebox
from serial.tools import list_ports
import queue,time
from serial_link import SerialLink
from comm_test import CommTest,VERSION

class TestWindow:
    def __init__(self,parent,root,on_close):
        self.window=tk.Toplevel(parent);self.window.title('只读通信测试 — '+VERSION);self.window.geometry('820x450')
        self.root=root;self.on_close=on_close;self.events=queue.Queue();self.link=SerialLink('COMM_TEST',self.events)
        self.test=None;self.finished=True;self.closed=False
        self.port=tk.StringVar();self.mode=tk.StringVar(value='仅关节');self.period=tk.StringVar(value='1');self.minutes=tk.StringVar(value='10')
        self.stable=tk.BooleanVar(value=False);self.status=tk.StringVar(value='未连接，不会发送运动、使能或停止指令。');self.stats=tk.StringVar();self.path=tk.StringVar()
        body=ttk.Frame(self.window,padding=15);body.pack(fill='both',expand=True)
        ttk.Label(body,text='独立只读测试：异常自动结束并关闭测试串口，不重连、不重试。').pack(anchor='w')
        row=ttk.Frame(body);row.pack(fill='x',pady=12)
        self.box=ttk.Combobox(row,textvariable=self.port,width=65,state='readonly');self.box.pack(side='left')
        ttk.Button(row,text='刷新端口',command=self.refresh).pack(side='left',padx=8)
        row=ttk.Frame(body);row.pack(fill='x',pady=5)
        ttk.Combobox(row,textvariable=self.mode,values=['仅关节','仅末端','交替查询'],width=12,state='readonly').pack(side='left')
        ttk.Label(row,text='查询间隔(秒)').pack(side='left',padx=8)
        ttk.Combobox(row,textvariable=self.period,values=['2','1','0.2'],width=6,state='readonly').pack(side='left')
        ttk.Label(row,text='时长(分钟)').pack(side='left',padx=8);ttk.Entry(row,textvariable=self.minutes,width=7).pack(side='left')
        ttk.Checkbutton(body,text='已现场确认机械臂静止且有可靠支撑，其他软件不会发送动作',variable=self.stable).pack(anchor='w',pady=12)
        row=ttk.Frame(body);row.pack(fill='x')
        ttk.Button(row,text='开始测试',command=self.start).pack(side='left',padx=5)
        ttk.Button(row,text='停止测试并保存',command=lambda:self.stop('用户停止测试')).pack(side='left',padx=5)
        ttk.Label(body,textvariable=self.status,wraplength=780).pack(anchor='w',pady=15)
        ttk.Label(body,textvariable=self.stats,wraplength=780).pack(anchor='w')
        ttk.Label(body,textvariable=self.path,wraplength=780).pack(anchor='w',pady=12)
        ttk.Label(body,text='“停止测试”只停止查询，不是机械臂急停。异常报告包含原始字节和端口枚举快照。',wraplength=780).pack(anchor='w')
        self.refresh();self.window.protocol('WM_DELETE_WINDOW',self.close);self.window.after(50,self.poll)

    def refresh(self):
        try:self.ports={f'{p.device} | {p.description}':p for p in list_ports.comports()};self.box['values']=list(self.ports)
        except Exception as exc:self.status.set('枚举端口失败：'+str(exc))

    def start(self):
        if self.test and self.test.active:return
        try:
            if not self.stable.get():raise ValueError('请先现场确认静止和支撑，再勾选确认项')
            if self.port.get() not in self.ports:raise ValueError('请选择当前可用串口')
            port=self.ports[self.port.get()]
            duration=float(self.minutes.get())*60
            self.test=CommTest(self.link.send,self.root/'logs/comm_tests',{'device':port.device,'description':port.description,'vid':port.vid,'pid':port.pid,'serial_number':port.serial_number,'hwid':port.hwid},mode={'仅关节':'joints','仅末端':'pose','交替查询':'alternate'}[self.mode.get()],interval=float(self.period.get()),duration=duration)
            self.finished=False;self.path.set('报告目录：'+str(self.test.directory))
            try:self.link.open(port.device,115200)
            except Exception as exc:self.test.stop('打开串口失败：'+str(exc),'io_error');self.finish()
        except Exception as exc:messagebox.showerror('未开始',str(exc),parent=self.window)

    def finish(self):
        if self.finished:return
        self.finished=True;self.link.close()
        try:
            ports=[{'device':p.device,'description':p.description,'hwid':p.hwid} for p in list_ports.comports()]
            self.test.port_snapshot(ports)
        except Exception as exc:
            try:self.test.port_snapshot([],str(exc))
            except Exception as save_error:self.status.set('报告保存失败：'+str(save_error));return
        self.status.set('测试已停止：'+self.test.reason);self.window.bell()

    def stop(self,reason):
        if self.test and self.test.active:self.test.stop(reason)
        if self.test:self.finish()

    def poll(self):
        if self.closed:return
        try:
            if self.test and self.test.active:
                if self.events.qsize()>3000:self.test.stop('接收事件积压','io_error')
                for _ in range(1000):
                    try:_,gen,kind,data,stamp=self.events.get_nowait()
                    except queue.Empty:break
                    if gen==self.link.generation:self.test.event(kind,data,stamp)
                self.test.tick(time.monotonic())
                if not self.test.active:self.finish()
            if self.test:
                t=self.test;elapsed=(time.monotonic()-t.start if t.active else t.result['elapsed_s'])
                self.stats.set(f'运行 {elapsed:.1f}s | 已发送 {t.tx} | 有效回复 {t.ok} | 坏帧 {t.bad} | 超时 {t.timeouts} | I/O异常 {t.io_errors} | 最近/最大耗时 {t.last_rtt or 0:.1f}/{t.rtt_max:.1f} ms')
                if t.active:self.status.set(t.reason)
        except Exception as exc:
            try:
                if self.test and self.test.active:self.test.stop('测试程序异常：'+str(exc),'io_error')
            finally:self.link.close();self.finished=True;self.status.set('测试异常已停止：'+str(exc))
        self.window.after(50,self.poll)

    def close(self):
        self.stop('关闭测试窗口');self.link.close();self.closed=True;self.window.destroy();self.on_close()
