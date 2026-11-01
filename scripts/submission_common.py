"""Shared, dependency-free source inventory helpers (Python 3.10+)."""
from pathlib import Path
import hashlib
SKIP_DIRS={'.git','.venv','venv','__pycache__','.Xil','logs'}
def excluded(rel):
    return (any(p in SKIP_DIRS for p in rel.parts)
            or rel.parts[0]=='build'
            or rel.as_posix().startswith('validation/local/')
            or rel.as_posix()=='board/board_config.tcl'
            or rel.suffix in {'.pyc','.pyo','.dcp','.log','.jou','.str','.wdb'}
            or '.log.' in rel.name or rel.name in {'.env','Thumbs.db','.DS_Store'}
            or (rel.name.startswith('.env.') and rel.name!='.env.example'))
def inventory(root):
    return {p.relative_to(root).as_posix():{'sha256':hashlib.sha256(manifest_bytes(p)).hexdigest(),'bytes':len(manifest_bytes(p))}
            for p in sorted(root.rglob('*')) if p.is_file()
            and p.name!='SOURCE_MANIFEST.json' and not excluded(p.relative_to(root))}
def manifest_bytes(path):
    """Mirror Git text=auto LF storage; firmware artifact hashes stay raw."""
    data=path.read_bytes()
    if path.suffix.lower() not in {'.bin','.png','.jpg','.jpeg','.pdf','.docx','.zip','.bit','.elf','.xsa'} and b'\0' not in data[:8000]:
        data=data.replace(b'\r\n',b'\n')
    return data
def digest(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''): h.update(chunk)
    return h.hexdigest()
