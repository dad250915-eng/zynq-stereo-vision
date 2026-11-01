"""Aggregate log metrics independently; do not add overlapping stage times."""
from pathlib import Path
import argparse,json,re,statistics
def parse(text):
    records=[]; current=None; skipped=[]
    for line in text.splitlines():
        if line.startswith('PERF frame='):
            if current is not None:records.append(current)
            pairs=dict(re.findall(r'(\w+)=([^\s]+)',line))
            current={'frame':int(pairs['frame']),'roi':pairs.get('roi'),'units':pairs.get('units')}
        elif current is not None and line.startswith('PERF IP '):
            for k,v in re.findall(r'\b([A-Z])=(-?\d+)',line):current['ip_'+k]=int(v)
        elif current is not None and line.startswith('PERF GHI='):
            for k,v in re.findall(r'\b(GHI|total|prep|present)=(-?\d+)',line):current['roi_'+k]=int(v)
        elif current is not None and line.startswith('PERF overlap_D_GHI='):
            m=re.search(r'overlap_D_GHI=(-?\d+)',line)
            if m:current['overlap_D_GHI']=int(m.group(1))
        elif current is not None and line.startswith('PERF CAPTURE '):
            if re.search(r'\bunits=us\b',line):
                for k,v in re.findall(r'\b(copy_us|park_wait|rect_left|rect_right|rect_pair|pre_roi|round)=(-?\d+)',line):current['capture_'+k]=int(v)
        elif line.startswith(('PERF DEPTH ','PERF FUSION ')):
            if current is not None:records.append(current)
            current=None
            skipped.append(line)
    if current is not None:records.append(current)
    accepted=[r for r in records if r.get('units')=='us']
    metrics={}
    for key in sorted({k for r in accepted for k in r if k.startswith(('ip_','roi_','capture_','overlap_'))}):
        values=[r[key] for r in accepted if key in r and r[key]>=0]
        if values:
            metrics[key]={'n':len(values),'min_us':min(values),'mean_us':statistics.mean(values),'max_us':max(values)}
    rates={}
    for name,key in [('roi_processing_hz','roi_total'),('complete_round_hz','capture_round')]:
        if key in metrics and metrics[key]['mean_us']>0:rates[name]=1e6/metrics[key]['mean_us']
    return {'units':'us','frames':len(accepted),'excluded_units_frames':len(records)-len(accepted),'metrics':metrics,'rates':rates,
            'full_mode_lines_not_aggregated':len(skipped),'records':accepted,
            'scope':'Software-observed intervals. G/H/I overlap; no sum of IP times, no inferred camera/HDMI FPS. Missing CAPTURE means no complete-round rate.'}
def main():
    a=argparse.ArgumentParser();a.add_argument('log',type=Path);a.add_argument('--out',type=Path);n=a.parse_args()
    result=parse(n.log.read_text(encoding='utf-8-sig',errors='replace'));s=json.dumps(result,ensure_ascii=False,indent=2)+'\n'
    if n.out:n.out.parent.mkdir(parents=True,exist_ok=True);n.out.write_text(s,encoding='utf-8')
    print(s);return 0 if result['frames'] else 1
if __name__=='__main__':raise SystemExit(main())
