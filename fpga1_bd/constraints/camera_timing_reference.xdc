# REFERENCE ONLY: not registered in the active constraint set.
# Vendor demo values, not measured/validated for the current Vitis 720p table.
# Check PCLK, sensor launch edge, setup/hold and board/cable skew before enabling.
create_clock -name cam0_pclk -period 11.904 [get_ports cmos_pclk_0]
create_clock -name cam1_pclk -period 11.904 [get_ports cmos_pclk_1]
set_input_delay -clock cam0_pclk -min 2.200 [get_ports {cmos_d_0[*] cmos_href_0 cmos_vsync_0}]
set_input_delay -clock cam0_pclk -max 4.000 [get_ports {cmos_d_0[*] cmos_href_0 cmos_vsync_0}]
set_input_delay -clock cam1_pclk -min 2.200 [get_ports {cmos_d_1[*] cmos_href_1 cmos_vsync_1}]
set_input_delay -clock cam1_pclk -max 4.000 [get_ports {cmos_d_1[*] cmos_href_1 cmos_vsync_1}]
# Run after synthesis, when the input buffers exist. Scope to these two inputs only.
foreach camera_port {cmos_pclk_0 cmos_pclk_1} {
    set input_net [get_nets -of_objects [get_ports $camera_port]]
    set input_buffer [get_cells -of_objects $input_net -filter {REF_NAME =~ IBUF*}]
    set buffered_net [get_nets -of_objects [get_pins -of_objects $input_buffer -filter {REF_PIN_NAME == O}]]
    if {[llength $buffered_net] != 1} {error "Cannot identify PCLK IBUF output for $camera_port"}
    set_property CLOCK_DEDICATED_ROUTE FALSE $buffered_net
}
