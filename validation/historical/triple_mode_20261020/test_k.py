from pathlib import Path
import subprocess,os,ctypes,sys,json
w=Path(__file__).resolve().parent
dest=Path('E:/fpga_projects/stereo_match_fpga2/roi_architecture/triple_mode_20261020')
hls=Path('E:/Xilinx2023.2/Vitis_HLS/2023.2');mingw=hls/'tps/win64/msys64/mingw64'
clang=Path('E:/Xilinx2023.2/Vivado/2023.2/tps/llvm/7.0/win64/bin/clang++.exe')
env=os.environ.copy();env['PATH']=str(mingw/'bin')+';'+env['PATH']
cmd=[str(clang),'--target=x86_64-w64-windows-gnu','--sysroot='+str(mingw),'-isystem',str(mingw/'include/c++/6.2.0'),'-isystem',str(mingw/'include/c++/6.2.0/x86_64-w64-mingw32'),'-L'+str(mingw/'lib/gcc/x86_64-w64-mingw32/6.2.0'),'-fuse-ld=lld','-std=c++14','-O1','-Wno-unknown-pragmas','-DHLS_STREAM_THREAD_UNSAFE','-DDISABLE_MAX_HLS_STREAM_DEPTH_PRINT','-pthread','-static-libstdc++','-static-libgcc','-I'+str(hls/'include'),'-I'+str(dest/'src')]
objects=[]
with (dest/'native_compile.log').open('w') as log:
 for i,f in enumerate([w/'test_k.cpp',dest/'src/roi_k_depth.cpp']):
  obj=dest/('k_test_%d.o'%i);objects.append(str(obj));subprocess.run([*cmd,'-c',str(f),'-o',str(obj)],env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 subprocess.run([*cmd,*objects,'-shared','-lsupc++','-lwinpthread','-o',str(dest/'test_k.dll')],env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
os.add_dll_directory(str(mingw/'bin'))
sys.path.insert(0,str(w.parent/'roi_architecture'))
from patch_host_imports import patch_imports
patch_imports(dest/'test_k.dll')
rc=ctypes.CDLL(str(dest/'test_k.dll')).run_k_tests()
assert rc==0,rc
(dest/'k_validation.json').write_text(json.dumps({'K_native':'PASS','full_frame':'1280x720','guards':'PASS','depth_orientation':'vertical flip','mode_passthrough':'bit exact including sidebands','invalid_disparity':'black','protocol_faults':'reported','RTL_backpressure':'requires integration/board test'},indent=2))
