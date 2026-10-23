`timescale 1ns/1ps
// Independent dual-camera control IP. No AXI, processor, or pixel ports.
// Port cam0 corresponds to ps_i2c0, cam1 to ps_i2c1 in the verified application.
module dual_ov5640_init #(
    parameter integer CLK_HZ = 50000000,
    parameter integer I2C_HZ = 100000,
    parameter integer HW_PHASE_MS = 500,
    parameter integer SW_RESET_MS = 5,
    parameter integer SETTLE_MS = 20,
    parameter integer TIMEOUT_US = 10000,
    parameter integer MAX_RETRIES = 2
)(
    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 clk CLK",
       X_INTERFACE_PARAMETER = "ASSOCIATED_RESET rst_n" *) input wire clk,
    (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 rst_n RST",
       X_INTERFACE_PARAMETER = "POLARITY ACTIVE_LOW" *) input wire rst_n,
    input wire restart,
    inout wire cam0_scl, inout wire cam0_sda,
    inout wire cam1_scl, inout wire cam1_sda,
    output wire cam0_rst_n, output wire cam1_rst_n,
    output wire busy, output wire done,
    output reg [1:0] camera_done, output reg [1:0] camera_error,
    output wire active_camera,
    output reg [9:0] reg_index,
    output reg [15:0] failed_reg,
    output reg [1:0] error_code, output reg [1:0] error_byte
);
    localparam integer MS_CYCLES = CLK_HZ/1000;
    localparam HW_HIGH=0, HW_LOW=1, HW_WAIT=2, LAUNCH=3,
        WAIT_WRITE=4, DELAY_SW=5, DELAY_END=6, FINISHED=7;
    reg [2:0] state;
    reg [31:0] delay_count;
    reg camera;
    reg [7:0] retry_count;
    reg start_write;
    wire [23:0] rom_data;
    wire rom_end;
    wire master_done, master_busy;
    wire [1:0] master_error, master_byte;
    wire scl_low, sda_low;
    wire scl0_i, sda0_i, scl1_i, sda1_i;
    (* ASYNC_REG = "TRUE" *) reg [1:0] reset_sync;
    reg restart_q;
    wire local_reset_n = reset_sync[1] && !(restart && !restart_q);
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin reset_sync<=0; restart_q<=0; end
        else begin reset_sync<={reset_sync[0],1'b1}; restart_q<=restart; end
    end
    // restart is a clk-domain pulse/level; only its rising edge restarts the IP.
    // Explicit pad buffers preserve open-drain behavior when packaged/OOC synthesized.
    IOBUF scl0_buf(.I(1'b0),.T(!(!camera && scl_low)),.O(scl0_i),.IO(cam0_scl));
    IOBUF sda0_buf(.I(1'b0),.T(!(!camera && sda_low)),.O(sda0_i),.IO(cam0_sda));
    IOBUF scl1_buf(.I(1'b0),.T(!( camera && scl_low)),.O(scl1_i),.IO(cam1_scl));
    IOBUF sda1_buf(.I(1'b0),.T(!( camera && sda_low)),.O(sda1_i),.IO(cam1_sda));
    assign cam0_rst_n = local_reset_n && (state != HW_LOW);
    assign cam1_rst_n = cam0_rst_n;
    assign active_camera = camera;
    assign busy = local_reset_n && (state != FINISHED);
    assign done = (&camera_done) && !(|camera_error);
    ov5640_init_rom rom(.index(reg_index), .data(rom_data), .last(rom_end));
    ov5640_sccb_write #(.CLK_HZ(CLK_HZ), .I2C_HZ(I2C_HZ),
        .TIMEOUT_US(TIMEOUT_US)) master (
        .clk(clk), .rst_n(local_reset_n), .start(start_write),
        .word_data({8'h78,rom_data}),
        .scl_i(camera ? scl1_i : scl0_i),
        .sda_i(camera ? sda1_i : sda0_i),
        .scl_low(scl_low), .sda_low(sda_low),
        .busy(master_busy), .done(master_done),
        .error_code(master_error), .error_byte(master_byte));
    always @(posedge clk or negedge local_reset_n) begin
        if (!local_reset_n) begin
            state<=HW_HIGH; delay_count<=0; camera<=0; reg_index<=0;
            retry_count<=0; start_write<=0; camera_done<=0; camera_error<=0;
            failed_reg<=0; error_code<=0; error_byte<=0;
        end else begin
            start_write<=0;
            case(state)
                HW_HIGH, HW_LOW, HW_WAIT: begin
                    if (delay_count >= MS_CYCLES*HW_PHASE_MS-1) begin
                        delay_count<=0;
                        case(state)
                            HW_HIGH: state<=HW_LOW;
                            HW_LOW: state<=HW_WAIT;
                            default: state<=LAUNCH;
                        endcase
                    end else delay_count<=delay_count+1;
                end
                LAUNCH: begin
                    if (rom_end) begin delay_count<=0; state<=DELAY_END; end
                    else begin start_write<=1; state<=WAIT_WRITE; end
                end
                WAIT_WRITE: if (master_done) begin
                    if (master_error!=0) begin
                        if (retry_count < MAX_RETRIES) begin
                            retry_count<=retry_count+1; state<=LAUNCH;
                        end else begin
                            camera_error[camera]<=1;
                            failed_reg<=rom_data[23:8]; error_code<=master_error;
                            error_byte<=master_byte; state<=FINISHED;
                        end
                    end else begin
                        retry_count<=0; reg_index<=reg_index+1;
                        if (reg_index==1) begin delay_count<=0; state<=DELAY_SW; end
                        else state<=LAUNCH;
                    end
                end
                DELAY_SW: begin
                    if (delay_count >= MS_CYCLES*SW_RESET_MS-1) begin
                        delay_count<=0; state<=LAUNCH;
                    end else delay_count<=delay_count+1;
                end
                DELAY_END: begin
                    if (delay_count >= MS_CYCLES*SETTLE_MS-1) begin
                        camera_done[camera]<=1; delay_count<=0;
                        if (!camera) begin camera<=1; reg_index<=0; state<=LAUNCH; end
                        else state<=FINISHED;
                    end else delay_count<=delay_count+1;
                end
                FINISHED: begin end
                default: state<=FINISHED;
            endcase
        end
    end
endmodule
