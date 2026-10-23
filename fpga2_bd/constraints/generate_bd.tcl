# 历史参考脚本，仅保留原文；避免误操作原工程，当前副本不执行这些命令。
# set root E:/fpga_projects/stereo_match_fpga2
# open_project $root/stereo_match_fpga2.xpr
# open_bd_design $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd
# puts "RESET_POLARITY: [get_property CONFIG.C_EXT_RESET_HIGH [get_bd_cells hdmi_cfg_reset]] AUX: [get_property CONFIG.C_AUX_RESET_HIGH [get_bd_cells hdmi_cfg_reset]]"
# if {[get_property CONFIG.C_EXT_RESET_HIGH [get_bd_cells hdmi_cfg_reset]] != 0} {error "HDMI reset polarity wrong"}
# generate_target all [get_files $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd]
# set wrappers [make_wrapper -files [get_files $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd] -top]
# foreach w $wrappers {if {![llength [get_files -quiet $w]]} {add_files -norecurse $w}}
# update_compile_order -fileset sources_1
# close_project
