from tkinter import ttk

def add_hand_panel(parent,app):
    p=ttk.LabelFrame(parent,text='手动夹爪与升降（逐步点击）',padding=6);p.pack(fill='x',pady=4)
    row=ttk.Frame(p);row.pack(fill='x')
    for text,fn in [('开爪',lambda:app.hand_command(True)),('闭爪',lambda:app.hand_command(False)),('抬起 Z+',lambda:app.vertical_move(1)),('下降 Z−',lambda:app.vertical_move(-1))]:
        ttk.Button(row,text=text,command=lambda f=fn:app.guard(f)).pack(side='left',padx=3)
    ttk.Label(row,text='距离mm').pack(side='left');ttk.Entry(row,textvariable=app.lift_distance,width=6).pack(side='left')
    ttk.Label(row,text='速度').pack(side='left');ttk.Entry(row,textvariable=app.lift_speed,width=4).pack(side='left')
    ttk.Label(p,textvariable=app.hand_text,wraplength=810).pack(anchor='w',pady=3)
    ttk.Label(p,text='升降须已处于参考朝下姿态；Z±距离分段执行，不保证实物严格直线。开闭爪使用固件电流设定，回执不代表已夹稳。',wraplength=810).pack(anchor='w')
    return p
