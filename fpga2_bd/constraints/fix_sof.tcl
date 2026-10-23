# 历史参考脚本，仅保留原文；避免误操作原工程，当前副本不执行这些命令。
# set root E:/fpga_projects/stereo_match_fpga2
# open_project $root/stereo_match_fpga2.xpr
# open_bd_design $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd
# set_property CONFIG.CONST_VAL 0 [get_bd_cells xlconstant_14]
# foreach i {0 1} {
#  set p [get_bd_intf_pins axis_dwidth_converter_$i/S_AXIS]
#  disconnect_bd_intf_net [get_bd_intf_nets -of_objects $p] $p
#  create_bd_cell -type ip -vlnv xilinx.com:ip:axis_subset_converter:1.1 rx_sof_expand_$i
#  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES {4} CONFIG.M_TDATA_NUM_BYTES {4} CONFIG.S_TUSER_WIDTH {1} CONFIG.M_TUSER_WIDTH {4} CONFIG.S_HAS_TLAST {1} CONFIG.M_HAS_TLAST {1} CONFIG.TUSER_REMAP {3'b000,tuser[0]}] [get_bd_cells rx_sof_expand_$i]
#  connect_bd_intf_net [get_bd_intf_pins axis_frame_decoder_0/m_axis_$i] [get_bd_intf_pins rx_sof_expand_$i/S_AXIS]
#  connect_bd_intf_net [get_bd_intf_pins rx_sof_expand_$i/M_AXIS] [get_bd_intf_pins axis_dwidth_converter_$i/S_AXIS]
#  connect_bd_net [get_bd_pins processing_system7_0/FCLK_CLK1] [get_bd_pins rx_sof_expand_$i/aclk]
#  connect_bd_net [get_bd_pins aurora_receive_reset_0/video_axis_aresetn] [get_bd_pins rx_sof_expand_$i/aresetn]
#  set_property CONFIG.M_TUSER_WIDTH 1 [get_bd_cells axis_subset_converter_$i]
# }
# save_bd_design
# validate_bd_design -force
# save_bd_design
# generate_target all [get_files $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd]
# make_wrapper -files [get_files $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd] -top
# close_project
