from pathlib import Path
from zipfile import ZipFile
root=Path(__file__).resolve().parent.parent
for archive in (root/'build/hls_exports').glob('*.zip'):
    target=root/'build/ip_repo'/archive.stem
    target.mkdir(parents=True,exist_ok=True)
    with ZipFile(archive) as z:
        for n in z.namelist():
            if not (target/n).resolve().is_relative_to(target.resolve()):raise ValueError('Unsafe zip member')
        z.extractall(target)
    print(archive.name,'->',target)
