"""Reader thread for each physical port. Opening never sends an ASCII command."""
import threading
import time
from usb_diagnostics import record

class SerialLink:
    def __init__(self,name,events,serial_factory=None):
        self.name=name;self.events=events;self.serial_factory=serial_factory
        self.ser=None;self.thread=None;self.stop=threading.Event();self.lock=threading.Lock()
        self.generation=0
        self.port_name=None;self.last_rx_at=None;self.last_tx_at=None;self.rx_bytes=0;self.tx_bytes=0;self.last_rx_waiting=None;self.partial_bytes=0;self.last_diag_at=0.
    @property
    def connected(self):return self.ser is not None and self.ser.is_open
    def open(self,port,baud=115200):
        if self.connected:raise RuntimeError("串口已连接")
        self.close()
        self.port_name=port;self.last_rx_at=None;self.last_tx_at=None;self.rx_bytes=0;self.tx_bytes=0;self.partial_bytes=0
        record('OPEN_ATTEMPT',port=port,baud=baud,dtr=False,rts=False)
        if self.serial_factory is None:
            import serial
            factory=serial.Serial
        else:factory=self.serial_factory
        ser=factory(port=None,baudrate=baud,timeout=0.1,write_timeout=0.5)
        # Avoid intentional DTR/RTS reset/enable. Adapter hardware may still differ.
        ser.dtr=False;ser.rts=False;ser.port=port
        try:ser.open();ser.reset_input_buffer()
        except Exception as exc:
            record('OPEN_ERROR',port=port,error=repr(exc));ser.close();raise
        self.ser=ser;self.stop.clear();self.generation+=1
        record('OPEN_OK',**self.snapshot())
        gen=self.generation
        self.thread=threading.Thread(target=self._read,args=(ser,gen),daemon=True)
        self.thread.start()
    def close(self):
        if self.ser is not None:record('CLOSE_REQUEST',**self.snapshot())
        self.stop.set();self.generation+=1
        with self.lock:
            if self.ser:
                try:self.ser.close()
                except Exception:pass
            self.ser=None
        if self.thread and self.thread is not threading.current_thread():self.thread.join(timeout=0.5)
        self.thread=None
    def send(self,line):
        if '\n' in line or '\r' in line:raise ValueError("只允许单条协议命令")
        data=(line+'\n').encode('ascii')
        with self.lock:
            if not self.connected:raise RuntimeError("机械臂串口未连接")
            begun=time.monotonic()
            record('WRITE_BEGIN',command=line,bytes=len(data),**self.snapshot())
            try:
                count=self.ser.write(data)
                if count!=len(data):raise IOError("串口命令未完整发送，禁止自动重发")
                self.last_tx_at=time.monotonic();self.tx_bytes+=count
                record('WRITE_OK',command=line,elapsed_ms=(self.last_tx_at-begun)*1000,count=count,meaning='host write completed; NOT device execution acknowledgement',**self.snapshot())
            except Exception as exc:
                record('WRITE_ERROR',command=line,elapsed_ms=(time.monotonic()-begun)*1000,error=repr(exc),**self.snapshot())
                raise
            # Windows output queue is host-side only. Failure is evidence, not a new control fault.
            try:record('HOST_OUTPUT_QUEUE',bytes_waiting=self.ser.out_waiting,**self.snapshot())
            except Exception as exc:record('HOST_OUTPUT_QUEUE_UNAVAILABLE',error=repr(exc),**self.snapshot())
    def snapshot(self):
        now=time.monotonic()
        return {'port':self.port_name,'generation':self.generation,'handle_open':self.connected,
                'reader_alive':bool(self.thread and self.thread.is_alive()),'event_backlog':self.events.qsize(),
                'rx_bytes':self.rx_bytes,'tx_bytes':self.tx_bytes,'last_rx_age_s':None if self.last_rx_at is None else now-self.last_rx_at,
                'last_write_age_s':None if self.last_tx_at is None else now-self.last_tx_at,
                'last_host_input_queue':self.last_rx_waiting,'partial_line_bytes':self.partial_bytes,
                'firmware_usb_busy':'unavailable'}

    def _emit(self,kind,data,gen):
        self.events.put((self.name,gen,kind,data,time.monotonic()))
    def _read(self,ser,gen):
        buffer=bytearray()
        try:
            while not self.stop.is_set() and gen==self.generation:
                operation='in_waiting';waiting=ser.in_waiting;self.last_rx_waiting=waiting
                if time.monotonic()-self.last_diag_at>=1:
                    self.last_diag_at=time.monotonic();record('READER_HEARTBEAT',**self.snapshot())
                if waiting>16384:
                    raise IOError("串口积压过多，不能把缓存旧帧当作新坐标")
                operation='read';data=ser.read(max(1,min(waiting,4096)))
                if not data:continue
                self.last_rx_at=time.monotonic();self.rx_bytes+=len(data)
                self._emit('raw',bytes(data),gen)
                buffer.extend(data)
                self.partial_bytes=len(buffer)
                if len(buffer)>8192:raise IOError("串口行过长或缺少换行")
                while b'\n' in buffer:
                    line,_,buffer=buffer.partition(b'\n')
                    self.partial_bytes=len(buffer)
                    self._emit('line',line.decode('ascii',errors='replace').rstrip('\r'),gen)
        except Exception as exc:
            if not self.stop.is_set():
                record('READ_ERROR',operation=locals().get('operation'),error=repr(exc),**self.snapshot())
                self._emit('error',str(exc),gen)
