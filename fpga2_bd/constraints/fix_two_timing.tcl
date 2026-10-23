# 历史参考脚本，仅保留原文；避免误操作原工程，当前副本不执行这些命令。
# set root E:/fpga_projects/stereo_match_fpga2
# open_project $root/stereo_match_fpga2.xpr
# open_bd_design $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd
# set_property CONFIG.PRIM_SOURCE No_buffer [get_bd_cells hdmi_cfg_clk]
# set f $root/constraints/receiver_reset_sync.xdc
# if {![llength [get_files -quiet $f]]} {add_files -fileset constrs_1 -norecurse $f}
# set_property USED_IN_SYNTHESIS false [get_files $f]
# validate_bd_design -force
# save_bd_design
# generate_target all [get_files $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd]
# close_project
# # Recheck the existing routed netlist with the corrected clock relationship.
# open_checkpoint $root/stereo_match_fpga2.runs/impl_1/design_1_wrapper_routed.dcp
# set bad [get_clocks -quiet design_1_i/hdmi_cfg_clk/inst/clk_in1]
# if {[llength $bad] != 1} {error "Expected exactly one duplicate HDMI clock"}
# delete_clocks $bad
# set syncpins [get_pins -hierarchical -filter {NAME =~ *aurora_receive_reset_0/inst/rx_domain/release_sync_reg*/CLR || NAME =~ *aurora_receive_reset_0/inst/video_domain/release_sync_reg*/CLR}]
# puts "RESET_SYNC_PINS: [llength $syncpins] $syncpins"
# if {[llength $syncpins] != 4} {error "Expected four reset synchronizer CLR pins"}
# read_xdc $root/constraints/receiver_reset_sync.xdc
# report_timing_summary -file $root/constraints/timing_fix_recheck.rpt
# report_methodology -file $root/constraints/timing_fix_methodology.rpt
# close_design
