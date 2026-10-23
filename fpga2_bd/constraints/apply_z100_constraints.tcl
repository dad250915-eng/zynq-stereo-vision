# 历史参考脚本，仅保留原文；避免误操作原工程，当前副本不执行这些命令。
# set root E:/fpga_projects/stereo_match_fpga2
# open_project $root/stereo_match_fpga2.xpr
# foreach name {an5642.xdc hdmi.xdc} {
#     set f [get_files -quiet */$name]
#     if {[llength $f]} {set_property IS_ENABLED false $f}
# }
# foreach name {z100_sfpa.xdc z100_sfpa_control.xdc z100_hdmi_parallel.xdc} {
#     set path $root/constraints/$name
#     if {![llength [get_files -quiet $path]]} {add_files -fileset constrs_1 -norecurse $path}
# }
# set_property IS_ENABLED true [get_files $root/constraints/z100_sfpa.xdc]
# set_property IS_ENABLED false [get_files $root/constraints/z100_sfpa_control.xdc]
# set_property IS_ENABLED false [get_files $root/constraints/z100_hdmi_parallel.xdc]
# foreach f [get_files -of_objects [get_filesets constrs_1]] {
#     puts "CONSTRAINT_STATUS: [get_property IS_ENABLED $f] $f"
# }
# close_project
