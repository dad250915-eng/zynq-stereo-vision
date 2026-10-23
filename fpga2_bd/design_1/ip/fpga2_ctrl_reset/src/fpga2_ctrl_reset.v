`timescale 1ns/1ps
// Reverse-channel control packets. Forward video packet format is unchanged.
module fpga2_ctrl_reset #(
    parameter integer HEARTBEAT_CYCLES=6250
)(
    (* X_INTERFACE_INFO="xilinx.com:signal:clock:1.0 clk CLK", X_INTERFACE_PARAMETER="ASSOCIATED_BUSIF M_AXIS, ASSOCIATED_RESET rst_n" *)
    input wire clk,
    (* X_INTERFACE_INFO="xilinx.com:signal:reset:1.0 rst_n RST", X_INTERFACE_PARAMETER="POLARITY ACTIVE_LOW" *)
    input wire rst_n,
    input wire channel_up, sys_reset_out,
    input wire receiver_ready, video_reset_n,
    (* X_INTERFACE_INFO="xilinx.com:interface:axis:1.0 M_AXIS TDATA" *)
    output wire [31:0] m_axis_tdata,
    (* X_INTERFACE_INFO="xilinx.com:interface:axis:1.0 M_AXIS TKEEP" *)
    output wire [3:0] m_axis_tkeep,
    (* X_INTERFACE_INFO="xilinx.com:interface:axis:1.0 M_AXIS TLAST" *)
    output wire m_axis_tlast,
    (* X_INTERFACE_INFO="xilinx.com:interface:axis:1.0 M_AXIS TVALID" *)
    output wire m_axis_tvalid,
    (* X_INTERFACE_INFO="xilinx.com:interface:axis:1.0 M_AXIS TREADY" *)
    input wire m_axis_tready,
    output wire debug_ready
);
    wire local_n;
    xpm_cdc_async_rst #(.DEST_SYNC_FF(4),.INIT_SYNC_FF(1),.RST_ACTIVE_HIGH(0)) reset_sync (
        .src_arst(rst_n && channel_up && !sys_reset_out),.dest_clk(clk),.dest_arst(local_n));
    // Level inputs: asynchronous revoke, four-clock qualified release.
    // Software/admission must hold receiver_ready low until VDMA is armed.
    wire allow;
    xpm_cdc_async_rst #(.DEST_SYNC_FF(4),.INIT_SYNC_FF(1),.RST_ACTIVE_HIGH(0)) ready_sync (
        .src_arst(local_n && receiver_ready && video_reset_n),
        .dest_clk(clk),.dest_arst(allow));
    assign debug_ready=local_n && allow;
    localparam IDLE=0, HEADER=1, PAYLOAD=2;
    reg [1:0] state=IDLE;
    reg sent_allow=0, packet_allow=0;
    reg [31:0] timer=0;
    assign m_axis_tvalid=local_n && state!=IDLE;
    assign m_axis_tdata=(state==HEADER) ? 32'h52445931 : (packet_allow ? 32'hA55A0001 : 32'hA55A0000);
    assign m_axis_tlast=(state==PAYLOAD);
    assign m_axis_tkeep=4'hf;
    always @(posedge clk or negedge local_n) begin
        if(!local_n) begin state<=IDLE; sent_allow<=0; packet_allow<=0;timer<=0;end
        else case(state)
            IDLE: if(timer==0 || allow!=sent_allow) begin
                packet_allow<=allow;state<=HEADER;
            end else timer<=timer-1;
            HEADER: if(m_axis_tready) state<=PAYLOAD;
            PAYLOAD: if(m_axis_tready) begin
                sent_allow<=packet_allow;timer<=(HEARTBEAT_CYCLES > 0 ? HEARTBEAT_CYCLES-1 : 0);state<=IDLE;
            end
            default: state<=IDLE;
        endcase
    end
endmodule
