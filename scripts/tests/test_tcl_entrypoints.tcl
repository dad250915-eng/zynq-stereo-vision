# Tcl-only control-flow tests. Xilinx commands below are stubs, never device I/O.
set root [file normalize [file join [file dirname [info script]] ../..]]
set sandbox [file join $root build tool_tests tcl_case]
if {![file isdirectory $sandbox]} {error "Generate tcl_case fixture first; see test instructions"}
set argv0 mock_harness
set events {}
proc assert {condition message} {if {![uplevel 1 [list expr $condition]]} {error $message}}
proc event {name args} {lappend ::events [linsert $args 0 $name]}
foreach p [glob -directory [file join $root scripts] *.tcl] {
  set f [open $p r];fconfigure $f -encoding utf-8;set code [read $f];close $f
  assert {[info complete $code]} "Incomplete Tcl structure: $p"
}
puts "PASS Tcl lexical structure (not Xilinx API semantics)"

set stale 0
set slack 0.5
proc get_runs {name} {return $name}
proc get_property {prop object} {
  switch -- $prop {
    STATUS {if {$object eq "synth_1"} {return {synth_design Complete!}};return {write_bitstream Complete!}}
    NEEDS_REFRESH {return $::stale}
    SLACK {return $::slack}
    default {error "Unexpected property $prop"}
  }
}
proc get_timing_paths {args} {return p0}
proc get_drc_violations {args} {return {}}
foreach cmd {open_project open_run close_project report_utilization report_timing_summary report_drc write_hw_platform launch_runs wait_on_run} {
  proc $cmd {args} [format {event %s {*}$args} $cmd]
}
set xpr [file join $sandbox fixture.xpr]
set f [open $xpr w];puts $f fixture;close $f
set argc 3;set argv [list $xpr fpga2 2]
set stale 1
set events {}
assert {[catch {source [file join $root scripts build_and_export.tcl]} message]} "Stale build should stop"
assert {[string match {*stale*} $message]} "Unexpected stale-build failure: $message"
assert {![string match {*write_hw_platform*} $events]} "Stale build exported XSA"
set stale 0;set slack -0.028;set events {}
assert {[catch {source [file join $root scripts build_and_export.tcl]} message]} "Negative setup slack should stop"
assert {![string match {*write_hw_platform*} $events]} "Negative setup slack exported XSA"
set slack 0.5;set events {}
source [file join $root scripts build_and_export.tcl]
assert {[string match {*write_hw_platform*} $events]} "Expected mock export missing"
puts "PASS build stale/negative-setup guards and mock positive branch"

foreach cmd {setws platform domain bsp app importsources} {
  proc $cmd {args} [format {event %s {*}$args} $cmd]
}
set ws [file join $sandbox vitis_workspace]
set hw [file join $sandbox fixture.xsa]
set f [open $hw w];puts $f fixture;close $f
set argc 2;set argv [list $hw $ws]
source [file join $root scripts create_vitis_workspace.tcl]
assert {[file isfile [file join $ws app_sources display_demo.c]]} "Missing staged application"
assert {![file exists [file join $ws app_sources platform_init]]} "PS initialization imported as application source"
assert {[catch {source [file join $root scripts create_vitis_workspace.tcl]} message]} "Existing workspace should not be overwritten"
puts "PASS Vitis source staging and existing-workspace guard (mock commands)"

set events {}
source [file join $root scripts run_fpga2.tcl]
assert {[llength $events]==0} "Sourcing downloader unexpectedly accessed a device"
source [file join $sandbox config.tcl]
set pc_bad 0
set map_bad 0
proc connect {args} {event connect {*}$args}
proc targets {args} {
  event targets {*}$args
  if {[lsearch -exact $args -target-properties]>=0} {return [list [dict create id 1 name {ARM Cortex-A9 MPCore #0}]]}
  return 1
}
foreach cmd {stop rst fpga ps7_init ps7_post_config bpremove dow con} {
  proc $cmd {args} [format {event %s {*}$args} $cmd]
}
proc bpadd {args} {event bpadd {*}$args;return bp0}
proc rrd {args} {
  if {$::pc_bad} {return [dict create pc 00000000]}
  return [dict create pc [format %08X [dict get $::board_config marker]]]
}
proc mrd {args} {
  set path [lindex $args [expr {[lsearch -exact $args -file]+1}]]
  set addr [lindex $args end-1];set count [lindex $args end]
  set b $::board_config
  if {$count==921600} {
    foreach lane {left right} {
      if {$addr==[dict get $b ${lane}_addr]} {file copy -force [dict get $b ${lane}_map] $path;return}
    }
    error "Unexpected block read"
  }
  set value 0xffffffff
  foreach lane {left right} {
    if {$addr==[dict get $b ${lane}_ptr_symbol]} {set value [dict get $b ${lane}_addr]}
    if {$::map_bad && $addr==[dict get $b ${lane}_addr]+4*(360*1280+640)} {set value 0}
  }
  set f [open $path wb];puts -nonewline $f [binary format i $value];close $f
}
set events {};set pc_bad 1
assert {[catch {run_fpga2 [file join $sandbox config.tcl]} message]} "Wrong PC should stop"
assert {![string match {*dow -data*} $events]} "Wrong PC downloaded maps"
set events {};set pc_bad 0;set map_bad 1
assert {[catch {run_fpga2 [file join $sandbox config.tcl]} message]} "Bad initialization should stop"
assert {![string match {*dow -data*} $events]} "Bad initialization downloaded maps"
set events {};set map_bad 0
run_fpga2 [file join $sandbox config.tcl]
assert {[string match {*dow -data*} $events]} "Mock map download absent"
assert {[lindex [lindex $events end] 0] eq "con"} "Mock execution did not resume"
puts "PASS downloader wrong-PC/sentinel guards and mock readback branch"
puts "ALL TCL MOCK CHECKS PASS; no Vivado, XSCT, JTAG or robot operation executed."
