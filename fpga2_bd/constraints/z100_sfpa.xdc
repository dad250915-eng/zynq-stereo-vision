# Z100 FFG900, SFPA J31, GTX bank 109 lane 0.
# Reference: Z100 baseboard 1V2 p15, core schematic p6, vendor example 57.
# Dedicated GT pins: do NOT apply LVCMOS / LVDS IOSTANDARD.
set_property PACKAGE_PIN AD10 [get_ports GT_DIFF_REFCLK1_0_clk_p]
set_property PACKAGE_PIN AD9 [get_ports GT_DIFF_REFCLK1_0_clk_n]
set_property PACKAGE_PIN AH10 [get_ports {GT_SERIAL_RX_0_rxp[0]}]
set_property PACKAGE_PIN AH9 [get_ports {GT_SERIAL_RX_0_rxn[0]}]
set_property PACKAGE_PIN AK10 [get_ports {GT_SERIAL_TX_0_txp[0]}]
set_property PACKAGE_PIN AK9 [get_ports {GT_SERIAL_TX_0_txn[0]}]
create_clock -name z100_sfpa_refclk -period 6.400 [get_ports GT_DIFF_REFCLK1_0_clk_p]
# Aurora must use 156.25 MHz reference, 2.5 Gbps, GTXE2_CHANNEL_X0Y0.
# Enable SFPA transmission via sfpa_tx_disable=0; see staged control XDC.

