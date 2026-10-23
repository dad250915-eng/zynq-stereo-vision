# Deliberately scoped CDC exceptions. Do not cut clocks or functional datapaths.
# These chains asynchronously assert and shift out reset in destination clocks.
set camera_reset_clr [get_pins -quiet -hierarchical -regexp {.*alinx_ov5640_[01]/inst/(axis_reset_pclk_reg\[[0-3]\]|fifo_ready_axis_reg\[[0-2]\])/CLR}]
set_false_path -to $camera_reset_clr
# Stable level passes through the two-flop ASYNC_REG done_sync chain.
# Only the asynchronous input to stage zero is excluded, not Q0 -> D1.
set aurora_done_d [get_pins -quiet -hierarchical -regexp {.*aurora_reset_ctrl_0/inst/done_sync_reg\[0\]/D}]
set_false_path -to $aurora_done_d
# Camera initialization status levels: meta is the first of two ASYNC_REG stages.
set camera_status_meta [get_pins -quiet -hierarchical -regexp {.*axis_tx_reset_ctrl_0/inst/(done_meta_reg|error_meta_reg)\[[01]\]/D}]
set_false_path -to $camera_status_meta
