"""Refresh only after reviewing changes; hashes are not functional proof."""
from pathlib import Path
import argparse,json
from submission_common import inventory
def main():
    a=argparse.ArgumentParser();a.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);n=a.parse_args()
    root=n.root.resolve()
    data={'schema':3,'hash_normalization':'Git text=auto LF canonical bytes; binary unchanged',
          'exclusions':['SOURCE_MANIFEST.json','build/','.git/','.venv/','venv/','__pycache__/','*.pyc','.Xil/','logs/','*.log','*.log.*','*.jou','*.dcp','*.str','*.wdb','.env*','validation/local/','board/board_config.tcl'],
          'note':'Source release inventory, excluding ignored runtime/build artifacts. Text SHA256 and byte sizes use LF canonical data. Not proof of a clean tool build.','files':inventory(root)}
    (root/'SOURCE_MANIFEST.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f'Manifest refreshed: {len(data["files"])} files')
if __name__=='__main__':main()
