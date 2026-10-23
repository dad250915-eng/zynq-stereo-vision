//****************************************Copyright (c)***********************************//
//原子哥在线教学平台：www.yuanzige.com
//技术支持：www.openedv.com
//淘宝店铺：http://openedv.taobao.com 
//关注微信公众平台微信号："正点原子"，免费获取ZYNQ & FPGA & STM32 & LINUX资料。
//版权所有，盗版必究。
//Copyright(C) 正点原子 2018-2028
//All rights reserved
//----------------------------------------------------------------------------------------
// File name:           hdmi_colorbar_top
// Last modified Date:  2023/9/21 9:30:00
// Last Version:        V1.1
// Descriptions:        HDMI彩条显示实验顶层模块
//----------------------------------------------------------------------------------------
// Created by:          正点原子
// Created date:        2023/9/21 9:30:00
// Version:             V1.0
// Descriptions:        The original version
//
//----------------------------------------------------------------------------------------
//****************************************************************************************//

module  dvi_transmitter_top(
	
	input        cfg_clk,		 	//10MHz
	input        pixel_clk,		 	//150MHz
	input        rst_n,			 	//reset
	
    input [23:0] video_in_data,  	// RGB888 video in
    input        video_in_hs,    	// hsync data
    input        video_in_vs,    	// vsync data
    input        video_in_de,    	// data enable
 
    output        rstn_out,     	//芯片复位信号，低有效
    output        iic_scl,      	//I2C的串行时钟信号
    inout         iic_sda,      	//I2C的串行数据信号
   
    output        video_clk,    	//输出时钟                        
    output [23:0] video_out_data,   
    output        video_out_hs,  
    output        video_out_vs,  
    output        video_out_de  
);

//*****************************************************
//**                    main code
//*****************************************************

assign video_clk      = pixel_clk;
assign video_out_data = video_in_data;
assign video_out_hs   = video_in_hs;
assign video_out_vs   = video_in_vs;
assign video_out_de   = video_in_de;


//例化视频芯片控制模块
ms72xx_ctl ms72xx_ctl(
    .clk         (  cfg_clk    ), 
    .rst_n       (  rst_n   ),  

    .rstn_out    (  rstn_out  ),      //芯片复位信号，低有效                        
    .init_over   (    ),              //配置全部完成标志
    .iic_scl     (  iic_scl    ), 
    .iic_sda     (  iic_sda    )  
);

endmodule 