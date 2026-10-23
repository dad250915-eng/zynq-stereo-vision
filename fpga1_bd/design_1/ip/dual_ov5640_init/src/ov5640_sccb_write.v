`timescale 1ns/1ps
// Single-master, four-byte SCCB/I2C write. Open-drain pad controls.
// ACK is checked on every byte. A stuck bus/clock causes a bounded failure.
module ov5640_sccb_write #(
    parameter integer CLK_HZ = 50000000,
    parameter integer I2C_HZ = 100000,
    parameter integer TIMEOUT_US = 10000
)(
    input wire clk, input wire rst_n,
    input wire start, input wire [31:0] word_data,
    input wire scl_i, input wire sda_i,
    output reg scl_low, output reg sda_low,
    output reg busy, output reg done,
    output reg [1:0] error_code, output reg [1:0] error_byte
);
    localparam integer Q = (CLK_HZ + 4*I2C_HZ-1)/(4*I2C_HZ);
    localparam integer LIMIT = (CLK_HZ/1000000)*TIMEOUT_US;
    localparam IDLE=0, BUS_FREE=1, START_HOLD=2, BIT_LOW=3,
        BIT_RISE=4, BIT_HIGH=5, BIT_FALL=6, ACK_LOW=7,
        ACK_RISE=8, ACK_HIGH=9, ACK_FALL=10, STOP_LOW=11,
        STOP_RISE=12, STOP_HIGH=13, STOP_FREE=14;
    reg [3:0] state;
    reg [31:0] div_count, timeout_count, shift;
    reg [2:0] bit_count;
    reg [1:0] byte_count;
    (* ASYNC_REG = "TRUE" *) reg [1:0] scl_sync, sda_sync;
    wire waiting = ((state == BUS_FREE) && !(scl_sync[1] && sda_sync[1])) ||
        (((state == BIT_HIGH) || (state == ACK_HIGH) ||
          (state == STOP_HIGH)) && !scl_sync[1]);
    wire long_phase = (state==BUS_FREE) || (state==START_HOLD) ||
        (state==BIT_HIGH) || (state==ACK_HIGH) ||
        (state==STOP_HIGH) || (state==STOP_FREE);
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin scl_sync<=2'b11; sda_sync<=2'b11; end
        else begin scl_sync<={scl_sync[0],scl_i}; sda_sync<={sda_sync[0],sda_i}; end
    end
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state<=IDLE; div_count<=0; timeout_count<=0; shift<=0;
            bit_count<=0; byte_count<=0; scl_low<=0; sda_low<=0;
            busy<=0; done<=0; error_code<=0; error_byte<=0;
        end else begin
            done<=0;
            if (state == IDLE) begin
                div_count<=0; timeout_count<=0;
                if (start) begin
                    shift<=word_data; bit_count<=0; byte_count<=0;
                    error_code<=0; error_byte<=0; busy<=1;
                    scl_low<=0; sda_low<=0; state<=BUS_FREE;
                end
            end else if (waiting) begin
                div_count<=0;
                if (timeout_count >= LIMIT-1) begin
                    // Release both lines; a stuck physical bus cannot guarantee STOP.
                    scl_low<=0; sda_low<=0; busy<=0; done<=1;
                    error_code<=2; error_byte<=byte_count; state<=IDLE;
                    timeout_count<=0;
                end else timeout_count<=timeout_count+1;
            end else begin
                timeout_count<=0;
                if (div_count == (long_phase ? 2*Q-1 : Q-1)) begin
                    div_count<=0;
                    case(state)
                        BUS_FREE: begin sda_low<=1; state<=START_HOLD; end
                        START_HOLD: begin scl_low<=1; state<=BIT_RISE; end
                        BIT_RISE: begin sda_low<=!shift[31]; state<=BIT_LOW; end
                        BIT_LOW: begin scl_low<=0; state<=BIT_HIGH; end
                        BIT_HIGH: begin scl_low<=1; state<=BIT_FALL; end
                        BIT_FALL: begin
                            shift<={shift[30:0],1'b0};
                            if (bit_count==7) begin bit_count<=0; sda_low<=0; state<=ACK_LOW; end
                            else begin bit_count<=bit_count+1; sda_low<=!shift[30]; state<=BIT_LOW; end
                        end
                        ACK_LOW: begin scl_low<=0; state<=ACK_HIGH; end
                        ACK_HIGH: begin
                            scl_low<=1;
                            if (sda_sync[1]) begin error_code<=1; error_byte<=byte_count; end
                            state<=ACK_FALL;
                        end
                        ACK_FALL: begin
                            if ((error_code!=0) || (byte_count==3)) begin sda_low<=1; state<=STOP_LOW; end
                            else begin byte_count<=byte_count+1; sda_low<=!shift[31]; state<=BIT_LOW; end
                        end
                        STOP_LOW: begin scl_low<=0; state<=STOP_HIGH; end
                        STOP_HIGH: begin sda_low<=0; state<=STOP_FREE; end
                        STOP_FREE: begin busy<=0; done<=1; state<=IDLE; end
                        default: begin state<=IDLE; busy<=0; scl_low<=0; sda_low<=0; end
                    endcase
                end else div_count<=div_count+1;
            end
        end
    end
endmodule
