# Vivado 2023.2. Creates a NEW project under this submission/build only.
set root [file normalize [file join [file dirname [info script]] ..]]
set board fpga1_bd
set name stereo_match_fpga1
create_project $name [file join $root build $name] -part xc7a35tfgg484-2
set_property ip_repo_paths [list [file join $root $board design_1 ip] [file join $root build ip_repo]] [current_project]
update_ip_catalog
add_files -norecurse [file join $root {fpga1_bd/rtl/aurora_ready_rx.v}]
set constraint_path [file join $root {fpga1_bd/constraints/ax7a035b_an5642_pins.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED true $cf
set_property USED_IN_SYNTHESIS true $cf
set_property USED_IN_IMPLEMENTATION true $cf
set constraint_path [file join $root {fpga1_bd/constraints/ax7a035b_board_clock.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED true $cf
set_property USED_IN_SYNTHESIS true $cf
set_property USED_IN_IMPLEMENTATION true $cf
set constraint_path [file join $root {fpga1_bd/constraints/camera_timing_42mhz.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED true $cf
set_property USED_IN_SYNTHESIS true $cf
set_property USED_IN_IMPLEMENTATION true $cf
set constraint_path [file join $root {fpga1_bd/constraints/ax7a035b_sfp1.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED true $cf
set_property USED_IN_SYNTHESIS true $cf
set_property USED_IN_IMPLEMENTATION true $cf
set constraint_path [file join $root {fpga1_bd/constraints/reset_status_cdc.xdc}]
add_files -fileset constrs_1 -norecurse $constraint_path
set cf [get_files $constraint_path]
set_property IS_ENABLED true $cf
set_property USED_IN_SYNTHESIS false $cf
set_property USED_IN_IMPLEMENTATION true $cf
set_property PROCESSING_ORDER LATE $cf
import_files -norecurse [file join $root $board design_1 design_1.bd]
set bd [get_files */design_1.bd]
open_bd_design $bd
validate_bd_design
generate_target all $bd
set wrappers [make_wrapper -files $bd -top]
add_files -norecurse $wrappers
update_compile_order -fileset sources_1
puts "Source project created. Synthesis, implementation and bitstream have NOT been launched."
