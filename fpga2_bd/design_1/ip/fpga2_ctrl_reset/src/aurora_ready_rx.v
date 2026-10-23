`timescale 1ns/1ps
module aurora_ready_rx #(
    parameter integer TIMEOUT_CYCLES=625000
)(
    input wire clk, video_clk, rst_n, channel_up, sys_reset_out,
    input wire tx_local_reset_n,
    input wire [31:0] rx_data,
    input wire [3:0] rx_keep,
    input wire rx_last, rx_valid, rx_error,
    output wire video_reset_n, tx_reset_n,
    output wire debug_remote_ready,
    output reg [31:0] debug_ready_packets=0
);
    wire local_n;
    xpm_cdc_async_rst #(.DEST_SYNC_FF(4),.INIT_SYNC_FF(1),.RST_ACTIVE_HIGH(0)) reset_sync (
        .src_arst(rst_n && channel_up && !sys_reset_out),.dest_clk(clk),.dest_arst(local_n));
    reg remote_ready=0;
    reg [31:0] lease=0;
    // 0: packet boundary, 1: expected payload, 2: discard to TLAST.
    reg [1:0] parser=0;
    assign debug_remote_ready=remote_ready && local_n;
    wire run_allowed=local_n && remote_ready && tx_local_reset_n;
    xpm_cdc_async_rst #(.DEST_SYNC_FF(4),.INIT_SYNC_FF(1),.RST_ACTIVE_HIGH(0)) video_release (
        .src_arst(run_allowed),.dest_clk(video_clk),.dest_arst(video_reset_n));
    xpm_cdc_async_rst #(.DEST_SYNC_FF(4),.INIT_SYNC_FF(1),.RST_ACTIVE_HIGH(0)) tx_release (
        .src_arst(run_allowed),.dest_clk(clk),.dest_arst(tx_reset_n));
    always @(posedge clk or negedge local_n) begin
        if(!local_n) begin remote_ready<=0;lease<=0;parser<=0;debug_ready_packets<=0;end
        else begin
            if(lease!=0) lease<=lease-1;
            if(lease<=1) remote_ready<=0;
            if(rx_error) begin remote_ready<=0;lease<=0;parser<=2;end
            else if(rx_valid) begin
                case(parser)
                    0: if(rx_keep==4'hf && rx_data==32'h52445931 && !rx_last) parser<=1;
                       else begin parser<=rx_last ? 0:2;remote_ready<=0;lease<=0;end
                    1: begin
                        parser<=rx_last ? 0:2;
                        if(rx_keep==4'hf && rx_last && rx_data==32'hA55A0001) begin
                            remote_ready<=1;lease<=TIMEOUT_CYCLES;debug_ready_packets<=debug_ready_packets+1;
                        end else begin remote_ready<=0;lease<=0;end
                    end
                    default: if(rx_last) parser<=0;
                endcase
            end
        end
    end
endmodule
