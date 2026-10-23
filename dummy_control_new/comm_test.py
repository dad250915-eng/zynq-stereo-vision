"""Read-only serial test engine. No motion commands and no retry after anomaly."""
import json,math,time,uuid,hashlib,platform,sys
from datetime import datetime
from pathlib import Path

VERSION='comm-test-2026-09-27.1'

class CommTest:
    def __init__(self,send,root,port,mode='joints',interval=1.,duration=600.,timeout=2.,clock=time.monotonic):
        if mode not in ('joints','pose','alternate'):raise ValueError('无效查询模式')
        if interval not in (.2,1.,2.) or not 1<=duration<=86400 or timeout!=2.:raise ValueError('无效测试参数')
        self.send=send;self.clock=clock;self.start=clock();self.next=self.start+2.5
        self.deadline=self.start+duration;self.mode=mode;self.interval=interval;self.timeout=timeout
        self.active=True;self.pending=None;self.tx=0;self.ok=0;self.bad=0;self.timeouts=0;self.io_errors=0
        self.rtt_sum=0.;self.rtt_max=0.;self.last_rtt=None;self.reason='测试开始，等待2.5秒后查询';self.result=None
        self.directory=Path(root)/(datetime.now().strftime('%Y%m%d_%H%M%S_')+uuid.uuid4().hex[:8])
        self.directory.mkdir(parents=True);self.file=(self.directory/'events.jsonl').open('w',encoding='utf-8')
        hashes={}
        for name in ('comm_test.py','comm_test_ui.py','serial_link.py'):
            f=Path(__file__).parent/name
            if f.exists():hashes[name]=hashlib.sha256(f.read_bytes()).hexdigest()
        self.meta={'version':VERSION,'port':port,'mode':mode,'interval_s':interval,'duration_s':duration,'timeout_s':timeout,'python':sys.version,'platform':platform.platform(),'source_sha256':hashes}
        self.record('START',**self.meta)

    def record(self,event,**data):
        self.file.write(json.dumps({'wall_time':datetime.now().isoformat(timespec='milliseconds'),'elapsed_s':round(self.clock()-self.start,6),'event':event,**data},ensure_ascii=False)+'\n');self.file.flush()

    def stop(self,reason,category='manual'):
        if not self.active:return
        self.active=False;self.reason=reason
        if category=='bad_frame':self.bad+=1
        if category=='timeout':self.timeouts+=1
        if category=='io_error':self.io_errors+=1
        self.result={**self.meta,'reason':reason,'category':category,'elapsed_s':self.clock()-self.start,'sent':self.tx,'valid':self.ok,'bad_frames':self.bad,'timeouts':self.timeouts,'io_errors':self.io_errors,'last_rtt_ms':self.last_rtt,'max_rtt_ms':self.rtt_max,'mean_rtt_ms':self.rtt_sum/self.ok if self.ok else None,'pending':self.pending,'port_snapshot':None}
        try:
            self.record('STOP',**self.result)
            self.save_summary()
        finally:self.file.close()

    def save_summary(self):
        dest=self.directory/'summary.json';temp=self.directory/'summary.tmp'
        temp.write_text(json.dumps(self.result,ensure_ascii=False,indent=2),encoding='utf-8');temp.replace(dest)

    def port_snapshot(self,ports,error=None):
        if self.result is None:return
        self.result['port_snapshot']={'ports':ports,'error':error}
        self.save_summary()

    def event(self,kind,data,stamp):
        if not self.active:return
        if kind=='raw':
            self.record('RX_RAW',received_at=stamp,hex=data.hex(' '))
            if any(b>126 or (b<32 and b not in (10,13)) for b in data):self.stop('接收非ASCII/异常控制字节，已停止查询','bad_frame')
            return
        if kind=='error':self.record('IO_ERROR',detail=str(data));self.stop('串口读取异常：'+str(data),'io_error');return
        self.record('RX_LINE',received_at=stamp,text=data)
        if not data.strip():return
        if not self.pending:self.stop('收到未关联回复，停止以免混淆查询','bad_frame');return
        command,sent=self.pending
        if stamp<sent or stamp-sent>self.timeout:self.stop('查询回复迟到/时序异常','timeout');return
        fields=data.split()
        try:
            if len(fields)!=7 or fields[0]!='ok':raise ValueError()
            values=[float(x) for x in fields[1:]]
            if not all(math.isfinite(x) and abs(x)<=3.4e38 for x in values):raise ValueError()
        except (ValueError,OverflowError):self.stop('位置回复格式或数字无效','bad_frame');return
        self.last_rtt=(stamp-sent)*1000;self.rtt_sum+=self.last_rtt;self.rtt_max=max(self.rtt_max,self.last_rtt)
        self.ok+=1;self.pending=None;self.next=max(sent+self.interval,stamp+.05)
        self.record('VALID',query=command,values=values,rtt_ms=self.last_rtt)
        self.reason='正常查询中'

    def tick(self,now):
        if not self.active:return
        if self.pending and now-self.pending[1]>self.timeout:self.stop('查询2秒无完整回复，已停止查询','timeout');return
        if now>=self.deadline and self.pending is None:self.stop('设定时长结束','complete');return
        if self.pending is None and now>=self.next:
            kind=self.mode if self.mode!='alternate' else ('joints' if self.ok%2==0 else 'pose')
            command='#GETJPOS' if kind=='joints' else '#GETLPOS'
            self.record('TX_ATTEMPT',command=command);self.pending=(command,now)
            try:self.send(command)
            except Exception as exc:self.stop('串口写入异常：'+str(exc),'io_error');return
            self.tx+=1;self.record('TX_OK',command=command)
