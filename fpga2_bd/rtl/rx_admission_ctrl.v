`timescale 1ns/1ps
// Admission and fault monitor for a source that CANNOT honor TREADY.
// Data/keep/last remain on the existing Aurora -> AXIS clock converter net.
// Not an elastic buffer. A lost beat latches fault and flushes the RX pipeline.
module rx_admission_ctrl(
    input wire rx_clk,
    input wire ctrl_clk,
    input wire rst_n,
    input wire sw_enable,
    input wire channel_up,
    input wire sys_reset_out,
    input wire rx_aresetn,
    input wire rx_valid,
    input wire fifo_ready,
    output wire admitted_valid,
    output wire receiver_ready,
    output wire clear_req,
    output wire [31:0] status
);
    (* ASYNC_REG="TRUE", SHREG_EXTRACT="NO" *) reg [1:0] arm_sync=0;
    (* ASYNC_REG="TRUE", SHREG_EXTRACT="NO" *) reg [1:0] reset_sync=0;
    always @(posedge rx_clk or negedge rst_n)
        if (!rst_n) reset_sync<=0;
        else reset_sync<={reset_sync[0],1'b1};
    reg streaming=0;
    reg fault=0;
    always @(posedge rx_clk or negedge reset_sync[1]) begin
        if (!reset_sync[1]) begin arm_sync<=0; streaming<=0; fault<=0; end
        else begin
            arm_sync <= {arm_sync[0],sw_enable};
            if (!arm_sync[1]) begin streaming<=0; fault<=0; end
            else begin
                if (streaming && (!channel_up || sys_reset_out ||
                                 (rx_valid && !fifo_ready)))
                    fault<=1;
                if (fault || !channel_up || sys_reset_out || !rx_aresetn)
                    streaming<=0;
                else if (fifo_ready) streaming<=1;
            end
        end
    end
    assign clear_req = !arm_sync[1] || fault;
    assign receiver_ready = streaming && arm_sync[1] && !fault && channel_up && !sys_reset_out && rx_aresetn;
    assign admitted_valid = rx_valid && streaming && arm_sync[1] &&
                            !fault && channel_up && !sys_reset_out && rx_aresetn;
    wire [4:0] rx_status={arm_sync[1],rx_aresetn,fault,streaming,channel_up};
    (* ASYNC_REG="TRUE", SHREG_EXTRACT="NO" *) reg [4:0] status_meta=0,status_sync=0;
    always @(posedge ctrl_clk or negedge rst_n)
        if (!rst_n) begin status_meta<=0; status_sync<=0; end
        else begin status_meta<=rx_status; status_sync<=status_meta; end
    // Signature lets software reject the previous bitstream before enabling RX.
    assign status = 32'h52320000 | {27'd0,status_sync};
endmodule

