from pathlib import Path
import subprocess,sys,json
w=Path(__file__).resolve().parent
d=Path('E:/fpga_projects/stereo_match_fpga2/roi_architecture/triple_mode_20261020')
sim=d/'ui_sim';sim.mkdir(exist_ok=True)
runner=w.parent/'roi_architecture/run_tool.py'
with (d/'ui_rtl_test.log').open('w') as log:
 for args in [['xvlog','-sv',str(w/'test_ui.sv'),str(d/'src/roi_mode_button.v')],['xelab','test_ui','-s','ui_test_sim'],['xsim','ui_test_sim','-runall']]:
  subprocess.run([sys.executable,str(runner),*args],cwd=sim,stdout=log,stderr=subprocess.STDOUT,check=True)
text=(d/'ui_rtl_test.log').read_text(errors='replace');print(text[-1600:]);assert 'PASS UI RTL:' in text
(d/'ui_validation.json').write_text(json.dumps({'RTL':'PASS','debounce_cycles_production':2000000,'clock_Hz':100000000,'debounce_ms':20,'test_counter_scaled':8,'key':'PL_KEY0/SW4/J13 active low LVCMOS18','LED':'PL_LED0/LED4/U21 active high LVCMOS33'},indent=2))
