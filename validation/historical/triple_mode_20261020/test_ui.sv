`timescale 1ns/1ps
module test_ui;
 reg clk=0,resetn=0,key_n=1,alarm_enable=0;
 wire [1:0] mode_depth;wire alarm_led;
 roi_mode_button #(.DEBOUNCE_CYCLES(8)) dut(.*);
 always #5 clk=~clk;
 task cycles(input integer n);repeat(n) @(negedge clk);endtask
 task press;begin key_n=0;cycles(30);key_n=1;cycles(30);end endtask
 initial begin
  cycles(4);resetn=1;cycles(20);alarm_enable=1;cycles(2);
  if(mode_depth!==0||alarm_led!==1)$fatal(1,"reset/range LED");
  key_n=0;cycles(2);key_n=1;cycles(20);
  if(mode_depth!==0)$fatal(1,"bounce accepted");
  key_n=0;cycles(60);
  if(mode_depth!==1||alarm_led!==0)$fatal(1,"hold must toggle once");
  key_n=1;cycles(30);press();
  if(mode_depth!==2||alarm_led!==0)$fatal(1,"fusion mode");
  press();if(mode_depth!==0||alarm_led!==1)$fatal(1,"wrap to range");
  alarm_enable=0;cycles(2);if(alarm_led!==0)$fatal(1,"alarm off");
  press();press();resetn=0;cycles(3);
  if(mode_depth!==0||alarm_led!==0)$fatal(1,"reset from fusion");
  $display("PASS UI RTL: bounce/hold/three-mode cycle/reset/LED");$finish;
 end
 initial begin #20000;$fatal(1,"timeout");end
endmodule
