"""Host-side evidence only: no device commands or firmware USB_BUSY claims."""
import json,logging,time,threading
from logging.handlers import RotatingFileHandler
from pathlib import Path
from datetime import datetime
_lock=threading.Lock();_handler=None
VERSION='reference-preset-2026-09-29.10'

def record(event,**fields):
    global _handler
    try:
        with _lock:
            if _handler is None:
                folder=Path(__file__).parent/'logs';folder.mkdir(exist_ok=True)
                _handler=RotatingFileHandler(folder/'usb_diagnostics.jsonl',maxBytes=8*1024*1024,backupCount=4,encoding='utf-8')
                _handler.setFormatter(logging.Formatter('%(message)s'))
            payload=json.dumps({'time':datetime.now().isoformat(timespec='milliseconds'),'mono':time.monotonic(),'event':event,**fields},ensure_ascii=False,default=str)
            _handler.handle(logging.LogRecord('usb_diag',logging.INFO,'',0,payload,(),None))
    except Exception:
        # Diagnostics must never interrupt transport or issue recovery commands.
        pass

def enumerate_ports_async(reason):
    def work():
        try:
            from serial.tools import list_ports
            ports=[{'device':p.device,'vid':p.vid,'pid':p.pid,'serial_number':p.serial_number,'description':p.description,'hwid':p.hwid} for p in list_ports.comports()]
            record('PORT_ENUMERATION',reason=reason,ports=ports,meaning='OS enumeration only; presence does not prove device responsive')
        except Exception as exc:record('PORT_ENUMERATION_ERROR',reason=reason,error=repr(exc))
    threading.Thread(target=work,daemon=True).start()
