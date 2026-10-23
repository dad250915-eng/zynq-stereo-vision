# Only first-stage CDC data and asynchronous reset synchronizer assertion.
set_false_path -to [get_pins -hierarchical -filter {NAME =~ *rx_admission_ctrl_0*arm_sync_reg?0?/D || NAME =~ *rx_admission_ctrl_0*status_meta_reg*/D || NAME =~ *rx_admission_ctrl_0*reset_sync_reg*/CLR}]
