from pathlib import Path
import ctypes as C,subprocess,json
w=Path(__file__).resolve().parent
dest=Path('E:/fpga_projects/stereo_match_fpga2/roi_architecture/triple_mode_20261020')
src=Path('E:/vitis_stereo_match_fpga2/stereo_match_fpga2_application/src')
old=(w.parent/'d_parallel_20261015/test_schedule.py').read_text()
prefix=old.split("prefix=r'''",1)[1].split("'''",1)[0]
prefix=prefix.replace('typedef unsigned int u32;', 'typedef unsigned int u32;typedef unsigned int uint32_t;typedef int int32_t;typedef unsigned short u16;')
for n in ['a_gray','b_target','c_contour','d_overlay','e_region','g_supply','h_sgm','i_measure','j_display','k_depth']:
 prefix+='#include "xroi_'+n+'_hw.h"\n'
mock=old.split("prefix+=r'''",1)[1].split("'''",1)[0]
mock=mock.replace('registers[9][256]','registers[10][256]').replace('armed[9],sticky[9]','armed[10],sticky[10]').replace('finish[9],durations[9],started[9]','finish[10],durations[10],started[10]').replace('i<9;i++','i<10;i++').replace('j<9;j++','j<10;j++')
mock=mock.replace('static unsigned index_of', 'static unsigned mode_pin,led_value;static int valid_xyz=1,xyz_z=500000;static unsigned starts[10],sequence,order[10];\nstatic u16 palette[256];\n#undef ROI_DEPTH_PALETTE\n#define ROI_DEPTH_PALETTE ((UINTPTR)palette)\nstatic unsigned index_of')
mock=mock.replace('rb[35]=frame;','rb[35]=frame;rb[15]=rb[37]=valid_xyz;rb[33]=1;rb[50]=(u32)xyz_z;rb[59]=1;rb[60]=1000;')
mock=mock.replace('unsigned i=index_of(address&0xffff0000U),offset=address&0xffffU;', 'if((address&0xffff0000U)==ROI_UI_GPIO_BASE)return (address&0xffffU)==0?mode_pin:0;\n unsigned i=index_of(address&0xffff0000U),offset=address&0xffffU;',1)
# Output mock needs a different GPIO path.
start=mock.index('static void Xil_Out32')
mock=mock[:start]+mock[start:].replace('unsigned i=index_of(address&0xffff0000U),offset=address&0xffffU;', 'if((address&0xffff0000U)==ROI_UI_GPIO_BASE){if((address&0xffffU)==8)led_value=value;return;}\n unsigned i=index_of(address&0xffff0000U),offset=address&0xffffU;',1)
mock=mock.replace('armed[i]=1;sticky[i]=0;', 'starts[i]++;order[i]=++sequence;\n  if(i==5 && (!armed[6]||!armed[9]||(!registers[9][XROI_K_DEPTH_CONTROL_ADDR_MODE_DATA/4]&&!armed[7])))errors|=16;\n  armed[i]=1;sticky[i]=0;')
helpers=(dest/'src/roi_pipeline_capture.h').read_text()
helpers=helpers[helpers.index('static u32 roi_read('):helpers.index('static int roi_preflight(')]
capture=(dest/'src/roi_pipeline_capture.h').read_text()
body=capture[capture.index('static int roi_capture_once('):]
code=prefix+mock+'\n#undef ROI_TIMEOUT_US\n#define ROI_TIMEOUT_US 500000U\n'+helpers+body+r'''
#define CHECK(v) do{if(!(v))return __LINE__;}while(0)
__declspec(dllexport) int run_schedule_tests(void){
 const u8 *left=(const u8*)0x2000000,*right=(const u8*)0x3000000;
 durations[0]=11000;durations[1]=63000;durations[2]=9300;durations[3]=11000;
 durations[4]=15;durations[5]=36000;durations[6]=36300;durations[7]=36400;durations[8]=10700;durations[9]=36500;
 roi_ui_init();CHECK(!led_value);CHECK(!palette[0]&&!palette[255]);CHECK(palette[254]!=palette[30]);
 CHECK(roi_dual_capture_once(left,right)==0);CHECK(!errors);CHECK(led_value==1);
 CHECK(order[7]<order[9]&&order[9]<order[6]&&order[6]<order[5]);CHECK(started[5]<finish[3]);
 xyz_z=500001;CHECK(roi_dual_capture_once(left,right)==0);CHECK(!led_value);
 xyz_z=499999;CHECK(roi_dual_capture_once(left,right)==0);CHECK(led_value);
 valid_xyz=0;CHECK(roi_dual_capture_once(left,right)==0);CHECK(!led_value);
 valid_xyz=1;xyz_z=-1;CHECK(roi_dual_capture_once(left,right)==0);CHECK(!led_value);
 xyz_z=400000;empty_target=1;CHECK(roi_dual_capture_once(left,right)==0);CHECK(!led_value);empty_target=0;
 unsigned bi=starts[1],ii=starts[7],ji=starts[8],ki=starts[9];
 mode_pin=1;
 CHECK(roi_dual_capture_once(left,right)==0);CHECK(roi_display_mode==1);CHECK(!led_value);
 CHECK(starts[1]==bi&&starts[7]==ii&&starts[8]==ji&&starts[9]==ki+1);
 CHECK(registers[9][XROI_K_DEPTH_CONTROL_ADDR_MODE_DATA/4]==1);
 CHECK(registers[6][XROI_H_SGM_CONTROL_ADDR_ROWS_DATA/4]==720&&registers[6][XROI_H_SGM_CONTROL_ADDR_COLS_DATA/4]==1280);
 CHECK(registers[5][XROI_G_SUPPLY_CONTROL_ADDR_X0_DATA/4]==0&&registers[5][XROI_G_SUPPLY_CONTROL_ADDR_Y0_DATA/4]==0);
 CHECK(roi_dual_capture_once(left,right)==0);CHECK(!errors);
 mode_pin=2;CHECK(roi_dual_capture_once(left,right)==0);CHECK(roi_display_mode==2&&!led_value);
 CHECK(starts[1]==bi&&starts[7]==ii&&starts[8]==ji&&starts[9]==ki+3);
 CHECK(registers[9][XROI_K_DEPTH_CONTROL_ADDR_MODE_DATA/4]==2);
 CHECK(registers[9][XROI_K_DEPTH_CONTROL_ADDR_IMAGE_R_DATA/4]==(u32)(UINTPTR)right);
 CHECK(registers[9][XROI_K_DEPTH_CONTROL_ADDR_COLOR_STRIDE_WORDS_DATA/4]==1920);
 CHECK(registers[9][XROI_K_DEPTH_CONTROL_ADDR_BLEND_EIGHTHS_DATA/4]==1);
 CHECK(roi_dual_capture_once(left,right)==0);CHECK(!errors);
 mode_pin=3;CHECK(roi_dual_capture_once(left,right)==1);CHECK(!led_value);
 mode_pin=0;CHECK(roi_dual_capture_once(left,right)==0);CHECK(roi_display_mode==0&&led_value);
 CHECK(starts[7]==ii+1&&registers[9][XROI_K_DEPTH_CONTROL_ADDR_MODE_DATA/4]==0);
 // Protocol errors never produce a new displayed frame; timeout clears LED.
 fail_d=1;XTime previous_j=started[8];
 CHECK(roi_dual_capture_once(left,right)==1);CHECK(!led_value&&started[8]==previous_j);CHECK(!errors);
 return 0;
}
'''
(w/'schedule.c').write_text(code)
clang=Path('E:/Xilinx2023.2/Vivado/2023.2/tps/llvm/7.0/win64/bin/clang.exe')
subprocess.run([str(clang),'--target=x86_64-w64-windows-gnu','-ffreestanding','-O1','-Wno-int-conversion','-I'+str(dest/'src'),'-I'+str(src),'-c',str(w/'schedule.c'),'-o',str(w/'schedule.o')],check=True)
subprocess.run([str(clang.with_name('ld.lld.exe')),'-flavor','link','/dll','/noentry','/nodefaultlib',str(w/'schedule.o'),'/out:'+str(w/'schedule.dll')],check=True)
result=C.CDLL(str(w/'schedule.dll')).run_schedule_tests()
assert result==0,'Assertion at generated C line '+str(result)
report={'host_schedule':'PASS','cases':['consumer-first I/K/H/G launch','D and GHI overlap retained','Z=500000 on,500001 off,499999 on','invalid/negative/empty target off','range-depth-depth-fusion-fusion-invalid-range without stale active IP','depth skips B/C/D/E/I/J','full dimensions and zero crop','processing timeout clears LED and retains display'],'model_not_board':True}
(dest/'schedule_validation.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
