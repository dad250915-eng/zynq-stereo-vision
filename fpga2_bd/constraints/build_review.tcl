# 历史参考脚本，仅保留原文；避免误操作原工程，当前副本不执行这些命令。
# set root E:/fpga_projects/stereo_match_fpga2
# open_project $root/stereo_match_fpga2.xpr
# foreach r [get_runs] {
#  if {[lsearch -exact [list_property $r] INCREMENTAL_CHECKPOINT] >= 0} {set_property INCREMENTAL_CHECKPOINT {} $r}
#  if {[lsearch -exact [list_property $r] AUTO_INCREMENTAL_CHECKPOINT] >= 0} {set_property AUTO_INCREMENTAL_CHECKPOINT 0 $r}
# }
# update_compile_order -fileset sources_1
# report_compile_order -file $root/constraints/compile_order.rpt
# reset_run synth_1
# launch_runs synth_1 -jobs 4
# wait_on_run synth_1
# puts "SYNTH_STATUS: [get_property STATUS [get_runs synth_1]]"
# if {[get_property PROGRESS [get_runs synth_1]] ne "100%"} {error "Synthesis did not complete"}
# open_run synth_1
# report_drc -file $root/constraints/post_synth_drc.rpt
# report_io -file $root/constraints/post_synth_io.rpt
# close_design
# launch_runs impl_1 -to_step route_design -jobs 4
# wait_on_run impl_1
# puts "IMPL_STATUS: [get_property STATUS [get_runs impl_1]]"
# if {[get_property PROGRESS [get_runs impl_1]] ne "100%"} {error "Implementation did not complete"}
# open_run impl_1
# report_timing_summary -file $root/constraints/routed_timing.rpt
# report_drc -file $root/constraints/routed_drc.rpt
# close_project
# 
