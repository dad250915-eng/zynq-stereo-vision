`timescale 1ns / 1ps

// Descriptor-driven source rectangle reader.
//
// The PS writes 128-bit tile descriptors into the internal BRAM once after
// loading the rectification maps.  For each descriptor this IP emits two
// AXI4-Stream header beats followed by the packed BGRX source rectangle.
//
// Header beat 0 (TUSER=1): {tile_y, tile_x, source_y0, source_x0}
// Header beat 1 (TUSER=1): {32'd0, source_height, source_width}
// Pixel beats   (TUSER=0): two 32-bit BGRX pixels per 64-bit beat
// TLAST marks the final beat of one rectangle.  An empty rectangle asserts
// TLAST on header beat 1.
module remap_generate_v1_0 #(
    parameter integer C_S_AXI_ADDR_WIDTH = 6,
    parameter integer C_S_AXI_DATA_WIDTH = 32,
    parameter integer C_M_AXI_ADDR_WIDTH = 32
)(
    input  wire                              aclk,
    input  wire                              aresetn,

    input  wire [C_S_AXI_ADDR_WIDTH-1:0]     s_axi_awaddr,
    input  wire                              s_axi_awvalid,
    output wire                              s_axi_awready,
    input  wire [C_S_AXI_DATA_WIDTH-1:0]     s_axi_wdata,
    input  wire [(C_S_AXI_DATA_WIDTH/8)-1:0] s_axi_wstrb,
    input  wire                              s_axi_wvalid,
    output wire                              s_axi_wready,
    output reg  [1:0]                        s_axi_bresp,
    output reg                               s_axi_bvalid,
    input  wire                              s_axi_bready,
    input  wire [C_S_AXI_ADDR_WIDTH-1:0]     s_axi_araddr,
    input  wire                              s_axi_arvalid,
    output wire                              s_axi_arready,
    output reg  [C_S_AXI_DATA_WIDTH-1:0]     s_axi_rdata,
    output reg  [1:0]                        s_axi_rresp,
    output reg                               s_axi_rvalid,
    input  wire                              s_axi_rready,

    output reg  [C_M_AXI_ADDR_WIDTH-1:0]     m_axi_araddr,
    output wire [7:0]                        m_axi_arlen,
    output wire [2:0]                        m_axi_arsize,
    output wire [1:0]                        m_axi_arburst,
    output wire                              m_axi_arlock,
    output wire [3:0]                        m_axi_arcache,
    output wire [2:0]                        m_axi_arprot,
    output wire [3:0]                        m_axi_arqos,
    output reg                               m_axi_arvalid,
    input  wire                              m_axi_arready,
    input  wire [63:0]                       m_axi_rdata,
    input  wire [1:0]                        m_axi_rresp,
    input  wire                              m_axi_rlast,
    input  wire                              m_axi_rvalid,
    output wire                              m_axi_rready,

    output reg  [63:0]                       m_axis_tdata,
    output reg  [7:0]                        m_axis_tkeep,
    output reg                               m_axis_tuser,
    output reg                               m_axis_tlast,
    output reg                               m_axis_tvalid,
    input  wire                              m_axis_tready,

    output wire                              interrupt
);

    localparam [5:0] REG_CONTROL    = 6'h00;
    localparam [5:0] REG_SOURCE     = 6'h04;
    localparam [5:0] REG_STRIDE     = 6'h08;
    localparam [5:0] REG_DESC_BASE  = 6'h0C;
    localparam [5:0] REG_TILE_COUNT = 6'h10;
    localparam [5:0] REG_DESC_INDEX = 6'h14;
    localparam [5:0] REG_DESC_W0    = 6'h18;
    localparam [5:0] REG_DESC_W1    = 6'h1C;
    localparam [5:0] REG_DESC_W2    = 6'h20;
    localparam [5:0] REG_DESC_W3    = 6'h24;
    localparam [5:0] REG_DESC_COMMIT= 6'h28;
    localparam [5:0] REG_STATE      = 6'h2C;
    localparam [5:0] REG_ERROR      = 6'h30;
    localparam [5:0] REG_PROGRESS   = 6'h34;

    reg [31:0] source_base, stride_bytes, descriptor_base, tile_count;
    reg [10:0] descriptor_index;
    reg [31:0] descriptor_w0, descriptor_w1, descriptor_w2, descriptor_w3;
    reg start_pulse, done, busy, error;
    reg [7:0] error_code;

    reg [C_S_AXI_ADDR_WIDTH-1:0] awaddr_hold;
    reg [31:0] wdata_hold;
    reg [3:0] wstrb_hold;
    reg aw_hold_valid, w_hold_valid;
    reg desc_we;
    reg [10:0] desc_waddr;
    reg [127:0] desc_wdata;
    reg [10:0] desc_raddr;
    wire [127:0] desc_rdata;

    assign s_axi_awready = !aw_hold_valid && !s_axi_bvalid;
    assign s_axi_wready  = !w_hold_valid && !s_axi_bvalid;
    assign s_axi_arready = !s_axi_rvalid;
    assign interrupt = done;

    xpm_memory_sdpram #(
        .ADDR_WIDTH_A(11), .ADDR_WIDTH_B(11), .AUTO_SLEEP_TIME(0),
        .BYTE_WRITE_WIDTH_A(128), .CASCADE_HEIGHT(0),
        .CLOCKING_MODE("common_clock"), .ECC_MODE("no_ecc"),
        .MEMORY_INIT_FILE("none"), .MEMORY_INIT_PARAM("0"),
        .MEMORY_OPTIMIZATION("true"), .MEMORY_PRIMITIVE("block"),
        .MEMORY_SIZE(262144), .MESSAGE_CONTROL(0),
        .READ_DATA_WIDTH_B(128), .READ_LATENCY_B(1),
        .READ_RESET_VALUE_B("0"), .RST_MODE_A("SYNC"),
        .RST_MODE_B("SYNC"), .SIM_ASSERT_CHK(0),
        .USE_EMBEDDED_CONSTRAINT(0), .USE_MEM_INIT(0),
        .WAKEUP_TIME("disable_sleep"), .WRITE_DATA_WIDTH_A(128),
        .WRITE_MODE_B("read_first")
    ) descriptor_ram (
        .clka(aclk), .ena(1'b1), .wea(desc_we), .addra(desc_waddr),
        .dina(desc_wdata), .injectdbiterra(1'b0), .injectsbiterra(1'b0),
        .clkb(aclk), .enb(1'b1), .addrb(desc_raddr), .doutb(desc_rdata),
        .rstb(!aresetn), .regceb(1'b1), .sleep(1'b0),
        .dbiterrb(), .sbiterrb()
    );

    integer byte_index;
    always @(posedge aclk) begin
        if (!aresetn) begin
            source_base <= 0; stride_bytes <= 32'd7680;
            descriptor_base <= 0; tile_count <= 32'd900;
            descriptor_index <= 0;
            descriptor_w0 <= 0; descriptor_w1 <= 0;
            descriptor_w2 <= 0; descriptor_w3 <= 0;
            start_pulse <= 0; desc_we <= 0; desc_waddr <= 0; desc_wdata <= 0;
            aw_hold_valid <= 0; w_hold_valid <= 0;
            s_axi_bvalid <= 0; s_axi_bresp <= 0;
            s_axi_rvalid <= 0; s_axi_rresp <= 0; s_axi_rdata <= 0;
        end else begin
            start_pulse <= 0;
            desc_we <= 0;
            if (s_axi_awready && s_axi_awvalid) begin
                awaddr_hold <= s_axi_awaddr; aw_hold_valid <= 1;
            end
            if (s_axi_wready && s_axi_wvalid) begin
                wdata_hold <= s_axi_wdata; wstrb_hold <= s_axi_wstrb;
                w_hold_valid <= 1;
            end
            if (aw_hold_valid && w_hold_valid && !s_axi_bvalid) begin
                case (awaddr_hold[5:0])
                    REG_CONTROL: if (wstrb_hold[0] && wdata_hold[0] && !busy) start_pulse <= 1;
                    REG_SOURCE: for (byte_index=0; byte_index<4; byte_index=byte_index+1)
                        if (wstrb_hold[byte_index]) source_base[byte_index*8 +: 8] <= wdata_hold[byte_index*8 +: 8];
                    REG_STRIDE: for (byte_index=0; byte_index<4; byte_index=byte_index+1)
                        if (wstrb_hold[byte_index]) stride_bytes[byte_index*8 +: 8] <= wdata_hold[byte_index*8 +: 8];
                    REG_DESC_BASE: descriptor_base <= wdata_hold;
                    REG_TILE_COUNT: tile_count <= wdata_hold;
                    REG_DESC_INDEX: descriptor_index <= wdata_hold[10:0];
                    REG_DESC_W0: descriptor_w0 <= wdata_hold;
                    REG_DESC_W1: descriptor_w1 <= wdata_hold;
                    REG_DESC_W2: descriptor_w2 <= wdata_hold;
                    REG_DESC_W3: descriptor_w3 <= wdata_hold;
                    REG_DESC_COMMIT: if (wdata_hold[0]) begin
                        desc_we <= 1; desc_waddr <= descriptor_index;
                        desc_wdata <= {descriptor_w3,descriptor_w2,descriptor_w1,descriptor_w0};
                    end
                    default: ;
                endcase
                aw_hold_valid <= 0; w_hold_valid <= 0;
                s_axi_bvalid <= 1; s_axi_bresp <= 0;
            end
            if (s_axi_bvalid && s_axi_bready) s_axi_bvalid <= 0;
            if (s_axi_arready && s_axi_arvalid) begin
                case (s_axi_araddr[5:0])
                    REG_CONTROL: s_axi_rdata <= {28'd0,error,busy,done,1'b0};
                    REG_SOURCE: s_axi_rdata <= source_base;
                    REG_STRIDE: s_axi_rdata <= stride_bytes;
                    REG_DESC_BASE: s_axi_rdata <= descriptor_base;
                    REG_TILE_COUNT: s_axi_rdata <= tile_count;
                    REG_DESC_INDEX: s_axi_rdata <= {21'd0,descriptor_index};
                    REG_DESC_W0: s_axi_rdata <= descriptor_w0;
                    REG_DESC_W1: s_axi_rdata <= descriptor_w1;
                    REG_DESC_W2: s_axi_rdata <= descriptor_w2;
                    REG_DESC_W3: s_axi_rdata <= descriptor_w3;
                    REG_STATE: s_axi_rdata <= {28'd0,state};
                    REG_ERROR: s_axi_rdata <= {24'd0,error_code};
                    REG_PROGRESS: s_axi_rdata <= tiles_completed;
                    default: s_axi_rdata <= 0;
                endcase
                s_axi_rvalid <= 1; s_axi_rresp <= 0;
            end
            if (s_axi_rvalid && s_axi_rready) s_axi_rvalid <= 0;
        end
    end

    localparam [3:0] ST_IDLE=0, ST_DESC_WAIT=1, ST_DESC_CAPTURE=2,
        ST_HEADER0=3, ST_HEADER1=4, ST_ROW_MUL=5, ST_ROW_ADDR=6,
        ST_BURST=7, ST_AR=8, ST_R=9, ST_NEXT_ROW=10,
        ST_NEXT_TILE=11, ST_FINISH=12;
    reg [3:0] state;
    reg [31:0] cfg_source, cfg_stride, cfg_desc_base, cfg_tile_count;
    reg [31:0] tiles_completed;
    reg [15:0] tile_x, tile_y, source_x0, source_y0;
    reg [15:0] rect_width, rect_height;
    reg [15:0] row_index;
    reg [31:0] row_offset, current_address;
    reg [8:0] beats_remaining, burst_beats, beat_index;
    reg [7:0] axi_arlen_reg;

    assign m_axi_arlen = axi_arlen_reg;
    assign m_axi_arsize = 3'b011;
    assign m_axi_arburst = 2'b01;
    assign m_axi_arlock = 1'b0;
    assign m_axi_arcache = 4'b0011;
    assign m_axi_arprot = 3'b000;
    assign m_axi_arqos = 4'b0000;
    assign m_axi_rready = (state == ST_R) && (!m_axis_tvalid || m_axis_tready);

    function [8:0] choose_burst;
        input [31:0] address;
        input [8:0] remaining;
        reg [9:0] to_boundary;
        begin
            to_boundary = (13'd4096 - {1'b0,address[11:0]}) >> 3;
            if ({1'b0,remaining} < to_boundary)
                choose_burst = (remaining < 9'd8) ? remaining : 9'd8;
            else
                choose_burst = (to_boundary < 10'd8) ? to_boundary[8:0] : 9'd8;
        end
    endfunction

    always @(posedge aclk) begin
        if (!aresetn) begin
            state <= ST_IDLE; done <= 0; busy <= 0; error <= 0; error_code <= 0;
            m_axi_araddr <= 0; axi_arlen_reg <= 0; m_axi_arvalid <= 0;
            m_axis_tdata <= 0; m_axis_tkeep <= 8'hFF; m_axis_tuser <= 0;
            m_axis_tlast <= 0; m_axis_tvalid <= 0;
            desc_raddr <= 0; tiles_completed <= 0; row_index <= 0;
            row_offset <= 0; current_address <= 0;
            beats_remaining <= 0; burst_beats <= 0; beat_index <= 0;
        end else begin
            if (m_axis_tvalid && m_axis_tready) m_axis_tvalid <= 0;
            case (state)
                ST_IDLE: begin
                    m_axi_arvalid <= 0;
                    if (start_pulse) begin
                        done <= 0; busy <= 1; error <= 0; error_code <= 0;
                        cfg_source <= source_base; cfg_stride <= stride_bytes;
                        cfg_desc_base <= descriptor_base; cfg_tile_count <= tile_count;
                        tiles_completed <= 0;
                        desc_raddr <= descriptor_base[10:0];
                        state <= ST_DESC_WAIT;
                    end
                end
                ST_DESC_WAIT: state <= ST_DESC_CAPTURE;
                ST_DESC_CAPTURE: begin
                    source_x0 <= desc_rdata[15:0]; source_y0 <= desc_rdata[31:16];
                    rect_width <= desc_rdata[47:32]; rect_height <= desc_rdata[63:48];
                    tile_x <= desc_rdata[79:64]; tile_y <= desc_rdata[95:80];
                    state <= ST_HEADER0;
                end
                ST_HEADER0: if (!m_axis_tvalid) begin
                    m_axis_tdata <= {tile_y,tile_x,source_y0,source_x0};
                    m_axis_tkeep <= 8'hFF; m_axis_tuser <= 1; m_axis_tlast <= 0;
                    m_axis_tvalid <= 1; state <= ST_HEADER1;
                end
                ST_HEADER1: if (!m_axis_tvalid) begin
                    m_axis_tdata <= {32'd0,rect_height,rect_width};
                    m_axis_tkeep <= 8'hFF; m_axis_tuser <= 1;
                    m_axis_tlast <= (rect_width == 0 || rect_height == 0);
                    m_axis_tvalid <= 1;
                    if (rect_width == 0 || rect_height == 0) state <= ST_NEXT_TILE;
                    else begin row_index <= 0; state <= ST_ROW_MUL; end
                end
                ST_ROW_MUL: begin
                    row_offset <= (source_y0 + row_index) * cfg_stride[15:0];
                    state <= ST_ROW_ADDR;
                end
                ST_ROW_ADDR: begin
                    current_address <= cfg_source + row_offset + (source_x0 << 2);
                    beats_remaining <= rect_width >> 1;
                    state <= ST_BURST;
                end
                ST_BURST: begin
                    burst_beats <= choose_burst(current_address,beats_remaining);
                    m_axi_araddr <= current_address;
                    axi_arlen_reg <= choose_burst(current_address,beats_remaining)-1;
                    m_axi_arvalid <= 1; state <= ST_AR;
                end
                ST_AR: if (m_axi_arvalid && m_axi_arready) begin
                    m_axi_arvalid <= 0; beat_index <= 0; state <= ST_R;
                end
                ST_R: if (m_axi_rvalid && m_axi_rready) begin
                    m_axis_tdata <= m_axi_rdata; m_axis_tkeep <= 8'hFF;
                    m_axis_tuser <= 0;
                    m_axis_tlast <= m_axi_rlast &&
                        (beats_remaining <= burst_beats) &&
                        (row_index + 1 == rect_height);
                    m_axis_tvalid <= 1;
                    if (m_axi_rresp != 0) begin error <= 1; if(error_code==0) error_code<=2; end
                    if (m_axi_rlast) begin
                        if (beat_index + 1 != burst_beats) begin error<=1; if(error_code==0) error_code<=3; end
                        if (beats_remaining > burst_beats) begin
                            beats_remaining <= beats_remaining - burst_beats;
                            current_address <= current_address + burst_beats*8;
                            state <= ST_BURST;
                        end else if (row_index + 1 < rect_height) begin
                            row_index <= row_index + 1; state <= ST_ROW_MUL;
                        end else state <= ST_NEXT_TILE;
                    end else beat_index <= beat_index + 1;
                end
                ST_NEXT_TILE: begin
                    tiles_completed <= tiles_completed + 1;
                    if (tiles_completed + 1 >= cfg_tile_count) state <= ST_FINISH;
                    else begin
                        desc_raddr <= cfg_desc_base[10:0] + tiles_completed[10:0] + 1;
                        state <= ST_DESC_WAIT;
                    end
                end
                ST_FINISH: if (!m_axis_tvalid) begin
                    busy <= 0; done <= 1; state <= ST_IDLE;
                end
                default: begin busy<=0; done<=1; error<=1; error_code<=9; state<=ST_IDLE; end
            endcase
        end
    end
endmodule
