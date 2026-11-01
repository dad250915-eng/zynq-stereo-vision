# Explicit download only: xsct scripts/run_fpga2.tcl board/board_config.tcl
# Does not contain robot commands. Does not run when merely sourced without run_fpga2.
proc board_word {addr scratch} {
  mrd -force -size w -bin -file $scratch [format 0x%08X $addr] 1
  set f [open $scratch rb];set b [read $f];close $f
  if {[string length $b]!=4} {error "Incomplete DDR read"}
  binary scan $b cu* q
  expr {[lindex $q 0] | ([lindex $q 1]<<8) | ([lindex $q 2]<<16) | ([lindex $q 3]<<24)}
}
proc board_hash {python path} {
  string trim [exec $python -c {import hashlib,sys;print(hashlib.sha256(open(sys.argv[1],'rb').read()).hexdigest())} $path]
}
proc board_target {filter} {
  set ts [targets -target-properties -filter $filter]
  if {[llength $ts]!=1} {error "Expected exactly one FPGA2 A9 target; inspect targets and target_filter"}
  targets -set -filter $filter
}
proc run_fpga2 {config_path} {
  source $config_path
  foreach k {elf bit init left_map right_map} {
    set path [dict get $board_config $k]
    if {![file isfile $path] || [board_hash [dict get $board_config python] $path] ne [dict get $board_config ${k}_sha256]} {error "Artifact missing/changed: $k"}
  }
  set filter [dict get $board_config target_filter]
  connect -url tcp:127.0.0.1:3121
  board_target $filter
  if {[catch {stop} msg] && ![string equal -nocase [string trim $msg] "Already stopped"]} {error $msg}
  rst -system
  after 1000
  fpga -file [dict get $board_config bit]
  board_target $filter
  source [dict get $board_config init]
  ps7_init
  ps7_post_config
  rst -processor
  dow [dict get $board_config elf]
  set marker [dict get $board_config marker]
  set bp [bpadd -addr [format 0x%08X $marker] -type hw]
  set code [catch {con -block -timeout 60} msg]
  catch {stop};catch {bpremove $bp}
  if {$code} {error "Map-wait breakpoint failed: $msg; CPU left stopped"}
  set pc_text [dict get [rrd -nvlist pc] pc]
  if {[scan $pc_text %x pc]!=1 || $pc!=$marker} {error "Unexpected PC; no map download"}
  set scratch [file join [file dirname [file normalize $config_path]] map_probe.bin]
  foreach lane {left right} {
    set addr [dict get $board_config ${lane}_addr]
    set ptr [board_word [dict get $board_config ${lane}_ptr_symbol] $scratch]
    if {$ptr!=$addr} {error "Map pointer/storage contract mismatch; CPU left stopped"}
    if {[board_word [expr {$addr+4*(360*1280+640)}] $scratch]!=0xffffffff} {error "Map initialization not confirmed; CPU left stopped"}
  }
  foreach lane {left right} {
    set addr [dict get $board_config ${lane}_addr]
    set path [dict get $board_config ${lane}_map]
    dow -data $path [format 0x%08X $addr]
    mrd -force -size w -bin -file $scratch [format 0x%08X $addr] 921600
    if {[board_hash [dict get $board_config python] $scratch] ne [dict get $board_config ${lane}_map_sha256]} {error "Map readback mismatch; CPU left stopped"}
  }
  con
  puts "FPGA2 RUNNING. Start FPGA1 sender and inspect UART; no robot command was sent."
}
if {[info exists argv0] && [file normalize $argv0] eq [file normalize [info script]]} {
  if {$argc!=1} {error "Usage: run_fpga2.tcl board_config.tcl"}
  run_fpga2 [file normalize [lindex $argv 0]]
}
