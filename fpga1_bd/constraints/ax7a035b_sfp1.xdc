# AX7A035B carrier SFP1 (J2), AC7A035 core BANK216 channel 0.
# Core schematic p6/p9; carrier schematic p2/p3.
# Do not add LVCMOS/LVDS IOSTANDARDs to dedicated GTP pins.
set_property PACKAGE_PIN F6 [get_ports GT_DIFF_REFCLK1_0_clk_p]
set_property PACKAGE_PIN E6 [get_ports GT_DIFF_REFCLK1_0_clk_n]
create_clock -name sfp_refclk_125m -period 8.000 [get_ports GT_DIFF_REFCLK1_0_clk_p]
set_property PACKAGE_PIN B4 [get_ports {GT_SERIAL_TX_0_txp[0]}]
set_property PACKAGE_PIN A4 [get_ports {GT_SERIAL_TX_0_txn[0]}]
set_property PACKAGE_PIN B8 [get_ports {GT_SERIAL_RX_0_rxp[0]}]
set_property PACKAGE_PIN A8 [get_ports {GT_SERIAL_RX_0_rxn[0]}]
# SFP1_TX_DIS -> B15_L5_P -> J15. Carrier has 4.7k pull-up (disabled).
# BD drives this output LOW to enable module transmitter.
set_property PACKAGE_PIN J15 [get_ports {sfp1_tx_disable[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {sfp1_tx_disable[0]}]
set_property DRIVE 4 [get_ports {sfp1_tx_disable[0]}]
set_property SLEW SLOW [get_ports {sfp1_tx_disable[0]}]
