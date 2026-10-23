# 历史参考脚本，仅保留原文；避免误操作原工程，当前副本不执行这些命令。
# set root E:/fpga_projects/stereo_match_fpga2
# open_project $root/stereo_match_fpga2.xpr
# open_bd_design $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd
# proc wire {src dst} {
#  set s [get_bd_pins $src]; set d [get_bd_pins $dst]
#  set old [get_bd_nets -quiet -of_objects $d]
#  if {[llength $old]} {disconnect_bd_net $old $d}
#  connect_bd_net $s $d
# }
# create_bd_cell -type ip -vlnv xilinx.com:ip:clk_wiz:6.0 hdmi_cfg_clk
# set_property -dict [list CONFIG.PRIM_IN_FREQ {100.000} CONFIG.CLKOUT1_REQUESTED_OUT_FREQ {10.000} CONFIG.RESET_TYPE {ACTIVE_LOW}] [get_bd_cells hdmi_cfg_clk]
# create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 hdmi_cfg_reset
# set_property -dict [list CONFIG.C_EXT_RESET_HIGH {0} CONFIG.C_AUX_RESET_HIGH {1}] [get_bd_cells hdmi_cfg_reset]
# wire processing_system7_0/FCLK_CLK0 hdmi_cfg_clk/clk_in1
# wire proc_sys_reset_1/peripheral_aresetn hdmi_cfg_clk/resetn
# wire hdmi_cfg_clk/clk_out1 hdmi_cfg_reset/slowest_sync_clk
# wire hdmi_cfg_clk/locked hdmi_cfg_reset/dcm_locked
# wire proc_sys_reset_1/peripheral_aresetn hdmi_cfg_reset/ext_reset_in
# wire xlconstant_13/dout hdmi_cfg_reset/aux_reset_in
# wire xlconstant_13/dout hdmi_cfg_reset/mb_debug_sys_rst
# wire hdmi_cfg_clk/clk_out1 DVI_Transmitter_0/cfg_clk
# wire hdmi_cfg_reset/peripheral_aresetn DVI_Transmitter_0/rst_n
# wire axi_dynclk_0/PXL_CLK_O DVI_Transmitter_0/pixel_clk
# make_bd_intf_pins_external [get_bd_intf_pins DVI_Transmitter_0/Video_Out]
# set_property name Video_Out [get_bd_intf_ports Video_Out_0]
# foreach pin {video_clk iic_scl iic_sda rstn_out} {
#  make_bd_pins_external [get_bd_pins DVI_Transmitter_0/$pin]
#  set_property name $pin [get_bd_ports ${pin}_0]
# }
# foreach i {0 1} {
#  wire aurora_receive_reset_0/video_axis_aresetn axis_subset_converter_$i/aresetn
#  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES {4} CONFIG.M_TDATA_NUM_BYTES {2} CONFIG.HAS_TLAST {1} CONFIG.HAS_TKEEP {1} CONFIG.TUSER_BITS_PER_BYTE {1}] [get_bd_cells axis_dwidth_converter_$i]
# }
# set_property CONFIG.FREQ_HZ 156250000 [get_bd_intf_ports GT_DIFF_REFCLK1_0]
# set_property -dict [list CONFIG.TDATA_NUM_BYTES {4} CONFIG.HAS_TLAST {1} CONFIG.IS_ACLK_ASYNC {1}] [get_bd_cells axis_clock_converter_3]
# set_property IS_ENABLED true [get_files $root/constraints/z100_hdmi_parallel.xdc]
# set_property IS_ENABLED true [get_files $root/constraints/z100_sfpa_control.xdc]
# save_bd_design
# if {[catch {validate_bd_design -force} msg]} {puts "VALIDATION_FAILED: $msg"} else {puts "VALIDATION_PASSED"}
# save_bd_design
# close_project
