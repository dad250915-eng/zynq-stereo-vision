# Vitis HLS 2023.2: vitis_hls -f scripts/build_hls.tcl -tclargs roi_h_sgm
set root [file normalize [file join [file dirname [info script]] ..]]
set names {roi_a_gray roi_b_target roi_c_contour roi_d_overlay roi_e_region roi_g_supply roi_h_sgm roi_i_measure roi_j_display roi_k_depth}
if {$argc != 1 || [lsearch -exact $names [lindex $argv 0]] < 0} {
  error "Specify one HLS IP: $names"
}
set name [lindex $argv 0]
set src [file join $root fpga2_bd design_1 ip $name src ${name}.cpp]
set work [file join $root build hls $name]
set flags "-std=c++14 -I[file join $root third_party vitis_vision include] -I[file join $root third_party sgm_vision]"
open_project $work
set_top $name
add_files $src -cflags $flags
open_solution solution -flow_target vivado
set_part xc7z035ffg900-2
create_clock -period 10 -name default
csynth_design
set version 1.0
if {$name eq "roi_k_depth"} {set version 1.1}
set exports [file join $root build hls_exports]
file mkdir $exports
export_design -format ip_catalog -rtl verilog -vendor stereo.local -library user -version $version -ipname $name -output [file join $exports ${name}.zip]
exit
