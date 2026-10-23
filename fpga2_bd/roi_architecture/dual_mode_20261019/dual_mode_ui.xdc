# Verified Z100 IO allocation and base schematic 1V2, sheet 11.
set_property PACKAGE_PIN J13 [get_ports roi_mode_key_n]
set_property IOSTANDARD LVCMOS18 [get_ports roi_mode_key_n]
set_property PACKAGE_PIN U21 [get_ports roi_alarm_led]
set_property IOSTANDARD LVCMOS33 [get_ports roi_alarm_led]
# Only the asynchronous pin -> first synchronizer stage is false-pathed.
set_false_path -from [get_ports roi_mode_key_n] -to [get_cells -hier -filter {NAME =~ */key_meta_reg}]
