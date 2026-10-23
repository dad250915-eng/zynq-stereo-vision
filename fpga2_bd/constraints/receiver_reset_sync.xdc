# Only asynchronous assertion inputs of receiver reset synchronizer flops.
# Do not exclude synchronizer data paths or downstream reset release timing.
set_false_path -to [get_pins -hierarchical -filter {NAME =~ *aurora_receive_reset_0/inst/rx_domain/release_sync_reg*/CLR || NAME =~ *aurora_receive_reset_0/inst/video_domain/release_sync_reg*/CLR}]
