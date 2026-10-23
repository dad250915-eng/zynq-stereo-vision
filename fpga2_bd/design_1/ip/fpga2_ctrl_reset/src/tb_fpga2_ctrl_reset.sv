`timescale 1ns/1ps
module tb;
reg clk=0,video_clk=0;always #8 clk=~clk;always #10 video_clk=~video_clk;
reg rst=0,link=0,sys=0,back_ready=0,video_n=0,txlocal=0;
wire[31:0] data;wire[3:0] keep;wire last,valid;
reg ready=1,drop=0,err=0;wire run_video,run_tx,remote;wire[31:0] packets;
fpga2_ctrl_reset #(.HEARTBEAT_CYCLES(20)) tx(.clk(clk),.rst_n(rst),.channel_up(link),.sys_reset_out(sys),.receiver_ready(back_ready),.video_reset_n(video_n),.m_axis_tdata(data),.m_axis_tkeep(keep),.m_axis_tlast(last),.m_axis_tvalid(valid),.m_axis_tready(ready));
aurora_ready_rx #(.TIMEOUT_CYCLES(200)) rx(.clk(clk),.video_clk(video_clk),.rst_n(rst),.channel_up(link),.sys_reset_out(sys),.tx_local_reset_n(txlocal),.rx_data(data),.rx_keep(keep),.rx_last(last),.rx_valid(valid && ready && !drop),.rx_error(err),.video_reset_n(run_video),.tx_reset_n(run_tx),.debug_remote_ready(remote),.debug_ready_packets(packets));
reg stalled=0;reg[31:0] held;reg held_last;
always @(posedge clk) begin
 if(rst && link && !sys) begin
  if(stalled && (!valid || data!==held || last!==held_last))$fatal(1,"unstable TX under backpressure");
  stalled<=valid && !ready;held<=data;held_last<=last;
 end else stalled<=0;
end
task cycles(input integer n);repeat(n)@(negedge clk);endtask
task require_on;begin cycles(70);if(!remote || !run_tx || !run_video)$fatal(1,"READY not propagated");end endtask
task require_off;begin cycles(70);if(remote || run_tx || run_video)$fatal(1,"STOP not propagated");end endtask
initial begin
 cycles(10);rst=1;cycles(10);link=1;txlocal=1;
 cycles(100);if(run_video || remote)$fatal(1,"started before backend ready");
 back_ready=1;cycles(100);if(remote)$fatal(1,"started before video reset released");
 video_n=1;require_on;
 // Sustained heartbeat must renew lease.
 cycles(600);if(!run_video)$fatal(1,"lease not renewed");
 ready=0;cycles(10);back_ready=0;cycles(10);ready=1;require_off;
 back_ready=1;require_on;
 drop=1;cycles(220);if(remote || run_video)$fatal(1,"timeout did not revoke");
 drop=0;require_on;
 err=1;cycles(1);if(remote)$fatal(1,"error did not revoke");err=0;require_on;
 link=0;cycles(10);if(run_video || remote)$fatal(1,"link loss did not revoke");
 back_ready=0;link=1;require_off;
 back_ready=1;require_on;
 // Local camera init not done must hold whole data chain despite remote READY.
 txlocal=0;cycles(8);if(run_video || run_tx)$fatal(1,"local ready ignored");
 txlocal=1;require_on;
 $display("READY_HANDSHAKE_PASS packets=%d",packets);$finish;
end
initial begin #100000;$fatal(1,"timeout");end
endmodule
