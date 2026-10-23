`timescale 1ns/1ps
// Reverse-channel control packets. Forward video packet format is unchanged.
module aurora_ready_tx #(
    parameter integer HEARTBEAT_CYCLES=6250
)(
    input wire clk, rst_n, channel_up, sys_reset_out,
    input wire receiver_ready, video_reset_n,
    output wire [31:0] tx_data,
    output wire [3:0] tx_keep,
    output wire tx_last, tx_valid,
    input wire tx_ready,
    output wire debug_ready
);
    wire local_n;
    xpm_cdc_async_rst #(.DEST_SYNC_FF(4),.INIT_SYNC_FF(1),.RST_ACTIVE_HIGH(0)) reset_sync (
        .src_arst(rst_n && channel_up && !sys_reset_out),.dest_clk(clk),.dest_arst(local_n));
    wire video_ready_sync;
    xpm_cdc_single #(.DEST_SYNC_FF(3),.SRC_INPUT_REG(0),.INIT_SYNC_FF(1)) video_sync (
        .src_clk(1'b0),.src_in(video_reset_n),.dest_clk(clk),.dest_out(video_ready_sync));
    wire allow = receiver_ready && video_ready_sync;
    assign debug_ready=local_n && allow;
    localparam IDLE=0, HEADER=1, PAYLOAD=2;
    reg [1:0] state=IDLE;
    reg sent_allow=0, packet_allow=0;
    reg [31:0] timer=0;
    assign tx_valid=local_n && state!=IDLE;
    assign tx_data=(state==HEADER) ? 32'h52445931 : (packet_allow ? 32'hA55A0001 : 32'hA55A0000);
    assign tx_last=(state==PAYLOAD);
    assign tx_keep=4'hf;
    always @(posedge clk or negedge local_n) begin
        if(!local_n) begin state<=IDLE; sent_allow<=0; packet_allow<=0;timer<=0;end
        else case(state)
            IDLE: if(timer==0 || allow!=sent_allow) begin
                packet_allow<=allow;state<=HEADER;
            end else timer<=timer-1;
            HEADER: if(tx_ready) state<=PAYLOAD;
            PAYLOAD: if(tx_ready) begin
                sent_allow<=packet_allow;timer<=HEARTBEAT_CYCLES-1;state<=IDLE;
            end
            default: state<=IDLE;
        endcase
    end
endmodule
