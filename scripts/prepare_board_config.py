"""Bind a specific ARM ELF32, bitstream, init script and maps. No device I/O."""
from pathlib import Path
import argparse,json,struct,sys
from submission_common import digest
MAP_BYTES=1280*720*4
def symbols(path):
    b=path.read_bytes()
    if len(b)<52 or b[:7]!=b'\x7fELF\x01\x01\x01':raise ValueError('Expected little-endian ELF32')
    h=struct.unpack_from('<16sHHIIIIIHHHHHH',b)
    if h[2]!=40:raise ValueError('Expected ARM ELF')
    shoff,shsize,count=h[6],h[11],h[12]
    if shsize!=40 or count==0 or shoff+count*40>len(b):raise ValueError('Unsupported or truncated section table')
    sections=[struct.unpack_from('<IIIIIIIIII',b,shoff+i*40) for i in range(count)]
    result={}
    for s in sections:
        if s[1]!=2:continue
        if s[9]!=16 or s[5]%16 or s[6]>=count or s[4]+s[5]>len(b):raise ValueError('Invalid symbol table')
        t=sections[s[6]]
        if t[4]+t[5]>len(b):raise ValueError('Invalid string table')
        strings=b[t[4]:t[4]+t[5]]
        for off in range(s[4],s[4]+s[5],16):
            n,value,size,info,other,idx=struct.unpack_from('<IIIBBH',b,off)
            if not idx or n>=len(strings):continue
            name=strings[n:].split(b'\0',1)[0].decode('ascii',errors='replace')
            result[name]=(value,size)
    return result
def quote(s):
    s=str(s).replace('\\','/')
    if any(c in s for c in '{}\r\n'):raise ValueError('Use a simple path without braces/newlines')
    return '{'+s+'}'
def prepare(elf,bit,init,left,right):
    paths={'elf':elf,'bit':bit,'init':init,'left_map':left,'right_map':right}
    for p in paths.values():
        if not p.is_file():raise ValueError('Missing artifact: '+str(p))
    if left.stat().st_size!=MAP_BYTES or right.stat().st_size!=MAP_BYTES:raise ValueError('Incorrect map size')
    syms=symbols(elf)
    needed=['rectifyStorage','leftRectifyMap','rightRectifyMap','rectify_maps_wait_marker']
    absent=[s for s in needed if s not in syms]
    if absent:raise ValueError('Required symbols missing (use matching unstripped ELF): '+', '.join(absent))
    storage,size=syms['rectifyStorage']
    if size<2*MAP_BYTES or storage%4:raise ValueError('Unexpected rectifyStorage layout')
    # left/right symbols are POINTER VARIABLES, not the backing map arrays.
    # Contract verified against this submission's display_demo.c initialization.
    if not (0x100000<=storage and storage+2*MAP_BYTES<=0x10000000):raise ValueError('Map storage is outside expected DDR range or overlaps ROI buffers')
    marker=syms['rectify_maps_wait_marker'][0]
    if marker&1:raise ValueError('Thumb marker requires a separately validated breakpoint workflow')
    out={k:str(p.resolve()) for k,p in paths.items()}
    out.update({k+'_sha256':digest(p) for k,p in paths.items()})
    out.update({'python':sys.executable,'left_addr':storage,'right_addr':storage+MAP_BYTES,'left_ptr_symbol':syms['leftRectifyMap'][0],
                'right_ptr_symbol':syms['rightRectifyMap'][0],'marker':marker,'target_filter':'name =~ "*A9*#0"'})
    return out
def main():
    root=Path(__file__).resolve().parents[1]
    a=argparse.ArgumentParser();a.add_argument('--elf',type=Path,required=True);a.add_argument('--bit',type=Path,required=True);a.add_argument('--ps-init',type=Path,required=True)
    a.add_argument('--out',type=Path,required=True);a.add_argument('--left-map',type=Path,default=root/'calibration/left_map_u32.bin');a.add_argument('--right-map',type=Path,default=root/'calibration/right_map_u32.bin');n=a.parse_args()
    try:data=prepare(n.elf,n.bit,n.ps_init,n.left_map,n.right_map)
    except (ValueError,struct.error) as e:a.error(str(e))
    lines=['# Auto-generated for this exact build. Verify PS init provenance before use.','set board_config [dict create \\']
    items=list(data.items())
    for i,(k,v) in enumerate(items):lines.append('  '+k+' '+(str(v) if isinstance(v,int) else quote(v))+(' \\' if i<len(items)-1 else ']'))
    n.out.parent.mkdir(parents=True,exist_ok=True);n.out.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(json.dumps({'config':str(n.out),'left_addr':hex(data['left_addr']),'right_addr':hex(data['right_addr']),'marker':hex(data['marker']),
                      'scope':'No device access. Pairing of bit/init/hardware remains the builder responsibility.'},indent=2))
if __name__=='__main__':main()
