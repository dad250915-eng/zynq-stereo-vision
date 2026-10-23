# 当前约束与时钟检查（2026-09-07）

## 已完成
- rst_n=F15/LVCMOS33/ACTIVE_LOW，接 PLL resetn 和 proc_sys_reset_0/ext_reset_in。
- proc_sys_reset_0：C_EXT_RESET_HIGH=0；aux_reset_in、mb_debug_sys_rst 接常量0；50MHz 接 slowest_sync_clk；PLL locked 接 dcm_locked。
- peripheral_aresetn 接初始化、采集、位宽转换器、encoder 的低有效复位。
- 板载200MHz差分输入 sys_clk_p=R4、sys_clk_n=T4，DIFF_SSTL15；PLL输出50MHz。主时钟5ns由Clocking Wizard的IP XDC提供。
- AN5642 J11摄像头32信号引脚保持原映射：cam0=CMOS1、cam1=CMOS2。
- 新启用 camera_timing_42mhz.xdc，两路PCLK周期23.809524ns。旧 camera_timing_reference.xdc 已设置 IS_ENABLED=false。

## PCLK依据
- AN5642-2016-12-14.pdf：模块24MHz晶振。
- E:/vitis_workspace_dual_camera/dual_camera_2_application/src/ov5640.c 有效表及当前初始化ROM：3034=1A、3035=41、3036=69、3037=13、3108=01、460C=20、3824=04。
- OV5640 DVP应用指南R2.13C第4.1.5节、PDF第23-24页：相同720p时钟配置标注42MHz；30fps参考改3035=21才标注84MHz。
- 460C[1]=0为自动DVP分频，不可直接把3824=04当独立除4。
- 4740=20，指南第4.2节定义数据下降沿输出；当前采集RTL上升沿采样。
- 原7020工程an5642.xdc中的84MHz create_clock为注释，未发现可靠的原工程输入延时预算。

## 验证及尚未完成
- Vivado BD校验、输出生成成功；明确检查C_EXT_RESET_HIGH=0。未运行完整布局布线或上板测量。
- PCLK为依据配置和原厂模式确定的标称值，建议上板核实。
- 旧2.2~4ns输入延时未获当前模块/排线依据，随旧文件禁用；新文件没有虚构set_input_delay。需要确定传感器输出tCO及数据相对PCLK走线延时后，按下降沿参考补齐min/max。当前不能宣布输入时序收敛。
- 保留仅作用于两个PCLK输入缓冲输出的CLOCK_DEDICATED_ROUTE FALSE。
- 仍有原采集接口TKEEP四位对转换器两位的截断提示；需要后续规范IP接口。
- AXIS仍外接、Aurora尚未接入；FIFO复位覆盖和溢出恢复按先前要求暂不修改。

重新打开工程读取最新BD，避免旧GUI内容覆盖。
