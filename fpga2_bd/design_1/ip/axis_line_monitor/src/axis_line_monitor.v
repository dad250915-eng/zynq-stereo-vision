`timescale 1ns/1ps
// Passive AXIS video observer. All mon_* ports are INPUTS, including TREADY.
module axis_line_monitor #(
 parameter integer DATA_BYTES=4,
 parameter integer USER_WIDTH=1,
 parameter integer EXPECTED_BEATS=1280,
 parameter integer EXPECTED_LINES=720,
 parameter integer CHECK_KEEP=1,
 // 0 disables the idle timeout. Counts clocks with a partial line but no transfer.
 parameter integer IDLE_TIMEOUT_CYCLES=0
)(
 (* X_INTERFACE_INFO="xilinx.com:signal:clock:1.0 clk CLK", X_INTERFACE_PARAMETER="ASSOCIATED_RESET rst_n" *) input wire clk,
 (* X_INTERFACE_INFO="xilinx.com:signal:reset:1.0 rst_n RST", X_INTERFACE_PARAMETER="POLARITY ACTIVE_LOW" *) input wire rst_n,
 input wire clear,
 input wire [DATA_BYTES*8-1:0] mon_tdata,
 input wire [DATA_BYTES-1:0] mon_tkeep,
 input wire [USER_WIDTH-1:0] mon_tuser,
 input wire mon_tvalid, mon_tready, mon_tlast,
 output wire transfer,
 output reg aligned,
 output reg [31:0] current_beats, current_bytes,
 output reg [31:0] last_line_beats, last_line_bytes,
 output reg [31:0] frame_index, line_index, last_frame_lines,
 output reg [7:0] event_flags,
 output reg [7:0] sticky_flags,
 output reg [31:0] short_count, long_count, sof_mid_count,
 output reg [31:0] keep_error_count, frame_error_count, timeout_count,
 output reg [31:0] startup_partial_count, completed_lines, stall_cycles,
 output reg first_error_valid,
 output reg [7:0] first_error_flags,
 output reg [31:0] first_error_beats, first_error_bytes,
 output reg [31:0] first_error_frame, first_error_line
);
 // Flags: 0 short,1 long,2 SOF mid-line,3 bad KEEP,4 frame line count,
 // 5 idle timeout. Startup partial lines are counted separately, not errors.
 assign transfer=mon_tvalid && mon_tready;
 wire sof=|mon_tuser;
 reg frame_seen, long_reported, idle_reported;
 reg [31:0] idle_cycles;
 integer j;
 reg [31:0] byte_count, next_beats, next_bytes;
 reg [7:0] flags;
 reg [31:0] err_beats,err_bytes;
 always @* begin
   byte_count=0;
   for(j=0;j<DATA_BYTES;j=j+1) byte_count=byte_count+mon_tkeep[j];
   if(!CHECK_KEEP) byte_count=DATA_BYTES;
   next_beats=(sof ? 32'd0 : current_beats)+1;
   next_bytes=(sof ? 32'd0 : current_bytes)+byte_count;
   flags=0; err_beats=next_beats;err_bytes=next_bytes;
   if(transfer) begin
     if(sof && aligned && current_beats!=0) begin
       flags[2]=1;err_beats=current_beats;err_bytes=current_bytes;
     end
     if(sof && frame_seen && (line_index!=EXPECTED_LINES || current_beats!=0)) flags[4]=1;
     if(aligned || sof) begin
       if(CHECK_KEEP && mon_tkeep!={DATA_BYTES{1'b1}})flags[3]=1;
       if(next_beats>EXPECTED_BEATS && (!long_reported || sof))flags[1]=1;
       if(mon_tlast && next_beats<EXPECTED_BEATS)flags[0]=1;
     end
   end else if(aligned && current_beats!=0 && IDLE_TIMEOUT_CYCLES>0 &&
               !idle_reported && idle_cycles>=IDLE_TIMEOUT_CYCLES-1) begin
     flags[5]=1;err_beats=current_beats;err_bytes=current_bytes;
   end
 end
 always @(posedge clk) begin
   if(!rst_n || clear) begin
     aligned<=0;frame_seen<=0;long_reported<=0;idle_reported<=0;idle_cycles<=0;
     current_beats<=0;current_bytes<=0;last_line_beats<=0;last_line_bytes<=0;
     frame_index<=0;line_index<=0;last_frame_lines<=0;event_flags<=0;sticky_flags<=0;
     short_count<=0;long_count<=0;sof_mid_count<=0;keep_error_count<=0;
     frame_error_count<=0;timeout_count<=0;startup_partial_count<=0;
     completed_lines<=0;stall_cycles<=0;first_error_valid<=0;first_error_flags<=0;
     first_error_beats<=0;first_error_bytes<=0;first_error_frame<=0;first_error_line<=0;
   end else begin
     event_flags<=flags;sticky_flags<=sticky_flags|flags;
     if(flags[0])short_count<=short_count+1;
     if(flags[1])long_count<=long_count+1;
     if(flags[2])sof_mid_count<=sof_mid_count+1;
     if(flags[3])keep_error_count<=keep_error_count+1;
     if(flags[4])frame_error_count<=frame_error_count+1;
     if(flags[5])begin timeout_count<=timeout_count+1;idle_reported<=1;end
     if(|flags && !first_error_valid)begin
       first_error_valid<=1;first_error_flags<=flags;
       first_error_beats<=err_beats;first_error_bytes<=err_bytes;
       first_error_frame<=frame_index;first_error_line<=line_index;
     end
     if(mon_tvalid && !mon_tready)stall_cycles<=stall_cycles+1;
     if(!transfer && aligned && current_beats!=0 && idle_cycles!=32'hffffffff)
       idle_cycles<=idle_cycles+1;
     if(transfer) begin
       idle_cycles<=0;idle_reported<=0;
       if(sof)begin
         aligned<=1;frame_seen<=1;line_index<=0;long_reported<=0;
         if(frame_seen)begin frame_index<=frame_index+1;last_frame_lines<=line_index;end
       end
       if(aligned || sof)begin
         current_beats<=next_beats;current_bytes<=next_bytes;
         if(flags[1])long_reported<=1;
         if(mon_tlast)begin
           last_line_beats<=next_beats;last_line_bytes<=next_bytes;
           completed_lines<=completed_lines+1;
           line_index<=sof ? 32'd1 : line_index+1;
           current_beats<=0;current_bytes<=0;long_reported<=0;
         end
       end else if(mon_tlast)begin
         // Discard the first unknown fragment, then begin line-level checking.
         aligned<=1;current_beats<=0;current_bytes<=0;line_index<=0;
         startup_partial_count<=startup_partial_count+1;
       end
     end
   end
 end
 // mon_tdata is intentionally not used: this observer verifies boundaries,
 // handshake counts and byte enables, not pixel values or a payload checksum.
endmodule
