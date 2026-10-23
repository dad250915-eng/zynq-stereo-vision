`timescale 1ns / 1ps

// Removes the three header words and routes payload until TLAST.
// A preamble or route mismatch discards the rest of the frame until TLAST.
// Header matching restarts only at the next frame, never within a bad frame.
// Preambles inside payload are ordinary data. Early TLAST aborts the header.
module axis_frame_decoder #(
    parameter [31:0] PREAMBLE_0 = 32'hA5A55A5A,
    parameter [31:0] PREAMBLE_1 = 32'h5A5AA5A5,
    parameter [15:0] ROUTE_ID_0 = 16'h0001,
    parameter [15:0] ROUTE_ID_1 = 16'h0002
)(
    input wire clk,
    input wire rst_n,
    input wire [31:0] s_axis_tdata,
    input wire s_axis_tvalid,
    output wire s_axis_tready,
    input wire s_axis_tlast,
    output wire [31:0] m_axis_0_tdata,
    output wire m_axis_0_tvalid,
    input wire m_axis_0_tready,
    output wire m_axis_0_tlast,
    output wire m_axis_0_tuser,
    output wire [31:0] m_axis_1_tdata,
    output wire m_axis_1_tvalid,
    input wire m_axis_1_tready,
    output wire m_axis_1_tlast,
    output wire m_axis_1_tuser
);
    localparam SEARCH0 = 3'd0, SEARCH1 = 3'd1, PARAM = 3'd2,
               DATA0 = 3'd3, DATA1 = 3'd4, DROP = 3'd5;
    reg [2:0] state;
    reg first_user;
    wire transfer = s_axis_tvalid && s_axis_tready;

    assign s_axis_tready = rst_n && ((state == DATA0) ? m_axis_0_tready :
                                  (state == DATA1) ? m_axis_1_tready : 1'b1);
    assign m_axis_0_tdata = s_axis_tdata;
    assign m_axis_1_tdata = s_axis_tdata;
    assign m_axis_0_tvalid = rst_n && (state == DATA0) && s_axis_tvalid;
    assign m_axis_1_tvalid = rst_n && (state == DATA1) && s_axis_tvalid;
    assign m_axis_0_tlast = (state == DATA0) && s_axis_tlast;
    assign m_axis_1_tlast = (state == DATA1) && s_axis_tlast;
    assign m_axis_0_tuser = (state == DATA0) && first_user;
    assign m_axis_1_tuser = (state == DATA1) && first_user;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state <= SEARCH0;
            first_user <= 1'b0;
        end else if (transfer) begin
            case (state)
                SEARCH0: begin
                    if (s_axis_tlast) state <= SEARCH0;
                    else if (s_axis_tdata == PREAMBLE_0) state <= SEARCH1;
                    else state <= DROP;
                end
                SEARCH1: begin
                    if (s_axis_tlast) state <= SEARCH0;
                    else if (s_axis_tdata == PREAMBLE_1) state <= PARAM;
                    else state <= DROP;
                end
                PARAM: begin
                    first_user <= (s_axis_tdata[15:0] == 16'h0001);
                    if (s_axis_tlast) state <= SEARCH0;
                    else case (s_axis_tdata[31:16])
                        ROUTE_ID_0: state <= DATA0;
                        ROUTE_ID_1: state <= DATA1;
                        default: state <= DROP;
                    endcase
                end
                DATA0, DATA1: begin
                    first_user <= 1'b0;
                    if (s_axis_tlast) state <= SEARCH0;
                end
                DROP: if (s_axis_tlast) state <= SEARCH0;
                default: state <= SEARCH0;
            endcase
        end
    end
endmodule
