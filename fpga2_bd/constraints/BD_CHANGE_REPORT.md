# 本轮BD修改结果（2026-09-10）
备份：backup_bd_20260910_195240，包含修改前XPR、BD/IP配置、约束。
已完成：
- 保留用户已连接的DVI_Transmitter视频接口，补齐10MHz配置Clock Wizard、对应Processor System Reset、像素时钟和所有HDMI外部端口。
- 保留并核对Aurora接收复位连接；将两路RGB565 subset复位纳入video_axis_aresetn。
- 两路32->16位拆分前加入rx_sof_expand_0/1，将TUSER映射为000+SOF。防止Vivado默认复制导致两个像素都带SOF；拆分后subset输出TUSER宽度固定1。
- SFPA TX_DISABLE原常量默认1，已改0；约束对应实际[0:0]端口。
- 启用并行HDMI和SFPA控制XDC，参考时钟外部属性统一156.25MHz。
验证：Validate Design通过，generate_target all完成，wrapper已更新；检查生成HDL确认TUSER经过明确适配。
PS、stereo_match及四个remap模块参数与备份一致，未改算法源码。
未执行整机综合、实现和上板测试，不代表时序闭合。
尚存：
- 旧7020增量检查点与035器件不符，综合前应关闭/清除增量检查点。
- GPIO_0是遗留无引脚约束接口；本轮未删除既有GPIO功能。
- Aurora接收端无法通过TREADY反压发送端；仍需接收缓存/溢出恢复验证。
- 前端打包器不传最后字的TKEEP；当前方案按完整32位有效字工作，需保持每行像素数为偶数。
- HDMI需原软件配置像素时钟/VDMA/VTC后才能显示，尚未验证输出时序或颜色顺序。
此结果替代constraints/README.md里“HDMI/接收端尚未接线”的旧状态说明。
