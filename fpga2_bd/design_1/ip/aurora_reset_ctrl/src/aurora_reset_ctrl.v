`timescale 1ns / 1ps
// FPGA-targeted Verilog-2001. No MicroBlaze or AXI-Lite required.
module aurora_reset_ctrl #(
    parameter integer GT_HOLD_CYCLES = 1024,
    parameter integer USER_RESET_CYCLES = 256
)(
    input  wire init_clk,       // Continuous 50 MHz clock
    input  wire user_clk,       // Aurora user_clk_out; may stop during GT reset
    input  wire rst_n,          // Active-low whole-system reset
    input  wire clk_locked,     // Lock of the PLL providing init_clk
    input  wire restart_req,    // Active-high request; rising edge, hold >= 3 init cycles
    output wire aurora_reset,  // To Aurora reset (active HIGH)
    output wire aurora_gt_reset,// To Aurora gt_reset (active HIGH)
    output wire busy,
    output wire ready,         // Sequencer done, NOT channel_up
    output wire [2:0] state_debug
);
    localparam [2:0] HOLD_GT = 3'd0, WAIT_USER = 3'd1,
                     RUN = 3'd2, PRE_GT = 3'd3;
    localparam integer GT_LIMIT = (GT_HOLD_CYCLES < 16) ? 16 : GT_HOLD_CYCLES;
    // Margin over the normal-operation minimum 128 user cycles.
    localparam integer USER_LIMIT = (USER_RESET_CYCLES < 256) ? 256 : USER_RESET_CYCLES;
    wire reset_ok = rst_n & clk_locked;
    (* ASYNC_REG = "TRUE" *) reg [1:0] release_sync = 2'b00;
    always @(posedge init_clk or negedge reset_ok)
        if (!reset_ok) release_sync <= 2'b00;
        else release_sync <= {release_sync[0], 1'b1};
    wire ctrl_reset_n = release_sync[1];

    (* ASYNC_REG = "TRUE" *) reg [1:0] req_sync = 2'b00;
    reg req_last = 1'b0;
    always @(posedge init_clk or negedge ctrl_reset_n)
        if (!ctrl_reset_n) begin req_sync <= 0; req_last <= 0; end
        else begin req_sync <= {req_sync[0], restart_req}; req_last <= req_sync[1]; end
    wire restart_edge = req_sync[1] & ~req_last;

    reg [2:0] state = HOLD_GT;
    reg [31:0] init_count = 0;
    reg reset_reg = 1'b1;
    reg gt_reset_reg = 1'b1;
    assign aurora_reset = reset_reg | ~ctrl_reset_n;
    assign aurora_gt_reset = gt_reset_reg | ~ctrl_reset_n;
    assign ready = ctrl_reset_n && (state == RUN);
    assign busy = ~ready;
    assign state_debug = state;

    // Reset this counter whenever protocol reset is released OR GT is in reset.
    // Async clear works even when user_clk is stopped. Deassertion is synchronized.
    wire count_enable = aurora_reset & ~aurora_gt_reset & ctrl_reset_n;
    (* ASYNC_REG = "TRUE" *) reg [1:0] count_enable_sync = 0;
    always @(posedge user_clk or negedge count_enable)
        if (!count_enable) count_enable_sync <= 0;
        else count_enable_sync <= {count_enable_sync[0], 1'b1};
    reg [31:0] user_count = 0;
    reg user_done = 0;
    always @(posedge user_clk or negedge count_enable_sync[1])
        if (!count_enable_sync[1]) begin user_count <= 0; user_done <= 0; end
        else if (!user_done) begin
            if (user_count == USER_LIMIT-1) user_done <= 1;
            else user_count <= user_count + 1'b1;
        end
    (* ASYNC_REG = "TRUE" *) reg [1:0] done_sync = 0;
    always @(posedge init_clk or negedge ctrl_reset_n)
        if (!ctrl_reset_n) done_sync <= 0;
        else done_sync <= {done_sync[0], user_done};

    always @(posedge init_clk or negedge ctrl_reset_n) begin
        if (!ctrl_reset_n) begin
            state <= HOLD_GT; init_count <= 0; reset_reg <= 1; gt_reset_reg <= 1;
        end else begin
            case (state)
                HOLD_GT: begin
                    reset_reg <= 1; gt_reset_reg <= 1;
                    if (init_count == GT_LIMIT-1) begin
                        init_count <= 0; gt_reset_reg <= 0; state <= WAIT_USER;
                    end else init_count <= init_count + 1'b1;
                end
                WAIT_USER: begin
                    // Four init cycles flush any old synchronized completion.
                    if (init_count < 4) init_count <= init_count + 1'b1;
                    else if (done_sync[1]) begin
                        reset_reg <= 0; init_count <= 0; state <= RUN;
                    end
                end
                RUN: begin
                    init_count <= 0;
                    if (restart_edge) begin reset_reg <= 1; state <= PRE_GT; end
                end
                PRE_GT: begin
                    // Keep GT running while protocol reset propagates/counts.
                    if (init_count < 4) init_count <= init_count + 1'b1;
                    else if (done_sync[1]) begin
                        gt_reset_reg <= 1; init_count <= 0; state <= HOLD_GT;
                    end
                end
                default: begin
                    state <= HOLD_GT; init_count <= 0; reset_reg <= 1; gt_reset_reg <= 1;
                end
            endcase
        end
    end
endmodule
