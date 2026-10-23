# 历史参考脚本，仅保留原文；避免误操作原工程，当前副本不执行这些命令。
# set root E:/fpga_projects/stereo_match_fpga2
# open_project $root/stereo_match_fpga2.xpr
# config_ip_cache -disable_cache
# open_bd_design $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd
# set p [get_bd_intf_ports -quiet GPIO_0]
# if {[llength $p]} {delete_bd_objs $p}
# validate_bd_design -force
# save_bd_design
# generate_target all [get_files $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd]
# make_wrapper -files [get_files $root/stereo_match_fpga2.srcs/sources_1/bd/design_1/design_1.bd] -top
# update_compile_order -fileset sources_1
# launch_runs synth_1 -jobs 2
# wait_on_run synth_1
# puts "SYNTH_STATUS: [get_property STATUS [get_runs synth_1]]"
# if {[get_property PROGRESS [get_runs synth_1]] ne "100%"} {error "Synthesis incomplete"}
# launch_runs impl_1 -to_step route_design -jobs 2
# wait_on_run impl_1
# puts "IMPL_STATUS: [get_property STATUS [get_runs impl_1]]"
# open_run impl_1
# report_timing_summary -file $root/constraints/routed_timing.rpt
# report_drc -file $root/constraints/routed_drc.rpt
# report_io -file $root/constraints/routed_io.rpt
# close_project
