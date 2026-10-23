`timescale 1ns / 1ps
module alinx_ov5640
#(
   parameter BUFFER_DEPTH = 16384,
   parameter DEBUG_OV5640 = 1
)
(
	input                                        cmos_vsync,       //cmos vsync
	input                                        cmos_href,        //cmos hsync refrence
	input                                        cmos_pclk,        //cmos pxiel clock
	input   [9:0]                                cmos_d,           //cmos data

	// AXI4-Stream signals
	input                                        m_axis_video_aclk,     // AXI4-Stream clock
	input                                        m_axis_video_aresetn,  // AXI4-Stream reset, active low 
	output [15:0]                                m_axis_video_tdata,    // AXI4-Stream data
	output                                       m_axis_video_tvalid,   // AXI4-Stream valid 
	input                                        m_axis_video_tready,   // AXI4-Stream ready 
	output                                       m_axis_video_tuser,    // AXI4-Stream tuser (SOF)
	output                                       m_axis_video_tlast,    // AXI4-Stream tlast (EOL)
	output[3:0]                                  m_axis_video_tkeep     // AXI4-Stream tkeep
	
    );
assign m_axis_video_tkeep = 4'b1111;
wire[15:0]    cmos_d_16bit;
wire          cmos_href_16bit;
reg[7:0]      cmos_d_d0;
reg           cmos_href_d0;
reg           cmos_vsync_d0;
reg           cmos_vsync_d1;
wire          cmos_hblank;
wire           s_axis_tlast;
wire           s_axis_tuser;
wire          s_axis_tready;
wire          empty;
wire          full;
wire          overflow;
wire          underflow;
wire          wr_rst_busy;
wire          rd_rst_busy;
wire [11:0]   wr_data_count;
wire [11:0]   rd_data_count;
wire          rd_en;
wire [15:0]   fifo_tdata;
wire          fifo_tlast;
wire          fifo_tuser;
wire          vsync_fall;
reg           cmos_hblank_d0;
reg           cmos_hblank_d1;
reg           cmos_href_16bit_d0;
reg           cmos_href_16bit_d1;
reg[15:0]     cmos_d_16bit_d0;
reg[15:0]     cmos_d_16bit_d1;

reg           line_valid;
reg [15:0]    pixel_hold;
reg           s_axis_tvalid_reg;
reg [15:0]    s_axis_tdata_reg;
reg           href_d0, href_d1;//    
//wire          s_axis_tvalid = cmos_href_16bit_d1 & cmos_hblank_d1;
//wire          s_axis_tvalid = cmos_href_16bit_d1;//    
wire          s_axis_tvalid;
//wire[15:0]    s_axis_tdata = cmos_d_16bit_d1;  // ? tdata/tvalid   ? 
wire[15:0]    s_axis_tdata;
reg[31:0]     reset_cnt = 32'd0;
reg           startup_done = 1'b0;
wire cmos_aresetn;
(* ASYNC_REG = "TRUE" *) reg [3:0] axis_reset_pclk = 4'b0000;
always @(posedge cmos_pclk or negedge m_axis_video_aresetn) begin
    if (!m_axis_video_aresetn) axis_reset_pclk <= 4'b0000;
    else axis_reset_pclk <= {axis_reset_pclk[2:0], 1'b1};
end
assign cmos_aresetn = startup_done && axis_reset_pclk[3];

(* ASYNC_REG = "TRUE" *) reg [2:0] fifo_ready_axis = 3'b000;
always @(posedge m_axis_video_aclk or negedge cmos_aresetn) begin
    if (!cmos_aresetn) fifo_ready_axis <= 3'b000;
    else fifo_ready_axis <= {fifo_ready_axis[1:0], 1'b1};
end
wire fifo_read_ready = fifo_ready_axis[2] && !rd_rst_busy;
wire fifo_read_ready_pclk;
xpm_cdc_single #(.DEST_SYNC_FF(2), .SRC_INPUT_REG(0), .INIT_SYNC_FF(1)) read_ready_cdc (
    .src_clk(m_axis_video_aclk), .src_in(fifo_read_ready),
    .dest_clk(cmos_pclk), .dest_out(fifo_read_ready_pclk)
);
wire capture_ready = cmos_aresetn && !wr_rst_busy && fifo_read_ready_pclk;

always@(posedge cmos_pclk)
begin
    if(reset_cnt < 32'd200_000_000)
    begin
        reset_cnt <= reset_cnt + 32'd1;
        startup_done <= 1'b0;
    end
    else
    begin
        startup_done <= 1'b1;
    end
end

always@(posedge cmos_pclk)
begin
    if(cmos_aresetn == 1'b0)
    begin
        cmos_d_d0 <= 8'd0;
        cmos_href_d0 <= 1'b0;
        cmos_vsync_d0 <= 1'b0;
        cmos_vsync_d1 <= 1'b0;
    end
    else
    begin
        cmos_d_d0 <= cmos_d[9:2];
        cmos_href_d0 <= cmos_href;
        cmos_vsync_d0 <= cmos_vsync;
        cmos_vsync_d1 <= cmos_vsync_d0;
    end    
end
// ? 8     16    ?   ?   href/data   ??  ?   
cmos_8_16bit cmos_8_16bit_m0(
	.rst(~cmos_aresetn),
	.pclk(cmos_pclk),
	.pdata_i(cmos_d_d0),
	.de_i(cmos_href_d0),
	.pdata_o(cmos_d_16bit),
	.hblank(cmos_hblank),
	.de_o(cmos_href_16bit)
);

always@(posedge cmos_pclk)
begin
    if(cmos_aresetn == 1'b0)
    begin
        cmos_hblank_d0 <= 1'b0;
        cmos_hblank_d1 <= 1'b0;
        cmos_d_16bit_d0 <= 1'b0;
        cmos_d_16bit_d1 <= 1'b0;
        cmos_href_16bit_d0 <= 1'b0;
        cmos_href_16bit_d1 <= 1'b0;   
	end
	else
    begin
        cmos_hblank_d0 <= cmos_hblank;
        cmos_hblank_d1 <= cmos_hblank_d0;
        cmos_d_16bit_d0 <= cmos_d_16bit;
        cmos_d_16bit_d1 <= cmos_d_16bit_d0;
        cmos_href_16bit_d0 <= cmos_href_16bit;
        cmos_href_16bit_d1 <= cmos_href_16bit_d0;
    end	
end

//valid     data
assign s_axis_tvalid = cmos_href_16bit_d0;
assign s_axis_tdata  = cmos_d_16bit_d0;


//tlast
reg [10:0] pixel_cnt;
//wire href_rise = ~cmos_href_16bit_d1 & cmos_href_16bit_d0;
wire line_end;

assign line_end =
    (~cmos_href_16bit_d1) &&
    (~cmos_href_16bit_d0);

always @(posedge cmos_pclk) begin
    if(!cmos_aresetn)
        pixel_cnt <= 11'd0;
    else if(vsync_fall)
        pixel_cnt <= 11'd0;
    else if(line_end)
        pixel_cnt <= 11'd0;
    else if(s_axis_tvalid) begin
            pixel_cnt <= pixel_cnt + 11'd1;
    end
end

assign s_axis_tlast = s_axis_tvalid &&(pixel_cnt == 11'd1279);



//tuser
//   ?   vsync      ? ��   8     16    ?  ?        ?        ?   vsync   ? ? ?  ? ?      ��            ??    ��
reg frame_flag;
reg capture_enabled;
assign vsync_fall = cmos_vsync_d1 & ~cmos_vsync_d0;
wire fifo_wr_en = s_axis_tvalid && capture_enabled && capture_ready && !full;

always @(posedge cmos_pclk)
begin
    if(!capture_ready) begin
        frame_flag <= 1'b0;
        capture_enabled <= 1'b0;
    end
    else if(vsync_fall) begin
        /* Start only at a real frame boundary; the next written pixel is SOF. */
        frame_flag <= 1'b1;
        capture_enabled <= 1'b1;
    end
    else if(s_axis_tvalid && full) begin
        /* Drop the rest of an overflowing frame and resync at next VSYNC. */
        frame_flag <= 1'b0;
        capture_enabled <= 1'b0;
    end
    else if(frame_flag && fifo_wr_en)
        frame_flag <= 1'b0;
end

assign s_axis_tuser = frame_flag & fifo_wr_en;



// Obsolete commented ILA instance removed. Active debug logic is below.

assign s_axis_tready       = capture_enabled & capture_ready & ~full;

/* FWFT FIFO directly implements a stable AXI source under backpressure. */
assign m_axis_video_tvalid = m_axis_video_aresetn & fifo_read_ready & ~empty;
assign m_axis_video_tdata  = fifo_tdata;
assign m_axis_video_tlast  = fifo_tlast;
assign m_axis_video_tuser  = fifo_tuser;

assign rd_en = m_axis_video_tready & m_axis_video_tvalid;
        
generate
if(DEBUG_OV5640)
begin:ila


// Read-domain FIFO status is synchronized for PCLK-domain debug only.
wire empty_pclk_debug;
xpm_cdc_single #(.DEST_SYNC_FF(2), .SRC_INPUT_REG(0), .INIT_SYNC_FF(0), .SIM_ASSERT_CHK(0)) empty_debug_cdc (
    .src_clk(m_axis_video_aclk), .src_in(empty),
    .dest_clk(cmos_pclk), .dest_out(empty_pclk_debug)
);
ila_0 ila_0_m0(
    .clk(cmos_pclk),

    .probe0 (s_axis_tdata),      //      tdata
    .probe1 (s_axis_tvalid),     
    .probe2 (s_axis_tlast),
    .probe3 (s_axis_tuser),
    .probe4 (full),
    .probe5 (empty_pclk_debug),
    .probe6 (wr_data_count),
    .probe7 (overflow),
    .probe8 (cmos_aresetn),
    .probe9 (wr_rst_busy),
    .probe10(pixel_cnt) 
);

end
endgenerate

generate
if(DEBUG_OV5640)
begin:ila_m

ila_2 ila_2_m0(
    .clk(m_axis_video_aclk),

    .probe0 (m_axis_video_tdata),  //      tdata
    .probe1 (m_axis_video_tvalid),
    .probe2 (m_axis_video_tready),
    .probe3 (m_axis_video_tlast),
    .probe4 (m_axis_video_tuser),
    .probe5 (rd_en),
    .probe6 (empty),
    .probe7 (rd_data_count),
    .probe8 (m_axis_video_aresetn),
    .probe9 (rd_rst_busy),
    .probe10 (underflow)
);

end
endgenerate







xpm_fifo_async #(

  .FIFO_MEMORY_TYPE          ("auto"),
  .ECC_MODE                  ("no_ecc"),
  .RELATED_CLOCKS            (0),

  .FIFO_WRITE_DEPTH          (BUFFER_DEPTH),

  .WRITE_DATA_WIDTH          (18),
  .WR_DATA_COUNT_WIDTH       (12),

  .PROG_FULL_THRESH          (10),

  .FULL_RESET_VALUE          (0),

  .USE_ADV_FEATURES          ("0707"),

  .READ_MODE                 ("fwft"),
  .FIFO_READ_LATENCY         (0),

  .READ_DATA_WIDTH           (18),
  .RD_DATA_COUNT_WIDTH       (12),

  .PROG_EMPTY_THRESH         (10),

  .DOUT_RESET_VALUE          ("0"),

  .CDC_SYNC_STAGES           (2),

  .WAKEUP_TIME               (0)

) xpm_fifo_async_inst (

      .rst              (~cmos_aresetn),

      //--------------------
      // Write Side
      //--------------------
      .wr_clk           (cmos_pclk),
      .wr_en            (fifo_wr_en),
      .din              ({s_axis_tdata,s_axis_tlast,s_axis_tuser}),

      .full             (full),
      .overflow         (overflow),

      .prog_full        (),
      .wr_data_count    (wr_data_count),

      .almost_full      (),
      .wr_ack           (),
      .wr_rst_busy      (wr_rst_busy),

      //--------------------
      // Read Side
      //--------------------
      .rd_clk           (m_axis_video_aclk),
      .rd_en            (rd_en),

      .dout             ({fifo_tdata, fifo_tlast, fifo_tuser}),

      .empty            (empty),
      .underflow        (underflow),

      .rd_rst_busy      (rd_rst_busy),

      .prog_empty       (),
      .rd_data_count    (rd_data_count),

      .almost_empty     (),
      .data_valid       (),

      //--------------------
      // Misc
      //--------------------
      .sleep            (1'b0),

      .injectsbiterr    (1'b0),
      .injectdbiterr    (1'b0),

      .sbiterr          (),
      .dbiterr          ()

);

//wire empty;
//wire full;
//assign m_axis_video_tvalid = ~empty ;
//assign s_axis_tready = ~full;
//wire rd_en;

//assign rd_en =
//    m_axis_video_tready &
//    ~empty &
//    fifo_ready_maxis;
//xpm_fifo_async # (

//  .FIFO_MEMORY_TYPE          ("auto"),           //string; "auto", "block", or "distributed";
//  .ECC_MODE                  ("no_ecc"),         //string; "no_ecc" or "en_ecc";
//  .RELATED_CLOCKS            (0),                //positive integer; 0 or 1
//  .FIFO_WRITE_DEPTH          (BUFFER_DEPTH),     //positive integer
//  .WRITE_DATA_WIDTH          (18),               //positive integer
//  .WR_DATA_COUNT_WIDTH       (12),               //positive integer
//  .PROG_FULL_THRESH          (10),               //positive integer
//  .FULL_RESET_VALUE          (0),                //positive integer; 0 or 1
//  .USE_ADV_FEATURES          ("0707"),           //string; "0000" to "1F1F"; 
//  .READ_MODE                 ("fwft"),            //string; "std" or "fwft";
//  .FIFO_READ_LATENCY         (0),                //positive integer;
//  .READ_DATA_WIDTH           (18),               //positive integer
//  .RD_DATA_COUNT_WIDTH       (12),               //positive integer
//  .PROG_EMPTY_THRESH         (10),               //positive integer
//  .DOUT_RESET_VALUE          ("0"),              //string
//  .CDC_SYNC_STAGES           (2),                //positive integer
//  .WAKEUP_TIME               (0)                 //positive integer; 0 or 2;

//) xpm_fifo_async_inst (

//      .rst              (~cmos_aresetn),
//      .wr_clk           (cmos_pclk),
//      .wr_en            (s_axis_tvalid),
//      .din              ({s_axis_tdata,s_axis_tlast,s_axis_tuser}),
//      .full             (full),
//      .overflow         (),
//      .prog_full        (),
//      .wr_data_count    (),
//      .almost_full      (),
//      .wr_ack           (),
//      .wr_rst_busy      (),
//      .rd_clk           (m_axis_video_aclk),
//      .rd_en            (rd_en),
//      .dout             ({m_axis_video_tdata,m_axis_video_tlast,m_axis_video_tuser}),
//      .empty            (empty),
//      .underflow        (),
//      .rd_rst_busy      (),
//      .prog_empty       (),
//      .rd_data_count    (),
//      .almost_empty     (),
//      .data_valid       (),
//      .sleep            (1'b0),
//      .injectsbiterr    (1'b0),
//      .injectdbiterr    (1'b0),
//      .sbiterr          (),
//      .dbiterr          ()
//);
endmodule

