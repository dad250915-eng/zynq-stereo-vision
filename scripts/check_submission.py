"""Static submission checks; never invokes Vivado, downloads or robot motion."""
from pathlib import Path
import argparse,json,re,xml.etree.ElementTree as ET
from submission_common import inventory,excluded
def check(root,check_manifest=False):
    errors=[]; warnings=[]
    required=['README.md','docs/reproduction.md','CONSTRAINT_SETTINGS.json','CUSTOM_IP_VERSIONS.json',
              'fpga1_bd/design_1/design_1.bd','fpga2_bd/design_1/design_1.bd','fpga2_vitis/display_demo.c',
              'fpga2_vitis/lscript.ld','dummy_control_new/main.py','dummy_control_new/requirements.txt',
              'calibration/left_map_u32.bin','calibration/right_map_u32.bin','skill/README.md',
              'report/ai_collaboration.md','third_party/vivado_library_LICENSE.txt','third_party/vitis_vision/LICENSE.txt']
    for name in required:
        if not (root/name).is_file():errors.append('Missing required file: '+name)
    for p in root.rglob('*'):
        rel=p.relative_to(root)
        if not excluded(rel) and (not p.name.isascii() or any(c.isspace() for c in p.name)):
            errors.append('Non-English/whitespace name: '+rel.as_posix())
    for lane in ['left','right']:
        p=root/f'calibration/{lane}_map_u32.bin'
        if p.is_file() and p.stat().st_size!=1280*720*4:errors.append('Wrong rectification map size: '+str(p))
    components=0
    ns={'s':'http://www.spiritconsortium.org/XMLSchema/SPIRIT/1685-2009'}
    for p in root.rglob('component.xml'):
        if excluded(p.relative_to(root)):continue
        components+=1
        try:tree=ET.parse(p)
        except ET.ParseError as e:errors.append(f'{p.relative_to(root)}: {e}');continue
        for n in tree.findall('.//s:file/s:name',ns):
            if n.text and not (p.parent/n.text).is_file():errors.append(f'IP reference missing: {p.relative_to(root)} -> {n.text}')
    for cfg in ['CONSTRAINT_SETTINGS.json','CUSTOM_IP_VERSIONS.json']:
        p=root/cfg
        if not p.exists():continue
        try:data=json.loads(p.read_text(encoding='utf-8-sig'))
        except (ValueError,OSError) as e:errors.append(f'{cfg}: {e}');continue
        if cfg.startswith('CONSTRAINT'):
            for entries in data.values():
                for item in entries:
                    if not (root/item['file']).is_file():errors.append('Missing XDC: '+item['file'])
        else:
            for item in data:
                if item.get('kind')=='hls':
                    name=item['vlnv'].split(':')[2]
                    p=root/f'fpga2_bd/design_1/ip/{name}/src/{name}.cpp'
                    if not p.is_file():errors.append('Missing HLS source: '+str(p.relative_to(root)))
    p=root/'fpga2_bd/design_1/ip/roi_mode_button/src/roi_mode_button.v'
    if p.exists() and 'mode_depth==2?0:' not in p.read_text():errors.append('Mode button is not expected three-mode version')
    p=root/'fpga2_vitis/roi_pipeline_config.h'
    if p.exists():
        s=p.read_text()
        for key,value in [('ROI_WIDTH','1280U'),('ROI_HEIGHT','720U'),('ROI_COLOR_STRIDE_WORDS','1920U')]:
            if not re.search(r'#define\s+'+key+r'\s+'+value+r'\b',s):errors.append('Unexpected image contract: '+key)
    inv=inventory(root)
    if check_manifest:
        p=root/'SOURCE_MANIFEST.json'
        if not p.exists():errors.append('Missing SOURCE_MANIFEST.json')
        else:
            known=json.loads(p.read_text(encoding='utf-8-sig'))['files']
            for name,v in known.items():
                if name not in inv:errors.append('Manifest file missing: '+name)
                elif inv[name]['sha256']!=v['sha256'] or inv[name]['bytes']!=v['bytes']:errors.append('Manifest changed: '+name)
            for name in sorted(set(inv)-set(known)):errors.append('Manifest unlisted: '+name)
    if not (root/'LICENSE').exists():warnings.append('Project-wide license unresolved; see docs/license_status.md')
    warnings.extend(['Historical routed reports are not final triple-mode timing proof.',
                     'Clean HLS/Vivado/XSCT build and new board measurements remain pending.',
                     'AI excerpts/summaries do not replace an original full transcript export.'])
    return {'static_status':'PASS' if not errors else 'FAIL','scope':'static source/package checks only',
            'manifest_checked':check_manifest,'inventory_files':len(inv),'inventory_bytes':sum(x['bytes'] for x in inv.values()),
            'component_packages_checked':components,'errors':errors,'remaining_items':warnings}
def main():
    a=argparse.ArgumentParser();a.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);a.add_argument('--check-manifest',action='store_true');a.add_argument('--out',type=Path);n=a.parse_args()
    result=check(n.root.resolve(),n.check_manifest);s=json.dumps(result,ensure_ascii=False,indent=2)+'\n'
    if n.out:n.out.parent.mkdir(parents=True,exist_ok=True);n.out.write_text(s,encoding='utf-8')
    print(s);return int(bool(result['errors']))
if __name__=='__main__':raise SystemExit(main())
