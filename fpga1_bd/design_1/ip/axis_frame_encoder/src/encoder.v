`timescale 1ns/1ps
// Single-camera packet encoder. Instantiate twice; arbitrate externally.
// Unexpected SOF closes the truncated packet and restarts the same camera.
module axis_frame_encoder #(
    parameter [31:0] PREAMBLE_0 = 32'hA5A55A5A,
    parameter [31:0] PREAMBLE_1 = 32'h5A5AA5A5,
    parameter [15:0] ROUTE_ID = 16'h0001,
    parameter integer USER_WIDTH = 4
)(
    input wire clk,
    input wire rst_n,
    input wire [31:0] s_axis_tdata,
    input wire s_axis_tvalid,
    output wire s_axis_tready,
    input wire s_axis_tlast,
    input wire [USER_WIDTH-1:0] s_axis_tuser,
    output reg [31:0] m_axis_tdata,
    output wire m_axis_tvalid,
    input wire m_axis_tready,
    output reg m_axis_tlast,
    output wire [3:0] m_axis_tkeep,
    output wire [2:0] debug_state,
    output wire debug_first_user,
    output wire debug_input_sof_accept,
    output wire debug_resync_event,
    output reg [31:0] debug_resync_count
);
    localparam IDLE=3'd0, SYNC0=3'd1, SYNC1=3'd2,
               PARAM=3'd3, DATA=3'd5;
    reg [2:0] state;
    reg first_user, hold_last;
    reg [31:0] hold_data;
    wire [31:0] in_data = s_axis_tdata;
    wire in_valid = s_axis_tvalid;
    wire in_last = s_axis_tlast;
    wire in_user = s_axis_tuser[0]; // Preserve original low-bit SOF semantics.
    wire [15:0] route_id = ROUTE_ID;
    assign m_axis_tkeep = 4'hf;
    wire in_ready = rst_n && ((state==IDLE) ||
                    ((state==DATA) && !hold_last && m_axis_tready));
    wire in_fire = in_valid && in_ready;
    wire out_fire = m_axis_tvalid && m_axis_tready;
    // Keep the final pending pixel until the next input reveals whether a
    // missing TLAST must be repaired. No synthetic pixel is inserted.
    assign m_axis_tvalid = rst_n && ((state==SYNC0) || (state==SYNC1) ||
                           (state==PARAM) ||
                           ((state==DATA) && (hold_last || in_valid)));
    assign s_axis_tready = in_ready;
    assign debug_state=state;
    assign debug_first_user=first_user;
    assign debug_input_sof_accept=in_fire && in_user;
    assign debug_resync_event=(state==DATA) && !hold_last && in_fire && in_user;
    always @* begin
        m_axis_tdata=0;
        m_axis_tlast=0;
        case(state)
            SYNC0: m_axis_tdata=PREAMBLE_0;
            SYNC1: m_axis_tdata=PREAMBLE_1;
            PARAM: m_axis_tdata={route_id,15'd0,first_user};
            DATA: begin
                m_axis_tdata=hold_data;
                m_axis_tlast=hold_last || (in_valid && in_user);
            end
            default: begin end
        endcase
    end
    always @(posedge clk or negedge rst_n) begin
        if(!rst_n) begin
            state<=IDLE; first_user<=0;
            hold_data<=0; hold_last<=0; debug_resync_count<=0;
        end else begin
            case(state)
                IDLE: if(in_fire) begin
                    hold_data<=in_data; hold_last<=in_last;
                    first_user<=in_user; state<=SYNC0;
                end
                SYNC0: if(out_fire) state<=SYNC1;
                SYNC1: if(out_fire) state<=PARAM;
                PARAM: if(out_fire) state<=DATA;
                DATA: if(out_fire) begin
                    if(hold_last) begin
                        state<=IDLE;
                    end else begin
                        // out_fire implies in_fire here. The incoming pixel
                        // replaces the emitted one, including during recovery.
                        hold_data<=in_data; hold_last<=in_last;
                        if(in_user) begin
                            first_user<=1; state<=SYNC0;
                            debug_resync_count<=debug_resync_count+1'b1;
                            // Recovery starts a new packet on the SAME camera.
                        end
                    end
                end
                default: state<=IDLE;
            endcase
        end
    end
endmodule
