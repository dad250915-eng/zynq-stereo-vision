`timescale 1ns / 1ps

// Pipelined two-bank nearest-neighbour rectification engine.
//
// IP1 supplies one source bounding rectangle per 64x16 output tile through
// S_AXIS_SOURCE.  This block independently loads the corresponding dense map
// tile, runs an initiation-interval-one pixel pipeline, and drains completed
// output tiles through the independent AXI write channels.
module remap_operate_pipeline_v1_0 #(
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

    output reg  [C_M_AXI_ADDR_WIDTH-1:0]     m_axi_awaddr,
    output wire [7:0]                        m_axi_awlen,
    output wire [2:0]                        m_axi_awsize,
    output wire [1:0]                        m_axi_awburst,
    output wire                              m_axi_awlock,
    output wire [3:0]                        m_axi_awcache,
    output wire [2:0]                        m_axi_awprot,
    output wire [3:0]                        m_axi_awqos,
    output reg                               m_axi_awvalid,
    input  wire                              m_axi_awready,
    output reg  [63:0]                       m_axi_wdata,
    output wire [7:0]                        m_axi_wstrb,
    output wire                              m_axi_wlast,
    output reg                               m_axi_wvalid,
    input  wire                              m_axi_wready,
    input  wire [1:0]                        m_axi_bresp,
    input  wire                              m_axi_bvalid,
    output reg                               m_axi_bready,
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
    output reg                               m_axi_rready,

    input  wire [63:0]                       s_axis_source_tdata,
    input  wire [7:0]                        s_axis_source_tkeep,
    input  wire                              s_axis_source_tuser,
    input  wire                              s_axis_source_tlast,
    input  wire                              s_axis_source_tvalid,
    output wire                              s_axis_source_tready,
    output wire                              interrupt
);

    localparam integer TILE_W = 64;
    localparam integer TILE_H = 16;
    localparam integer TILE_PIXELS = 1024;
    localparam integer SOURCE_BUFFER_PIXELS = 4096;

    localparam [5:0] REG_CONTROL = 6'h00;
    localparam [5:0] REG_SOURCE  = 6'h04;
    localparam [5:0] REG_MAP     = 6'h08;
    localparam [5:0] REG_DEST    = 6'h0C;
    localparam [5:0] REG_WIDTH   = 6'h10;
    localparam [5:0] REG_HEIGHT  = 6'h14;
    localparam [5:0] REG_STRIDE  = 6'h18;
    localparam [5:0] REG_STATE   = 6'h1C;
    localparam [5:0] REG_ERROR   = 6'h20;
    localparam [5:0] REG_TILE_X  = 6'h24;
    localparam [5:0] REG_TILE_Y  = 6'h28;
    localparam [5:0] REG_MAP_DBG = 6'h2C;
    localparam [5:0] REG_SRC_DBG = 6'h30;
    localparam [5:0] REG_OUT_DBG = 6'h34;
    localparam [5:0] REG_AXI_DBG = 6'h38;

    reg [31:0] source_base, map_base, destination_base;
    reg [31:0] active_width, active_height, stride_bytes;
    reg [31:0] cfg_map, cfg_destination, cfg_width, cfg_height, cfg_stride;
    reg start_pulse, done, busy, error;
    reg [7:0] error_code;
    reg [4:0] state;
    reg [31:0] tile_x, tile_y;

    reg [C_S_AXI_ADDR_WIDTH-1:0] awaddr_hold;
    reg [31:0] wdata_hold;
    reg [3:0] wstrb_hold;
    reg aw_hold_valid, w_hold_valid;
    integer byte_index;

    wire config_valid = (active_width != 0) && (active_height != 0) &&
                        (active_width[5:0] == 0) &&
                        (active_height[3:0] == 0) &&
                        (stride_bytes[2:0] == 0) &&
                        ((stride_bytes >> 2) >= active_width);
    wire start_accept = start_pulse && config_valid;

    assign s_axi_awready = !aw_hold_valid && !s_axi_bvalid;
    assign s_axi_wready  = !w_hold_valid && !s_axi_bvalid;
    assign s_axi_arready = !s_axi_rvalid;
    assign interrupt = done;

    reg [7:0] axi_awlen_reg, axi_arlen_reg;
    reg axi_wlast_reg;
    assign m_axi_awlen = axi_awlen_reg;
    assign m_axi_awsize = 3'b011;
    assign m_axi_awburst = 2'b01;
    assign m_axi_awlock = 1'b0;
    assign m_axi_awcache = 4'b0011;
    assign m_axi_awprot = 3'b000;
    assign m_axi_awqos = 4'b0000;
    assign m_axi_wstrb = 8'hFF;
    assign m_axi_wlast = axi_wlast_reg;
    assign m_axi_arlen = axi_arlen_reg;
    assign m_axi_arsize = 3'b011;
    assign m_axi_arburst = 2'b01;
    assign m_axi_arlock = 1'b0;
    assign m_axi_arcache = 4'b0011;
    assign m_axi_arprot = 3'b000;
    assign m_axi_arqos = 4'b0000;

    // Debug counters retain the existing software register layout.
    reg [4:0] map_row;
    reg [2:0] map_segment;
    reg [5:0] map_beat;
    reg [10:0] rx_store_addr;
    reg [1:0] rx_state;
    reg [4:0] output_row;
    reg [2:0] output_segment;
    reg [5:0] output_beat;
    reg [31:0] tiles_written;

    // Controller error events are made sticky by the status block.
    reg map_error_pulse, write_error_pulse, stream_error_pulse;
    reg writer_done_pulse;

    always @(posedge aclk) begin
        if (!aresetn) begin
            source_base <= 0; map_base <= 0; destination_base <= 0;
            active_width <= 1280; active_height <= 720; stride_bytes <= 7680;
            start_pulse <= 0; aw_hold_valid <= 0; w_hold_valid <= 0;
            s_axi_bvalid <= 0; s_axi_bresp <= 0;
            s_axi_rvalid <= 0; s_axi_rresp <= 0; s_axi_rdata <= 0;
        end else begin
            start_pulse <= 0;
            if (s_axi_awready && s_axi_awvalid) begin
                awaddr_hold <= s_axi_awaddr;
                aw_hold_valid <= 1;
            end
            if (s_axi_wready && s_axi_wvalid) begin
                wdata_hold <= s_axi_wdata;
                wstrb_hold <= s_axi_wstrb;
                w_hold_valid <= 1;
            end
            if (aw_hold_valid && w_hold_valid && !s_axi_bvalid) begin
                case (awaddr_hold[5:0])
                    REG_CONTROL: if (wstrb_hold[0] && wdata_hold[0] && !busy)
                        start_pulse <= 1;
                    REG_SOURCE: for (byte_index=0; byte_index<4; byte_index=byte_index+1)
                        if (wstrb_hold[byte_index]) source_base[byte_index*8 +: 8] <= wdata_hold[byte_index*8 +: 8];
                    REG_MAP: for (byte_index=0; byte_index<4; byte_index=byte_index+1)
                        if (wstrb_hold[byte_index]) map_base[byte_index*8 +: 8] <= wdata_hold[byte_index*8 +: 8];
                    REG_DEST: for (byte_index=0; byte_index<4; byte_index=byte_index+1)
                        if (wstrb_hold[byte_index]) destination_base[byte_index*8 +: 8] <= wdata_hold[byte_index*8 +: 8];
                    REG_WIDTH: for (byte_index=0; byte_index<4; byte_index=byte_index+1)
                        if (wstrb_hold[byte_index]) active_width[byte_index*8 +: 8] <= wdata_hold[byte_index*8 +: 8];
                    REG_HEIGHT: for (byte_index=0; byte_index<4; byte_index=byte_index+1)
                        if (wstrb_hold[byte_index]) active_height[byte_index*8 +: 8] <= wdata_hold[byte_index*8 +: 8];
                    REG_STRIDE: for (byte_index=0; byte_index<4; byte_index=byte_index+1)
                        if (wstrb_hold[byte_index]) stride_bytes[byte_index*8 +: 8] <= wdata_hold[byte_index*8 +: 8];
                    default: ;
                endcase
                aw_hold_valid <= 0;
                w_hold_valid <= 0;
                s_axi_bvalid <= 1;
                s_axi_bresp <= 0;
            end
            if (s_axi_bvalid && s_axi_bready) s_axi_bvalid <= 0;

            if (s_axi_arready && s_axi_arvalid) begin
                case (s_axi_araddr[5:0])
                    REG_CONTROL: s_axi_rdata <= {28'd0,error,busy,done,1'b0};
                    REG_SOURCE:  s_axi_rdata <= source_base;
                    REG_MAP:     s_axi_rdata <= map_base;
                    REG_DEST:    s_axi_rdata <= destination_base;
                    REG_WIDTH:   s_axi_rdata <= active_width;
                    REG_HEIGHT:  s_axi_rdata <= active_height;
                    REG_STRIDE:  s_axi_rdata <= stride_bytes;
                    REG_STATE:   s_axi_rdata <= {27'd0,state};
                    REG_ERROR:   s_axi_rdata <= {24'd0,error_code};
                    REG_TILE_X:  s_axi_rdata <= tile_x;
                    REG_TILE_Y:  s_axi_rdata <= tile_y;
                    REG_MAP_DBG: s_axi_rdata <= {18'd0,map_row,map_segment,map_beat[5:0]};
                    REG_SRC_DBG: s_axi_rdata <= {18'd0,rx_state,2'b0,rx_store_addr};
                    REG_OUT_DBG: s_axi_rdata <= {8'd0,tiles_written[9:0],output_row,output_segment,output_beat[3:0]};
                    REG_AXI_DBG: s_axi_rdata <= {24'd0,m_axi_bresp,m_axi_rresp,m_axi_bvalid,m_axi_rvalid,m_axi_awvalid,m_axi_arvalid};
                    default: s_axi_rdata <= 0;
                endcase
                s_axi_rvalid <= 1;
                s_axi_rresp <= 0;
            end
            if (s_axi_rvalid && s_axi_rready) s_axi_rvalid <= 0;
        end
    end

    // Global frame status.  The three datapath controllers run independently.
    always @(posedge aclk) begin
        if (!aresetn) begin
            cfg_map <= 0; cfg_destination <= 0; cfg_width <= 0;
            cfg_height <= 0; cfg_stride <= 0;
            done <= 0; busy <= 0; error <= 0; error_code <= 0; state <= 0;
        end else if (start_pulse) begin
            done <= !config_valid;
            busy <= config_valid;
            error <= !config_valid;
            error_code <= config_valid ? 8'd0 : 8'd1;
            state <= config_valid ? 5'd1 : 5'd31;
            if (config_valid) begin
                cfg_map <= map_base;
                cfg_destination <= destination_base;
                cfg_width <= active_width;
                cfg_height <= active_height;
                cfg_stride <= stride_bytes;
            end
        end else begin
            if (map_error_pulse && !error) begin error<=1; error_code<=8'd2; end
            if (write_error_pulse && !error) begin error<=1; error_code<=8'd8; end
            if (stream_error_pulse && !error) begin error<=1; error_code<=8'd10; end
            if (writer_done_pulse) begin
                busy <= 0;
                done <= 1;
                state <= error ? 5'd31 : 5'd0;
            end
        end
    end

    // ---------------------------------------------------------------------
    // Source stream receiver and two asymmetric source BRAM banks.
    // ---------------------------------------------------------------------
    reg write_bank;
    reg [1:0] source_full;
    reg [1:0] source_release;
    reg header_event;
    reg header_bank;
    reg [15:0] bank_x0[0:1], bank_y0[0:1];
    reg [15:0] bank_w[0:1], bank_h[0:1];
    reg [15:0] bank_tile_x[0:1], bank_tile_y[0:1];

    wire source_handshake = s_axis_source_tvalid && s_axis_source_tready;
    wire source_pixel = source_handshake && !s_axis_source_tuser && (rx_state==2);
    wire source0_we = source_pixel && !write_bank;
    wire source1_we = source_pixel && write_bank;
    assign s_axis_source_tready = busy &&
        ((rx_state != 0) || !source_full[write_bank]);

    reg process_bank;
    reg [11:0] source_read_addr;
    wire [31:0] source0_rdata, source1_rdata;
    wire [31:0] selected_source_data = process_bank ? source1_rdata : source0_rdata;

    xpm_memory_sdpram #(
        .ADDR_WIDTH_A(11), .ADDR_WIDTH_B(12), .BYTE_WRITE_WIDTH_A(64),
        .CLOCKING_MODE("common_clock"), .ECC_MODE("no_ecc"),
        .MEMORY_INIT_FILE("none"), .MEMORY_INIT_PARAM("0"),
        .MEMORY_OPTIMIZATION("true"), .MEMORY_PRIMITIVE("block"),
        .MEMORY_SIZE(131072), .READ_DATA_WIDTH_B(32), .READ_LATENCY_B(1),
        .READ_RESET_VALUE_B("0"), .RST_MODE_A("SYNC"), .RST_MODE_B("SYNC"),
        .USE_EMBEDDED_CONSTRAINT(0), .USE_MEM_INIT(0),
        .WRITE_DATA_WIDTH_A(64), .WRITE_MODE_B("read_first")
    ) source_ram0 (
        .clka(aclk),.ena(1'b1),.wea(source0_we),.addra(rx_store_addr),.dina(s_axis_source_tdata),
        .injectdbiterra(1'b0),.injectsbiterra(1'b0),
        .clkb(aclk),.enb(1'b1),.addrb(source_read_addr),.doutb(source0_rdata),
        .rstb(!aresetn),.regceb(1'b1),.sleep(1'b0),.dbiterrb(),.sbiterrb()
    );
    xpm_memory_sdpram #(
        .ADDR_WIDTH_A(11), .ADDR_WIDTH_B(12), .BYTE_WRITE_WIDTH_A(64),
        .CLOCKING_MODE("common_clock"), .ECC_MODE("no_ecc"),
        .MEMORY_INIT_FILE("none"), .MEMORY_INIT_PARAM("0"),
        .MEMORY_OPTIMIZATION("true"), .MEMORY_PRIMITIVE("block"),
        .MEMORY_SIZE(131072), .READ_DATA_WIDTH_B(32), .READ_LATENCY_B(1),
        .READ_RESET_VALUE_B("0"), .RST_MODE_A("SYNC"), .RST_MODE_B("SYNC"),
        .USE_EMBEDDED_CONSTRAINT(0), .USE_MEM_INIT(0),
        .WRITE_DATA_WIDTH_A(64), .WRITE_MODE_B("read_first")
    ) source_ram1 (
        .clka(aclk),.ena(1'b1),.wea(source1_we),.addra(rx_store_addr),.dina(s_axis_source_tdata),
        .injectdbiterra(1'b0),.injectsbiterra(1'b0),
        .clkb(aclk),.enb(1'b1),.addrb(source_read_addr),.doutb(source1_rdata),
        .rstb(!aresetn),.regceb(1'b1),.sleep(1'b0),.dbiterrb(),.sbiterrb()
    );

    always @(posedge aclk) begin
        if (!aresetn || start_pulse) begin
            write_bank<=0; source_full<=0; rx_state<=0; rx_store_addr<=0;
            header_event<=0; header_bank<=0; stream_error_pulse<=0;
        end else begin
            header_event<=0; stream_error_pulse<=0;
            if (source_release[0]) source_full[0]<=0;
            if (source_release[1]) source_full[1]<=0;
            if (source_handshake) begin
                case (rx_state)
                    0: begin
                        if (!s_axis_source_tuser) stream_error_pulse<=1;
                        bank_x0[write_bank]<=s_axis_source_tdata[15:0];
                        bank_y0[write_bank]<=s_axis_source_tdata[31:16];
                        bank_tile_x[write_bank]<=s_axis_source_tdata[47:32];
                        bank_tile_y[write_bank]<=s_axis_source_tdata[63:48];
                        header_event<=1; header_bank<=write_bank; rx_state<=1;
                    end
                    1: begin
                        if (!s_axis_source_tuser) stream_error_pulse<=1;
                        bank_w[write_bank]<=s_axis_source_tdata[15:0];
                        bank_h[write_bank]<=s_axis_source_tdata[31:16];
                        rx_store_addr<=0;
                        if (s_axis_source_tlast || s_axis_source_tdata[31:0]==0) begin
                            source_full[write_bank]<=1;
                            write_bank<=~write_bank; rx_state<=0;
                        end else rx_state<=2;
                    end
                    2: begin
                        if (s_axis_source_tuser || s_axis_source_tkeep!=8'hFF)
                            stream_error_pulse<=1;
                        if (s_axis_source_tlast) begin
                            source_full[write_bank]<=1;
                            write_bank<=~write_bank; rx_state<=0;
                        end else rx_store_addr<=rx_store_addr+1;
                    end
                    default: rx_state<=0;
                endcase
            end
        end
    end

    // ---------------------------------------------------------------------
    // Two map BRAMs.  The read controller fills one while the compute
    // pipeline consumes the other.
    // ---------------------------------------------------------------------
    reg [1:0] map_pending, map_full, map_release;
    reg map_load_bank;
    reg [31:0] map_row_product;
    reg map0_we, map1_we;
    reg [8:0] map_write_addr;
    reg [63:0] map_write_data;
    reg [9:0] map_read_addr;
    wire [31:0] map0_rdata, map1_rdata;
    wire [31:0] selected_map_data = process_bank ? map1_rdata : map0_rdata;

    xpm_memory_sdpram #(
        .ADDR_WIDTH_A(9),.ADDR_WIDTH_B(10),.BYTE_WRITE_WIDTH_A(64),
        .CLOCKING_MODE("common_clock"),.ECC_MODE("no_ecc"),
        .MEMORY_INIT_FILE("none"),.MEMORY_INIT_PARAM("0"),
        .MEMORY_OPTIMIZATION("true"),.MEMORY_PRIMITIVE("block"),
        .MEMORY_SIZE(32768),.READ_DATA_WIDTH_B(32),.READ_LATENCY_B(1),
        .READ_RESET_VALUE_B("0"),.RST_MODE_A("SYNC"),.RST_MODE_B("SYNC"),
        .USE_EMBEDDED_CONSTRAINT(0),.USE_MEM_INIT(0),
        .WRITE_DATA_WIDTH_A(64),.WRITE_MODE_B("read_first")
    ) map_ram0 (
        .clka(aclk),.ena(1'b1),.wea(map0_we),.addra(map_write_addr),.dina(map_write_data),
        .injectdbiterra(1'b0),.injectsbiterra(1'b0),
        .clkb(aclk),.enb(1'b1),.addrb(map_read_addr),.doutb(map0_rdata),
        .rstb(!aresetn),.regceb(1'b1),.sleep(1'b0),.dbiterrb(),.sbiterrb()
    );
    xpm_memory_sdpram #(
        .ADDR_WIDTH_A(9),.ADDR_WIDTH_B(10),.BYTE_WRITE_WIDTH_A(64),
        .CLOCKING_MODE("common_clock"),.ECC_MODE("no_ecc"),
        .MEMORY_INIT_FILE("none"),.MEMORY_INIT_PARAM("0"),
        .MEMORY_OPTIMIZATION("true"),.MEMORY_PRIMITIVE("block"),
        .MEMORY_SIZE(32768),.READ_DATA_WIDTH_B(32),.READ_LATENCY_B(1),
        .READ_RESET_VALUE_B("0"),.RST_MODE_A("SYNC"),.RST_MODE_B("SYNC"),
        .USE_EMBEDDED_CONSTRAINT(0),.USE_MEM_INIT(0),
        .WRITE_DATA_WIDTH_A(64),.WRITE_MODE_B("read_first")
    ) map_ram1 (
        .clka(aclk),.ena(1'b1),.wea(map1_we),.addra(map_write_addr),.dina(map_write_data),
        .injectdbiterra(1'b0),.injectsbiterra(1'b0),
        .clkb(aclk),.enb(1'b1),.addrb(map_read_addr),.doutb(map1_rdata),
        .rstb(!aresetn),.regceb(1'b1),.sleep(1'b0),.dbiterrb(),.sbiterrb()
    );

    localparam [2:0] MR_IDLE=0, MR_ROW_MUL=1, MR_ADDR=2,
                     MR_AR=3, MR_DATA=4;
    reg [2:0] map_state;

    always @(posedge aclk) begin
        if (!aresetn || start_pulse) begin
            map_pending<=0; map_full<=0; map_state<=MR_IDLE;
            map_load_bank<=0; map_row<=0; map_segment<=0; map_beat<=0;
            map0_we<=0; map1_we<=0; map_write_addr<=0; map_write_data<=0;
            m_axi_araddr<=0; axi_arlen_reg<=0; m_axi_arvalid<=0;
            m_axi_rready<=0; map_error_pulse<=0;
        end else begin
            map0_we<=0; map1_we<=0; map_error_pulse<=0;
            if (header_event) map_pending[header_bank]<=1;
            if (map_release[0]) map_full[0]<=0;
            if (map_release[1]) map_full[1]<=0;
            case (map_state)
                MR_IDLE: begin
                    m_axi_arvalid<=0; m_axi_rready<=0;
                    if (map_pending[0]) begin
                        map_load_bank<=0; map_pending[0]<=0;
                        map_row<=0; map_segment<=0; map_state<=MR_ROW_MUL;
                    end else if (map_pending[1]) begin
                        map_load_bank<=1; map_pending[1]<=0;
                        map_row<=0; map_segment<=0; map_state<=MR_ROW_MUL;
                    end
                end
                MR_ROW_MUL: begin
                    map_row_product <=
                        (bank_tile_y[map_load_bank] + map_row) * cfg_width;
                    map_state<=MR_ADDR;
                end
                MR_ADDR: begin
                    m_axi_araddr <= cfg_map +
                        ((map_row_product + bank_tile_x[map_load_bank]) << 2) +
                        (map_segment << 6);
                    axi_arlen_reg<=7;
                    m_axi_arvalid<=1;
                    map_state<=MR_AR;
                end
                MR_AR: if (m_axi_arvalid && m_axi_arready) begin
                    m_axi_arvalid<=0; m_axi_rready<=1; map_beat<=0;
                    map_state<=MR_DATA;
                end
                MR_DATA: if (m_axi_rvalid && m_axi_rready) begin
                    map_write_addr<=map_row*32 + map_segment*8 + map_beat;
                    map_write_data<=m_axi_rdata;
                    if (map_load_bank) map1_we<=1; else map0_we<=1;
                    if (m_axi_rresp!=0) map_error_pulse<=1;
                    if (m_axi_rlast) begin
                        m_axi_rready<=0;
                        if (map_beat!=7) map_error_pulse<=1;
                        if (map_segment!=3) begin
                            map_segment<=map_segment+1; map_state<=MR_ADDR;
                        end else if (map_row!=TILE_H-1) begin
                            map_row<=map_row+1; map_segment<=0; map_state<=MR_ROW_MUL;
                        end else begin
                            map_full[map_load_bank]<=1; map_state<=MR_IDLE;
                        end
                    end else map_beat<=map_beat+1;
                end
                default: map_state<=MR_IDLE;
            endcase
        end
    end

    // ---------------------------------------------------------------------
    // Two output BRAMs.  The compute side writes 32 bits every clock; the
    // writer side reads packed 64-bit beats.
    // ---------------------------------------------------------------------
    reg [1:0] output_full, output_release;
    reg compute_output_bank;
    reg [9:0] pipe_output_addr;
    reg [31:0] pipe_output_data;
    reg pipe_output_we;
    reg [8:0] output_read_addr;
    wire [63:0] output0_rdata, output1_rdata;
    reg writer_bank;
    wire [63:0] selected_output_data = writer_bank ? output1_rdata : output0_rdata;
    reg [15:0] output_tile_x[0:1], output_tile_y[0:1];

    xpm_memory_sdpram #(
        .ADDR_WIDTH_A(10),.ADDR_WIDTH_B(9),.BYTE_WRITE_WIDTH_A(32),
        .CLOCKING_MODE("common_clock"),.ECC_MODE("no_ecc"),
        .MEMORY_INIT_FILE("none"),.MEMORY_INIT_PARAM("0"),
        .MEMORY_OPTIMIZATION("true"),.MEMORY_PRIMITIVE("block"),
        .MEMORY_SIZE(32768),.READ_DATA_WIDTH_B(64),.READ_LATENCY_B(1),
        .READ_RESET_VALUE_B("0"),.RST_MODE_A("SYNC"),.RST_MODE_B("SYNC"),
        .USE_EMBEDDED_CONSTRAINT(0),.USE_MEM_INIT(0),
        .WRITE_DATA_WIDTH_A(32),.WRITE_MODE_B("read_first")
    ) output_ram0 (
        .clka(aclk),.ena(1'b1),.wea(pipe_output_we && !compute_output_bank),
        .addra(pipe_output_addr),.dina(pipe_output_data),
        .injectdbiterra(1'b0),.injectsbiterra(1'b0),
        .clkb(aclk),.enb(1'b1),.addrb(output_read_addr),.doutb(output0_rdata),
        .rstb(!aresetn),.regceb(1'b1),.sleep(1'b0),.dbiterrb(),.sbiterrb()
    );
    xpm_memory_sdpram #(
        .ADDR_WIDTH_A(10),.ADDR_WIDTH_B(9),.BYTE_WRITE_WIDTH_A(32),
        .CLOCKING_MODE("common_clock"),.ECC_MODE("no_ecc"),
        .MEMORY_INIT_FILE("none"),.MEMORY_INIT_PARAM("0"),
        .MEMORY_OPTIMIZATION("true"),.MEMORY_PRIMITIVE("block"),
        .MEMORY_SIZE(32768),.READ_DATA_WIDTH_B(64),.READ_LATENCY_B(1),
        .READ_RESET_VALUE_B("0"),.RST_MODE_A("SYNC"),.RST_MODE_B("SYNC"),
        .USE_EMBEDDED_CONSTRAINT(0),.USE_MEM_INIT(0),
        .WRITE_DATA_WIDTH_A(32),.WRITE_MODE_B("read_first")
    ) output_ram1 (
        .clka(aclk),.ena(1'b1),.wea(pipe_output_we && compute_output_bank),
        .addra(pipe_output_addr),.dina(pipe_output_data),
        .injectdbiterra(1'b0),.injectsbiterra(1'b0),
        .clkb(aclk),.enb(1'b1),.addrb(output_read_addr),.doutb(output1_rdata),
        .rstb(!aresetn),.regceb(1'b1),.sleep(1'b0),.dbiterrb(),.sbiterrb()
    );

    // ---------------------------------------------------------------------
    // Initiation-interval-one pixel pipeline.
    // ---------------------------------------------------------------------
    localparam [1:0] CP_IDLE=0, CP_RUN=1, CP_DRAIN=2;
    reg [1:0] compute_state;
    reg next_process_bank, next_output_bank;
    reg [10:0] issue_index;
    reg issue_valid_d, issue_valid_d2;
    reg [9:0] issue_index_d, issue_index_d2;
    reg s1_valid, s2_valid, s3_valid, s4_valid, s5_valid;
    reg [9:0] s1_index,s2_index,s3_index,s4_index,s5_index;
    reg [31:0] s1_map;
    reg s2_inside,s3_inside,s4_inside,s5_inside;
    reg [15:0] s2_dx,s2_dy,s3_dx;
    reg [31:0] s3_row_offset;
    reg [15:0] proc_x0,proc_y0,proc_x1,proc_y1,proc_w;

    wire issue_enable = (compute_state==CP_RUN) && (issue_index<TILE_PIXELS);

    always @(posedge aclk) begin
        if (!aresetn || start_pulse) begin
            compute_state<=CP_IDLE; next_process_bank<=0; next_output_bank<=0;
            process_bank<=0; compute_output_bank<=0; output_full<=0;
            source_release<=0; map_release<=0; pipe_output_we<=0;
            map_read_addr<=0; source_read_addr<=0; issue_index<=0;
            issue_valid_d<=0; issue_valid_d2<=0;
            s1_valid<=0; s2_valid<=0; s3_valid<=0;
            s4_valid<=0; s5_valid<=0;
            tile_x<=0; tile_y<=0;
        end else begin
            source_release<=0; map_release<=0; pipe_output_we<=0;

            // BRAM map return alignment and all arithmetic pipeline stages.
            issue_valid_d <= issue_enable;
            if (issue_enable) begin
                issue_index_d <= issue_index[9:0];
                map_read_addr <= issue_index[9:0];
                issue_index <= issue_index + 1;
                if (issue_index==TILE_PIXELS-1) compute_state<=CP_DRAIN;
            end
            // XPM READ_LATENCY_B=1 still requires a full clock between the
            // registered address update and sampling doutb in this process.
            // Keep the map index/valid beside the request for those two edges.
            issue_valid_d2 <= issue_valid_d;
            if (issue_valid_d) issue_index_d2 <= issue_index_d;
            s1_valid <= issue_valid_d2;
            if (issue_valid_d2) begin
                s1_map<=selected_map_data;
                s1_index<=issue_index_d2;
            end
            s2_valid <= s1_valid;
            if (s1_valid) begin
                s2_index<=s1_index;
                s2_inside <= (s1_map!=32'hFFFFFFFF) &&
                    (s1_map[15:0]>=proc_x0) && (s1_map[15:0]<=proc_x1) &&
                    (s1_map[31:16]>=proc_y0) && (s1_map[31:16]<=proc_y1);
                s2_dx<=s1_map[15:0]-proc_x0;
                s2_dy<=s1_map[31:16]-proc_y0;
            end
            s3_valid<=s2_valid;
            if (s2_valid) begin
                s3_index<=s2_index; s3_inside<=s2_inside; s3_dx<=s2_dx;
                s3_row_offset<=s2_dy*proc_w;
            end
            s4_valid<=s3_valid;
            if (s3_valid) begin
                s4_index<=s3_index; s4_inside<=s3_inside;
                source_read_addr<=s3_row_offset+s3_dx;
            end
            s5_valid<=s4_valid;
            if (s4_valid) begin s5_index<=s4_index; s5_inside<=s4_inside; end
            // source_read_addr is registered in the s3 stage.  s4/s5 are the
            // request and BRAM-return alignment stages; adding another stage
            // would pair pixel N with the source data for a neighbouring map.
            if (s5_valid) begin
                pipe_output_we<=1;
                pipe_output_addr<=s5_index;
                pipe_output_data<=s5_inside ? selected_source_data : 32'd0;
                if (s5_index==TILE_PIXELS-1) begin
                    output_full[compute_output_bank]<=1;
                    output_tile_x[compute_output_bank]<=bank_tile_x[process_bank];
                    output_tile_y[compute_output_bank]<=bank_tile_y[process_bank];
                    source_release[process_bank]<=1;
                    map_release[process_bank]<=1;
                    next_process_bank<=~next_process_bank;
                    next_output_bank<=~next_output_bank;
                    compute_state<=CP_IDLE;
                end
            end
            if (output_release[0]) output_full[0]<=0;
            if (output_release[1]) output_full[1]<=0;

            if (compute_state==CP_IDLE && source_full[next_process_bank] &&
                map_full[next_process_bank] && !output_full[next_output_bank]) begin
                process_bank<=next_process_bank;
                compute_output_bank<=next_output_bank;
                proc_x0<=bank_x0[next_process_bank];
                proc_y0<=bank_y0[next_process_bank];
                proc_x1<=bank_x0[next_process_bank]+bank_w[next_process_bank]-1;
                proc_y1<=bank_y0[next_process_bank]+bank_h[next_process_bank]-1;
                proc_w<=bank_w[next_process_bank];
                tile_x<=bank_tile_x[next_process_bank];
                tile_y<=bank_tile_y[next_process_bank];
                issue_index<=0; map_read_addr<=0;
                issue_valid_d<=0; issue_valid_d2<=0;
                s1_valid<=0; s2_valid<=0; s3_valid<=0;
                s4_valid<=0; s5_valid<=0;
                compute_state<=CP_RUN;
            end
        end
    end

    // ---------------------------------------------------------------------
    // Independent output writer.  A complete 32-beat row is prefetched into
    // registers, then emitted as four continuous eight-beat AXI bursts.
    // ---------------------------------------------------------------------
    localparam [3:0] OW_IDLE=0,OW_PREFETCH=1,OW_ROW_MUL=2,OW_ADDR=3,
                     OW_AW=4,OW_W=5,OW_B=6;
    reg [3:0] writer_state;
    reg next_writer_bank;
    reg [5:0] prefetch_issue;
    reg prefetch_valid_d, prefetch_valid_d2;
    reg [5:0] prefetch_index_d, prefetch_index_d2;
    reg [63:0] row_buffer[0:31];
    reg [31:0] output_row_offset;

    always @(posedge aclk) begin
        if (!aresetn || start_pulse) begin
            writer_state<=OW_IDLE; next_writer_bank<=0; writer_bank<=0;
            output_release<=0; output_row<=0; output_segment<=0; output_beat<=0;
            prefetch_issue<=0; prefetch_valid_d<=0; prefetch_valid_d2<=0;
            output_read_addr<=0;
            m_axi_awaddr<=0; axi_awlen_reg<=7; m_axi_awvalid<=0;
            m_axi_wdata<=0; m_axi_wvalid<=0; axi_wlast_reg<=0;
            m_axi_bready<=0; tiles_written<=0; writer_done_pulse<=0;
            write_error_pulse<=0;
        end else begin
            output_release<=0; writer_done_pulse<=0; write_error_pulse<=0;
            prefetch_valid_d <= (writer_state==OW_PREFETCH && prefetch_issue<32);
            prefetch_valid_d2 <= prefetch_valid_d;
            if (prefetch_valid_d) prefetch_index_d2<=prefetch_index_d;
            if (writer_state==OW_PREFETCH && prefetch_issue<32) begin
                prefetch_index_d<=prefetch_issue;
                output_read_addr<=output_row*32+prefetch_issue;
                prefetch_issue<=prefetch_issue+1;
            end
            if (prefetch_valid_d2) begin
                row_buffer[prefetch_index_d2]<=selected_output_data;
                if (prefetch_index_d2==31) writer_state<=OW_ROW_MUL;
            end

            case (writer_state)
                OW_IDLE: begin
                    m_axi_awvalid<=0; m_axi_wvalid<=0; m_axi_bready<=0;
                    if (output_full[next_writer_bank]) begin
                        writer_bank<=next_writer_bank;
                        output_row<=0; output_segment<=0; output_beat<=0;
                        prefetch_issue<=0; prefetch_valid_d<=0;
                        prefetch_valid_d2<=0;
                        output_read_addr<=0; writer_state<=OW_PREFETCH;
                    end
                end
                OW_PREFETCH: ;
                OW_ROW_MUL: begin
                    output_row_offset <=
                        (output_tile_y[writer_bank]+output_row)*cfg_stride;
                    writer_state<=OW_ADDR;
                end
                OW_ADDR: begin
                    m_axi_awaddr<=cfg_destination+output_row_offset+
                        (output_tile_x[writer_bank]<<2)+(output_segment<<6);
                    axi_awlen_reg<=7; m_axi_awvalid<=1; writer_state<=OW_AW;
                end
                OW_AW: if (m_axi_awvalid && m_axi_awready) begin
                    m_axi_awvalid<=0; output_beat<=0;
                    m_axi_wdata<=row_buffer[output_segment*8];
                    axi_wlast_reg<=0; m_axi_wvalid<=1; writer_state<=OW_W;
                end
                OW_W: if (m_axi_wvalid && m_axi_wready) begin
                    if (output_beat==7) begin
                        m_axi_wvalid<=0; axi_wlast_reg<=0;
                        m_axi_bready<=1; writer_state<=OW_B;
                    end else begin
                        output_beat<=output_beat+1;
                        m_axi_wdata<=row_buffer[output_segment*8+output_beat+1];
                        axi_wlast_reg<=(output_beat==6);
                    end
                end
                OW_B: if (m_axi_bvalid && m_axi_bready) begin
                    m_axi_bready<=0;
                    if (m_axi_bresp!=0) write_error_pulse<=1;
                    if (output_segment!=3) begin
                        output_segment<=output_segment+1; writer_state<=OW_ADDR;
                    end else if (output_row!=TILE_H-1) begin
                        output_row<=output_row+1; output_segment<=0;
                        prefetch_issue<=0; prefetch_valid_d<=0;
                        prefetch_valid_d2<=0;
                        writer_state<=OW_PREFETCH;
                    end else begin
                        output_release[writer_bank]<=1;
                        next_writer_bank<=~next_writer_bank;
                        tiles_written<=tiles_written+1;
                        if (tiles_written+1 >= (cfg_width/TILE_W)*(cfg_height/TILE_H))
                            writer_done_pulse<=1;
                        writer_state<=OW_IDLE;
                    end
                end
                default: writer_state<=OW_IDLE;
            endcase
        end
    end

endmodule
