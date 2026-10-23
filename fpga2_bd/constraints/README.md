# Z100 后端约束迁移

已启用 z100_sfpa.xdc：SFPA/J31，RX AH10/AH9，TX AK10/AK9，参考时钟 AD10/AD9，156.25 MHz。
已禁用旧 an5642.xdc 与 hdmi.xdc；保留 system.xdc（配置电压3.3V，CFGBVS=VCCO）。

待完成：
1. Aurora 参考时钟从125改156.25 MHz；外部 GT_DIFF_REFCLK1_0 的 FREQ_HZ 改156250000。线速保持2.5Gbps，通道选择GTXE2_CHANNEL_X0Y0，参考时钟选实际MGTREFCLK0。本次只改约束，没有改BD。
2. 增加标量输出 sfpa_tx_disable，连接1位常量0，然后启用 z100_sfpa_control.xdc。AB19有板载上拉，悬空会禁用发射。
3. HDMI 当前仍为 rgb2dvi/TMDS；Z100板载发送芯片接收并行RGB，不能将TMDS端口直接绑定并行引脚。
   将并行视频输出及发送芯片初始化接口接出后，按 z100_hdmi_parallel.xdc 中的端口名称命名，再启用该文件。
   RGB/DE/HS/VS/像素时钟为3.3V；I2C及发送芯片复位为1.8V。
   该文件为引脚约束；输出时序还需按发送芯片配置核验，未添加虚构的output_delay。
4. GPIO_0为遗留摄像头控制接口，需删除或明确新用途，尚未绑定引脚。
5. 当前wrapper仍是旧版，需要BD连线完成后重新生成输出产物及wrapper。
6. PS DDR/FIXED_IO由PS配置管理。FCLK不需要绑定外部晶振引脚。
7. 完成后Validate Design、实现、检查report_io与时序/DRC；当前尚不能视为可上板工程。

资料：Z100底板1V2/核心板1V1原理图，26_ov5640_hdmi/pin.xdc，57_sfp_hdmi_8b10b及56_gtwizard例程。
恢复：原XDC文件未删除，工程XPR备份在本目录。不要让已打开的旧GUI工程状态覆盖此次XPR更新；重新打开工程。
