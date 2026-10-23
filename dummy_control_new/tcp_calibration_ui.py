import time,json
from pathlib import Path
import tkinter as tk
from tkinter import ttk
from tcp_calibration import fit_tcp,check_holdout,save_profile

class TCPWindow:
    def __init__(self,app,root):
        self.app=app;self.root=root;self.owner=None;self.samples=[];self.result=None;self.validation=None;self.down=None
        self.window=tk.Toplevel(app.root);self.window.title('一次性TCP标定（只记录反馈，不发运动）');self.window.geometry('900x600')
        self.confirm=tk.BooleanVar(value=False);self.status=tk.StringVar(value='未采集；不会自动移动或开闭夹爪')
        p=ttk.Frame(self.window,padding=12);p.pack(fill='both',expand=True)
        text=('TCP定义：两根夹指最前端之间的中心点。保持夹爪开口和安装不变，固定一个细尖/十字基准。\n'
              '用原有手动控制从至少5个不同朝向让TCP对准同一固定点，静止后每次点击记录。\n'
              '需改变至少两个方向的倾角；只平移、只绕一个轴旋转不够。无需知道固定点的基座坐标。\n'
              '拟合后，换一个未用于拟合的姿态再对准原固定点做独立验证。\n'
              '最后在有空间的位置使夹爪实际竖直向下，记录朝向；此时不必对准固定点。')
        ttk.Label(p,text=text,wraplength=860).pack(anchor='w',pady=8)
        row=ttk.Frame(p);row.pack(fill='x')
        for title,fn in [('记录同点样本',self.record),('删除最后样本',self.undo),('计算工具偏移',self.fit),('记录独立验证点',self.validate),('记录当前向下朝向',self.record_down)]:
            ttk.Button(row,text=title,command=lambda f=fn:app.guard(f)).pack(side='left',padx=2)
        self.output=tk.Text(p,height=14,wrap='word',state='disabled');self.output.pack(fill='both',expand=True,pady=8)
        ttk.Checkbutton(p,text='我已确认独立验证对准原固定点、所记录的夹爪朝向确实向下，工具安装和开口未改变',variable=self.confirm).pack(anchor='w')
        ttk.Button(p,text='保存已验证TCP参数',command=lambda:app.guard(self.save)).pack(anchor='w',pady=8)
        ttk.Label(p,textvariable=self.status,wraplength=860).pack(anchor='w')
        ttk.Label(p,text='参数保存后用于基座系TCP目标；原七组相机转换不是TCP外参，仍需重新标定。',wraplength=860).pack(anchor='w')
        self.window.protocol('WM_DELETE_WINDOW',self.close)
    def pose(self):
        if self.app.planning:raise ValueError('规划或运动流程进行中，不能采集')
        c=self.app.require();now=time.monotonic();c.check_auto_ready(now)
        if self.owner is not None and c is not self.owner:raise ValueError('机械臂连接已经改变，请关闭此窗口重新采集，不能混合不同零位')
        rows=list(c.joint_history)
        if len(rows)<3 or now-rows[0][0]>3 or rows[-1][0]-rows[0][0]<.4 or any(max(r[1][j] for r in rows)-min(r[1][j] for r in rows)>.1 for j in range(6)):raise ValueError('请等待三个稳定关节反馈')
        self.owner=c;return tuple(c.pose)
    def invalidate(self):self.result=None;self.validation=None;self.confirm.set(False)
    def display(self,text):
        self.output.configure(state='normal');self.output.insert('end',text+'\n');self.output.see('end');self.output.configure(state='disabled');self.status.set(text)
    def record(self):
        p=self.pose();self.samples.append(p);self.invalidate();self.display(f'样本{len(self.samples)}：{p}');self.app.log('TCP_SAMPLE '+str(p))
        (self.root/'tcp_samples_draft.json').write_text(json.dumps(self.samples,indent=2),encoding='utf-8')
    def undo(self):
        if self.samples:self.samples.pop()
        self.invalidate();self.display(f'剩余{len(self.samples)}个样本')
    def fit(self):
        self.result=fit_tcp(self.samples);self.validation=None;self.confirm.set(False)
        self.display('工具偏移mm：'+str(self.result['offset_mm'])+f"；RMS={self.result['rmse_mm']:.3f}mm；请采集独立验证点")
    def validate(self):
        if self.result is None:raise ValueError('先计算工具偏移')
        p=self.pose();err=check_holdout(self.result,p)
        self.validation=p if err<=5 else None;self.confirm.set(False);self.display(f'独立验证误差={err:.3f}mm；'+('通过数值检查' if err<=5 else '不通过，重新对准'))
    def record_down(self):
        self.down=self.pose();self.confirm.set(False);self.display('已记录待现场确认的向下ABC：'+str(self.down[3:]))
    def save(self):
        if self.app.controller is not self.owner or not self.app.link.connected:raise ValueError('连接已改变，不能保存本会话标定')
        if self.result is None or self.validation is None or self.down is None:raise ValueError('需要工具拟合、独立验证及向下朝向记录')
        d=save_profile(self.root/'tcp_profile.json',self.result,self.down,self.validation,self.confirm.get())
        self.app.log('TCP_PROFILE_SAVED '+str(d));self.display('已保存 tcp_profile.json；分步窗口点击“加载已标定TCP”，以后不需重复输入姿态和偏移')
    def close(self):self.window.destroy();self.app.tcp_window=None
