set project_root [file normalize [file join [file dirname [info script]] ..]]
if {[current_project -quiet] eq ""} {
    open_project $project_root/stereo_match_fpga1.xpr
} elseif {[file normalize [get_property DIRECTORY [current_project]]] ne $project_root} {
    error "Open stereo_match_fpga1.xpr before adding these constraints"
}
set pins_file $project_root/constraints/ax7a035b_an5642_pins.xdc
add_files -fileset constrs_1 -norecurse $pins_file
set_property USED_IN_SYNTHESIS true [get_files $pins_file]
set_property USED_IN_IMPLEMENTATION true [get_files $pins_file]
puts "Camera pin constraints registered; timing reference intentionally not enabled."
