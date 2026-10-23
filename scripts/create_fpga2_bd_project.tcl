# Vivado 2023.2. Creates a NEW project under this submission/build only.
set root [file normalize [file join [file dirname [info script]] ..]]
set board fpga2_bd
set name stereo_match_fpga2
create_project $name [file join $root build $name] -part xc7z035ffg900-2
set_property ip_repo_paths [list [file join $root $board design_1 ip] [file join $root build ip_repo]] [current_project]
update_ip_catalog
add_files -norecurse [file join $root {fpga2_bd/rtl/rx_admission_ctrl.v}]
set constraint_path [file join $root {fpga2_bd/constraints/constrs_1/new/system.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED true $cf
set_property USED_IN_SYNTHESIS true $cf
set_property USED_IN_IMPLEMENTATION true $cf
set constraint_path [file join $root {fpga2_bd/constraints/constrs_1/new/an5642.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED false $cf
set_property USED_IN_SYNTHESIS true $cf
set_property USED_IN_IMPLEMENTATION true $cf
set constraint_path [file join $root {fpga2_bd/constraints/constrs_1/new/hdmi.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED false $cf
set_property USED_IN_SYNTHESIS true $cf
set_property USED_IN_IMPLEMENTATION true $cf
set constraint_path [file join $root {fpga2_bd/constraints/z100_sfpa.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED true $cf
set_property USED_IN_SYNTHESIS true $cf
set_property USED_IN_IMPLEMENTATION true $cf
set constraint_path [file join $root {fpga2_bd/constraints/z100_sfpa_control.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED true $cf
set_property USED_IN_SYNTHESIS true $cf
set_property USED_IN_IMPLEMENTATION true $cf
set constraint_path [file join $root {fpga2_bd/constraints/z100_hdmi_parallel.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED true $cf
set_property USED_IN_SYNTHESIS true $cf
set_property USED_IN_IMPLEMENTATION true $cf
set constraint_path [file join $root {fpga2_bd/constraints/receiver_reset_sync.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED true $cf
set_property USED_IN_SYNTHESIS false $cf
set_property USED_IN_IMPLEMENTATION true $cf
set constraint_path [file join $root {fpga2_bd/constraints/rx_admission_cdc.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED true $cf
set_property USED_IN_SYNTHESIS false $cf
set_property USED_IN_IMPLEMENTATION true $cf
set constraint_path [file join $root {fpga2_bd/roi_architecture/dual_mode_20261019/dual_mode_ui.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED true $cf
set_property USED_IN_SYNTHESIS true $cf
set_property USED_IN_IMPLEMENTATION true $cf
import_files -norecurse [file join $root $board design_1 design_1.bd]
set bd [get_files */design_1.bd]
open_bd_design $bd
validate_bd_design
generate_target all $bd
set wrappers [make_wrapper -files $bd -top]
add_files -norecurse $wrappers
update_compile_order -fileset sources_1
puts "Source project created. Synthesis, implementation and bitstream have NOT been launched."
