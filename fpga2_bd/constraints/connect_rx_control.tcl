# 历史参考脚本，仅保留原文；避免误操作原工程，当前副本不执行这些命令。
# set root E:/fpga_projects/stereo_match_fpga2
# open_project $root/stereo_match_fpga2.xpr
# add_files -norecurse $root/rtl/rx_admission_ctrl.v
# add_files -fileset constrs_1 -norecurse $root/constraints/rx_admission_cdc.xdc
# set_property USED_IN_SYNTHESIS false [get_files $root/constraints/rx_admission_cdc.xdc]
# update_compile_order -fileset sources_1
# open_bd_design $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd
# create_bd_cell -type module -reference rx_admission_ctrl rx_admission_ctrl_0
# set_property -dict [list CONFIG.C_GPIO_WIDTH {1} CONFIG.C_ALL_OUTPUTS {1} CONFIG.C_DOUT_DEFAULT {0x00000000} CONFIG.C_IS_DUAL {1} CONFIG.C_GPIO2_WIDTH {32} CONFIG.C_ALL_INPUTS_2 {1}] [get_bd_cells axi_gpio_0]
# proc wire {src dst} {
#  set d [get_bd_pins $dst]
#  set n [get_bd_nets -quiet -of_objects $d]
#  if {[llength $n]} {disconnect_bd_net $n $d}
#  connect_bd_net [get_bd_pins $src] $d
# }
# wire processing_system7_0/FCLK_CLK0 rx_admission_ctrl_0/ctrl_clk
# wire aurora_8b10b_0/user_clk_out rx_admission_ctrl_0/rx_clk
# wire proc_sys_reset_1/peripheral_aresetn rx_admission_ctrl_0/rst_n
# wire axi_gpio_0/gpio_io_o rx_admission_ctrl_0/sw_enable
# wire rx_admission_ctrl_0/status axi_gpio_0/gpio2_io_i
# wire aurora_8b10b_0/channel_up rx_admission_ctrl_0/channel_up
# wire aurora_8b10b_0/sys_reset_out rx_admission_ctrl_0/sys_reset_out
# wire aurora_receive_reset_0/rx_axis_aresetn rx_admission_ctrl_0/rx_aresetn
# wire rx_admission_ctrl_0/clear_req aurora_receive_reset_0/clear_req
# # Replace only this AXIS interface with explicit wires to intercept TVALID.
# set n [get_bd_intf_nets -of_objects [get_bd_intf_pins axis_clock_converter_3/S_AXIS]]
# delete_bd_objs $n
# wire aurora_8b10b_0/m_axi_rx_tdata axis_clock_converter_3/s_axis_tdata
# wire aurora_8b10b_0/m_axi_rx_tkeep axis_clock_converter_3/s_axis_tkeep
# wire aurora_8b10b_0/m_axi_rx_tlast axis_clock_converter_3/s_axis_tlast
# wire aurora_8b10b_0/m_axi_rx_tvalid rx_admission_ctrl_0/rx_valid
# wire axis_clock_converter_3/s_axis_tready rx_admission_ctrl_0/fifo_ready
# wire rx_admission_ctrl_0/admitted_valid axis_clock_converter_3/s_axis_tvalid
# validate_bd_design -force
# save_bd_design
# generate_target all [get_files $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd]
# make_wrapper -files [get_files $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd] -top
# write_hw_platform -fixed -force -file $root/stereo_match_fpga2_control.xsa
# close_project
