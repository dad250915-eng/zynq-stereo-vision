# AC7A035 core schematic: 200MHz differential oscillator -> BANK34 MRCC.
# Clocking Wizard generates the internal 50MHz clock; do not add a second
# create_clock on clk_out1. Its generated clock is derived by Vivado.
set_property PACKAGE_PIN R4 [get_ports sys_clk_p]
set_property PACKAGE_PIN T4 [get_ports sys_clk_n]
set_property IOSTANDARD DIFF_SSTL15 [get_ports {sys_clk_p sys_clk_n}]
# clk_wiz input XDC supplies the 200MHz primary clock from PRIM_IN_FREQ=200.
# Do not duplicate that primary create_clock here.
