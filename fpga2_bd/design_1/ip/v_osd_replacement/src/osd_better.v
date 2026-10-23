//=============================================================================
// v_osd_replacement
//
// Two 1280x720 AXI4-Stream sources -> one 1280x720 AXI4-Stream output.
// The centre 640 columns of camera 0 are placed on the left and the centre
// 640 columns of camera 1 are placed on the right.
//
// A complete input line is captured from both streams at the same time, then
// the cropped pixels are emitted.  This intentionally back-pressures the two
// VDMA MM2S channels while the saved line is being emitted.  At 150 MHz the
// resulting average stream rate is sufficient for a 1280x720/60 video output.
// All counters advance only on real AXI4-Stream handshakes.
//=============================================================================

`timescale 1ns / 1ps

module v_osd_replacement (
    input  wire         aclk,
    input  wire         aclken,
    input  wire         aresetn,

    input  wire [23:0]  s_axis_video0_tdata,
    input  wire         s_axis_video0_tvalid,
    output wire         s_axis_video0_tready,
    input  wire         s_axis_video0_tlast,
    input  wire         s_axis_video0_tuser,

    input  wire [23:0]  s_axis_video1_tdata,
    input  wire         s_axis_video1_tvalid,
    output wire         s_axis_video1_tready,
    input  wire         s_axis_video1_tlast,
    input  wire         s_axis_video1_tuser,

    output wire [23:0]  m_axis_video_tdata,
    output wire         m_axis_video_tvalid,
    input  wire         m_axis_video_tready,
    output wire         m_axis_video_tlast,
    output wire         m_axis_video_tuser,

    output wire         irq
);

    localparam integer IN_W       = 1280;
    localparam integer OUT_W      = 1280;
    localparam integer OUT_H      = 720;
    localparam integer CROP_X     = 320;
    localparam integer CROP_W     = 640;

    localparam [1:0] ST_WAIT_SOF = 2'd0;
    localparam [1:0] ST_CAPTURE  = 2'd1;
    localparam [1:0] ST_OUTPUT   = 2'd2;

    (* ram_style = "block" *) reg [23:0] line0 [0:CROP_W-1];
    (* ram_style = "block" *) reg [23:0] line1 [0:CROP_W-1];

    reg [1:0]  state;
    reg [10:0] input_x;
    reg [10:0] output_x;
    reg [9:0]  output_y;

    reg [23:0] out_data;
    reg        out_valid;
    reg        out_last;
    reg        out_user;

    wire both_valid = s_axis_video0_tvalid && s_axis_video1_tvalid;
    wire both_sof   = s_axis_video0_tuser  && s_axis_video1_tuser;

    // In WAIT_SOF neither first pixel is consumed until both VDMA streams are
    // presenting their SOF beat. AXI masters must hold that beat while READY=0.
    wire accept_sof = aclken && (state == ST_WAIT_SOF) && both_valid && both_sof;
    wire accept_pair = aclken && (state == ST_CAPTURE) && both_valid;

    assign s_axis_video0_tready = (state == ST_WAIT_SOF)
                                      ? (aclken && s_axis_video1_tvalid &&
                                         s_axis_video0_tuser && s_axis_video1_tuser)
                                      : ((state == ST_CAPTURE)
                                             ? (aclken && s_axis_video1_tvalid)
                                             : 1'b0);
    assign s_axis_video1_tready = (state == ST_WAIT_SOF)
                                      ? (aclken && s_axis_video0_tvalid &&
                                         s_axis_video0_tuser && s_axis_video1_tuser)
                                      : ((state == ST_CAPTURE)
                                             ? (aclken && s_axis_video0_tvalid)
                                             : 1'b0);

    assign m_axis_video_tdata  = out_data;
    assign m_axis_video_tvalid = out_valid;
    assign m_axis_video_tlast  = out_last;
    assign m_axis_video_tuser  = out_user;
    assign irq = 1'b0;

    // Capture the centre 640 pixels of both input lines. Both inputs are
    // advanced together, so their line boundaries cannot drift apart.
    always @(posedge aclk) begin
        if (accept_sof) begin
            // SOF is pixel zero and lies outside the selected centre crop.
        end else if (accept_pair &&
                     (input_x >= CROP_X) && (input_x < CROP_X + CROP_W)) begin
            line0[input_x - CROP_X] <= s_axis_video0_tdata;
            line1[input_x - CROP_X] <= s_axis_video1_tdata;
        end
    end

    always @(posedge aclk or negedge aresetn) begin
        if (!aresetn) begin
            state     <= ST_WAIT_SOF;
            input_x   <= 11'd0;
            output_x  <= 11'd0;
            output_y  <= 10'd0;
            out_data  <= 24'd0;
            out_valid <= 1'b0;
            out_last  <= 1'b0;
            out_user  <= 1'b0;
        end else if (aclken) begin
            case (state)
                ST_WAIT_SOF: begin
                    out_valid <= 1'b0;
                    out_last  <= 1'b0;
                    out_user  <= 1'b0;
                    input_x   <= 11'd0;

                    if (accept_sof) begin
                        // Pixel 0 was consumed by the SOF handshake.
                        input_x <= 11'd1;
                        state   <= ST_CAPTURE;
                    end
                end

                ST_CAPTURE: begin
                    out_valid <= 1'b0;
                    out_last  <= 1'b0;
                    out_user  <= 1'b0;

                    if (accept_pair) begin
                        if (input_x == IN_W-1) begin
                            // TLAST is checked by the upstream VDMA; use the
                            // configured width as a deterministic safeguard.
                            input_x  <= 11'd0;
                            output_x <= 11'd0;
                            state    <= ST_OUTPUT;
                        end else begin
                            input_x <= input_x + 1'b1;
                        end
                    end
                end

                ST_OUTPUT: begin
                    // Load the first beat, or replace an accepted beat with
                    // the next one. Otherwise hold every AXIS signal stable.
                    if (!out_valid || m_axis_video_tready) begin
                        if (out_valid && out_last) begin
                            out_valid <= 1'b0;
                            out_last  <= 1'b0;
                            out_user  <= 1'b0;
                            output_x  <= 11'd0;

                            if (output_y == OUT_H-1) begin
                                output_y <= 10'd0;
                                state    <= ST_WAIT_SOF;
                            end else begin
                                output_y <= output_y + 1'b1;
                                state    <= ST_CAPTURE;
                            end
                        end else begin
                            out_valid <= 1'b1;
                            out_last  <= (output_x == OUT_W-1);
                            out_user  <= (output_y == 0) && (output_x == 0);

                            if (output_x < CROP_W)
                                out_data <= line0[output_x[9:0]];
                            else
                                out_data <= line1[output_x - CROP_W];

                            if (output_x != OUT_W-1)
                                output_x <= output_x + 1'b1;
                        end
                    end
                end

                default: begin
                    state     <= ST_WAIT_SOF;
                    out_valid <= 1'b0;
                end
            endcase
        end
    end

endmodule
