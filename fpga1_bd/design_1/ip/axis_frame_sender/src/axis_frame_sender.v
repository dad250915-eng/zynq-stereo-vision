`timescale 1ns/1ps
// Common clock. A complete packet is buffered before arbitration.
// Full 32-bit words only; data and TLAST are forwarded without modification.
module axis_frame_sender #(parameter integer FIFO_DEPTH=2048)(
 input wire clk, rst_n,
 input wire [31:0] s_axis_0_tdata,
 input wire s_axis_0_tvalid, s_axis_0_tlast,
 output wire s_axis_0_tready,
 input wire [31:0] s_axis_1_tdata,
 input wire s_axis_1_tvalid, s_axis_1_tlast,
 output wire s_axis_1_tready,
 output wire [31:0] m_axis_tdata,
 output wire m_axis_tvalid, m_axis_tlast,
 output wire [3:0] m_axis_tkeep,
 input wire m_axis_tready,
 output reg debug_fifo
);
 localparam CW=$clog2(FIFO_DEPTH)+1;
 wire [1:0] full,empty,wb,rb,overflow,underflow,we,re;
 wire [32:0] din[0:1], dout[0:1];
 reg [CW-1:0] packets0,packets1;
 reg active,selected,prefer;
 assign s_axis_0_tready=rst_n && !wb[0] && !full[0];
 assign s_axis_1_tready=rst_n && !wb[1] && !full[1];
 assign we[0]=s_axis_0_tvalid && s_axis_0_tready;
 assign we[1]=s_axis_1_tvalid && s_axis_1_tready;
 assign din[0]={s_axis_0_tlast,s_axis_0_tdata};
 assign din[1]={s_axis_1_tlast,s_axis_1_tdata};
 assign m_axis_tvalid=rst_n && active && !rb[selected] && !empty[selected];
 assign m_axis_tdata=dout[selected][31:0];
 assign m_axis_tlast=active && dout[selected][32];
 assign m_axis_tkeep=4'hf;
 wire fire=m_axis_tvalid && m_axis_tready;
 wire done=fire && m_axis_tlast;
 assign re[0]=fire && !selected;
 assign re[1]=fire && selected;
 wire avail0=packets0!=0 && !rb[0] && !empty[0];
 wire avail1=packets1!=0 && !rb[1] && !empty[1];
 always @(posedge clk) begin
  if(!rst_n) begin
   packets0<=0;packets1<=0;active<=0;selected<=0;prefer<=0;debug_fifo<=0;
  end else begin
   // Sticky full/overflow evidence. Backpressure prevents accepted-data loss.
   if((!wb[0] && full[0]) || (!wb[1] && full[1]) || (|overflow) || (|underflow)) debug_fifo<=1;
   case({we[0] && s_axis_0_tlast,done && !selected})
    2'b10: packets0<=packets0+1'b1;
    2'b01: packets0<=packets0-1'b1;
    default: ;
   endcase
   case({we[1] && s_axis_1_tlast,done && selected})
    2'b10: packets1<=packets1+1'b1;
    2'b01: packets1<=packets1-1'b1;
    default: ;
   endcase
   if(!active) begin
    if(avail0 || avail1) begin
     selected<=avail0 && avail1 ? prefer : !avail0;
     active<=1;
    end
   end else if(done) begin
    active<=0;prefer<=!selected;
   end
  end
 end
 genvar g;
 generate for(g=0;g<2;g=g+1) begin: fifo_bank
  xpm_fifo_sync #(
   .FIFO_MEMORY_TYPE("block"),.ECC_MODE("no_ecc"),
   .FIFO_WRITE_DEPTH(FIFO_DEPTH),.WRITE_DATA_WIDTH(33),.READ_DATA_WIDTH(33),
   .READ_MODE("fwft"),.FIFO_READ_LATENCY(0),
   .WR_DATA_COUNT_WIDTH(CW),.RD_DATA_COUNT_WIDTH(CW),
   .USE_ADV_FEATURES("0707"),.FULL_RESET_VALUE(0),.DOUT_RESET_VALUE("0"),
   .PROG_FULL_THRESH(10),.PROG_EMPTY_THRESH(10),.WAKEUP_TIME(0)
  ) fifo(
   .sleep(1'b0),.rst(!rst_n),.wr_clk(clk),
   .wr_en(we[g]),.din(din[g]),.full(full[g]),.overflow(overflow[g]),.wr_rst_busy(wb[g]),
   .rd_en(re[g]),.dout(dout[g]),.empty(empty[g]),.underflow(underflow[g]),.rd_rst_busy(rb[g]),
   .injectsbiterr(1'b0),.injectdbiterr(1'b0),.prog_full(),.wr_data_count(),
   .almost_full(),.wr_ack(),.prog_empty(),.rd_data_count(),.almost_empty(),
   .data_valid(),.sbiterr(),.dbiterr()
  );
 end endgenerate
endmodule
