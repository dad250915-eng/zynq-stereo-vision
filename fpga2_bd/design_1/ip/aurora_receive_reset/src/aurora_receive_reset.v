`timescale 1ns/1ps
// Receiver datapath reset only. Does NOT drive Aurora reset or gt_reset.
module aurora_receive_reset #(
    parameter integer RELEASE_CYCLES = 32
)(
    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 rx_clk CLK", X_INTERFACE_PARAMETER = "XIL_INTERFACENAME rx_clk, ASSOCIATED_RESET rx_axis_aresetn" *)
    input wire rx_clk,
    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 video_clk CLK", X_INTERFACE_PARAMETER = "XIL_INTERFACENAME video_clk, ASSOCIATED_RESET video_axis_aresetn" *)
    input wire video_clk,
    (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 rst_n RST", X_INTERFACE_PARAMETER = "XIL_INTERFACENAME rst_n, POLARITY ACTIVE_LOW" *)
    input wire rst_n,
    input wire channel_up,
    input wire sys_reset_out,
    input wire clear_req,
    (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 rx_axis_aresetn RST", X_INTERFACE_PARAMETER = "XIL_INTERFACENAME rx_axis_aresetn, POLARITY ACTIVE_LOW" *)
    output wire rx_axis_aresetn,
    (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 video_axis_aresetn RST", X_INTERFACE_PARAMETER = "XIL_INTERFACENAME video_axis_aresetn, POLARITY ACTIVE_LOW" *)
    output wire video_axis_aresetn
);
    // Either clock may stop: assertion must not depend on clock edges.
    // These inputs must be stable level status/control signals, not pulses
    // from combinational datapath decoding. Any low glitch safely restarts release.
    wire release_allowed = rst_n & channel_up & ~sys_reset_out & ~clear_req;
    aurora_receive_reset_domain #(.RELEASE_CYCLES(RELEASE_CYCLES)) rx_domain (
        .clk(rx_clk), .release_allowed(release_allowed), .aresetn(rx_axis_aresetn));
    aurora_receive_reset_domain #(.RELEASE_CYCLES(RELEASE_CYCLES)) video_domain (
        .clk(video_clk), .release_allowed(release_allowed), .aresetn(video_axis_aresetn));
endmodule

module aurora_receive_reset_domain #(
    parameter integer RELEASE_CYCLES = 32
)(input wire clk, input wire release_allowed, output wire aresetn);
    localparam integer HOLD_CYCLES = (RELEASE_CYCLES < 32) ? 32 : RELEASE_CYCLES;
    localparam integer COUNT_WIDTH = $clog2(HOLD_CYCLES);
    (* ASYNC_REG = "TRUE", SHREG_EXTRACT = "NO" *) reg [1:0] release_sync = 2'b00;
    reg [COUNT_WIDTH-1:0] count = 0;
    reg released = 1'b0;
    always @(posedge clk or negedge release_allowed)
        if (!release_allowed) release_sync <= 2'b00;
        else release_sync <= {release_sync[0],1'b1};
    // Local synchronized release drives the counter reset. Never feed the
    // asynchronous status combination straight into normal synchronous logic.
    always @(posedge clk or negedge release_sync[1]) begin
        if (!release_sync[1]) begin
            count <= 0;
            released <= 1'b0;
        end else if (!released) begin
            if (count == HOLD_CYCLES-1) released <= 1'b1;
            else count <= count + 1'b1;
        end
    end
    assign aresetn = released;
endmodule
