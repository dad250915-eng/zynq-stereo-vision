# STAGED: enable only after adding scalar OUTPUT sfpa_tx_disable.
# Connect xlconstant (width 1, value 0) to this output.
set_property -dict {PACKAGE_PIN AB19 IOSTANDARD LVCMOS33} [get_ports {sfpa_tx_disable[0]}]

