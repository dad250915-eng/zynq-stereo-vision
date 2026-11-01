# Run in Vivado 2023.2. Builds only the explicitly supplied copied project.
if {$argc < 2 || $argc > 3} {error "Usage: build_and_export.tcl project.xpr fpga1|fpga2 ?jobs?"}
set xpr [file normalize [lindex $argv 0]]
set board [lindex $argv 1]
if {$board ni {fpga1 fpga2}} {error "board must be fpga1 or fpga2"}
set jobs 4
if {$argc==3} {set jobs [lindex $argv 2]}
if {![string is integer -strict $jobs] || $jobs < 1} {error "jobs must be positive"}
if {![file isfile $xpr]} {error "Missing copied project: $xpr"}
set root [file normalize [file join [file dirname [info script]] ..]]
set build [file normalize [file join $root build]]
set rel [string range $xpr [string length $build] end]
if {![string equal -nocase [string range $xpr 0 [expr {[string length $build]-1}]] $build] || ![string match {/*} $rel]} {
  error "Use a newly created project below submission/build, not an original working project"
}
set out [file join $build release $board]
file mkdir $out
open_project $xpr
if {![string match {*Complete*} [get_property STATUS [get_runs synth_1]]]} {
  launch_runs synth_1 -jobs $jobs
  wait_on_run synth_1
}
if {![string match {*Complete*} [get_property STATUS [get_runs synth_1]]]} {error "Synthesis incomplete"}
if {![string match {write_bitstream Complete*} [get_property STATUS [get_runs impl_1]]]} {
  launch_runs impl_1 -to_step write_bitstream -jobs $jobs
  wait_on_run impl_1
}
set status [get_property STATUS [get_runs impl_1]]
if {![string match {write_bitstream Complete*} $status]} {error "Implementation incomplete: $status"}
if {[get_property NEEDS_REFRESH [get_runs impl_1]]} {error "Implementation stale; no XSA export"}
open_run impl_1
report_utilization -file [file join $out utilization.rpt]
report_timing_summary -report_unconstrained -file [file join $out timing_summary.rpt]
report_drc -file [file join $out drc.rpt]
set paths [get_timing_paths -delay_type max -max_paths 1]
if {![llength $paths]} {error "No setup timing path found; inspect clocks/constraints"}
set wns [get_property SLACK [lindex $paths 0]]
set f [open [file join $out build_status.txt] w]
puts $f "status=$status\nsetup_wns_ns=$wns\nSee timing_summary.rpt for hold and unconstrained paths."
close $f
if {$wns < 0} {error "Setup timing not met ($wns ns). Reports saved; no XSA export."}
if {[llength [get_drc_violations -filter {SEVERITY == Error}]]} {error "DRC errors; no XSA export"}
if {$board eq "fpga2"} {write_hw_platform -fixed -include_bit -force -file [file join $out stereo_fpga2.xsa]}
puts "BUILD_REPORTS_WRITTEN: $out; audit hold/unconstrained paths before claiming timing closure."
close_project
