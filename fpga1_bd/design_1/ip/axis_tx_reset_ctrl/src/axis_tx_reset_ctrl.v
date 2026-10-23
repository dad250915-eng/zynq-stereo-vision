`timescale 1ns/1ps
// Reset release gate only. Does not generate Aurora reset or gt_reset.
module axis_tx_reset_ctrl (
    input wire user_clk,             // Aurora user_clk_out
    input wire rst_n,                // Whole-system reset, active LOW
    input wire channel_up,           // Aurora channel_up
    input wire sys_reset_out,        // Aurora system reset, active HIGH
    input wire [1:0] camera_done,     // Both camera initialization completion flags
    input wire [1:0] camera_error,    // Both camera initialization error flags
    output wire m_axis_aresetn        // Clock Converter destination reset, active LOW
);
    // Loss of link/system reset asserts reset even if user_clk has stopped.
    // This combined signal is used only for reset assertion; release is synchronized.
    wire reset_ok = rst_n & channel_up & ~sys_reset_out;
    (* ASYNC_REG = "TRUE" *) reg [1:0] reset_release = 2'b00;
    always @(posedge user_clk or negedge reset_ok)
        if (!reset_ok) reset_release <= 2'b00;
        else reset_release <= {reset_release[0], 1'b1};
    wire local_reset_n = reset_release[1];

    (* ASYNC_REG = "TRUE" *) reg [1:0] done_meta = 0, done_sync = 0;
    (* ASYNC_REG = "TRUE" *) reg [1:0] error_meta = 0, error_sync = 0;
    always @(posedge user_clk or negedge local_reset_n)
        if (!local_reset_n) begin
            done_meta <= 0; done_sync <= 0;
            error_meta <= 0; error_sync <= 0;
        end else begin
            done_meta <= camera_done; done_sync <= done_meta;
            error_meta <= camera_error; error_sync <= error_meta;
        end

    // Require sixteen consecutive valid user-clock samples before release.
    reg [15:0] stable = 0;
    always @(posedge user_clk or negedge local_reset_n)
        if (!local_reset_n) stable <= 0;
        else if ((&done_sync) && !(|error_sync))
            stable <= {stable[14:0], 1'b1};
        else stable <= 0;
    assign m_axis_aresetn = stable[15];
endmodule
