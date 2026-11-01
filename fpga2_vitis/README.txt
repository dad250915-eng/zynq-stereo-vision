FPGA2 three-mode standalone application, Vivado/Vitis 2023.2.
Entry point: display_demo.c. Hardware: Cortex-A9 #0 + matching triple-mode XSA.
Build: scripts/create_vitis_workspace.tcl; link libm and use lscript.ld.
Load rectification maps only after rectify_maps_wait_marker is reached.
ROI_ARCHITECTURE=1; mode 0 RANGE, 1 DEPTH, 2 FUSION.
PC Python controls the robot. No direct FPGA robot-control mode is claimed.
Read ../docs/reproduction.md for setup, addresses, matching artifacts and limits.
