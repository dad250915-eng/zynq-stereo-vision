# Nominal PCLK: AN5642 24 MHz oscillator + verified Vitis 720p register table.
# OV5640 DVP application guide R2.13C section 4.1.5, pp23-24: 42 MHz.
# 3035=41,3036=69,3037=13,3108=01,460c=20 (automatic divider).
# 4740=20: falling-edge data launch; capture RTL samples rising edge.
create_clock -name cam0_pclk -period 23.809524 [get_ports cmos_pclk_0]
create_clock -name cam1_pclk -period 23.809524 [get_ports cmos_pclk_1]
# Input delays intentionally unresolved: old 2.2..4ns is not a verified board budget.
# Add set_input_delay -clock_fall -min/-max once sensor tCO and relative trace delay
# are established. Do not claim complete input timing closure from this file.
# Dedicated-route exceptions scoped to the two camera PCLK IBUF outputs.
set_property CLOCK_DEDICATED_ROUTE FALSE [get_nets {cmos_pclk_0_IBUF}]
set_property CLOCK_DEDICATED_ROUTE FALSE [get_nets {cmos_pclk_1_IBUF}]

