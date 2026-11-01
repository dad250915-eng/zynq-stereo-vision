# XSCT 2023.2. Creates a new workspace and imports only application sources.
if {$argc!=2} {error "Usage: create_vitis_workspace.tcl matching.xsa new_workspace"}
set hw [file normalize [lindex $argv 0]]
set ws [file normalize [lindex $argv 1]]
set root [file normalize [file join [file dirname [info script]] ..]]
if {![file isfile $hw]} {error "XSA missing: $hw"}
if {[file exists $ws] && [llength [glob -nocomplain -directory $ws *]]} {error "Choose an empty workspace; existing projects are not overwritten"}
file mkdir $ws
set stage [file join $ws app_sources]
file mkdir $stage
foreach pattern {*.c *.h lscript.ld} {
  foreach p [glob -nocomplain -directory [file join $root fpga2_vitis] $pattern] {file copy $p $stage}
}
foreach dir {display_ctrl dynclk} {file copy [file join $root fpga2_vitis $dir] $stage}
# platform_init/ps7_init.c is not an application compilation unit.
setws $ws
platform create -name stereo_platform -hw $hw -proc ps7_cortexa9_0 -os standalone
platform active stereo_platform
domain active standalone_domain
bsp config stdin ps7_uart_0
bsp config stdout ps7_uart_0
platform generate
app create -name stereo_app -platform stereo_platform -domain standalone_domain -template {Empty Application(C)}
importsources -name stereo_app -path $stage
app config -name stereo_app -set linker-script [file join $ws stereo_app src lscript.ld]
app config -name stereo_app -add libraries m
app build -name stereo_app
puts "VITIS_BUILD_FINISHED: inspect $ws/stereo_app/Debug and map file"
