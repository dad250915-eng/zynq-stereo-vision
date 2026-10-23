# SFP1 约束（2026-09-08）

使用AX7A035B的SFP1（原理图J2），不是SFP2/J3。
TX P/N=B4/A4；RX P/N=B8/A8；125MHz参考时钟P/N=F6/E6；TX_DISABLE=J15，LVCMOS33。
依据AC7A035核心板原理图第4/6/9页、AX7A035B扩展板第2/3页。
SFP1连接MGT通道0，当前Aurora为单通道0配置。参考时钟IBUFDS_GTE2输出连接GTREFCLK0。
SFP1_TX_DIS通过4.7k电阻上拉，必须驱动0才能使能发射；已在BD增加sfp1_tx_disable[0]并接xlconstant_0的0。
ax7a035b_sfp1.xdc已加入constrs_1。125MHz输入时钟8ns单独声明；IP中的OOC输入约束只用于IP独立综合，内部TXOUTCLK约束保留，不重复添加。
IP文件中的F10/E10和GT通道LOC示例均为注释，不作为本板实际绑定。
顺便修正BD中aurora_reset_ctrl_0的reset/gt_reset输出POLARITY为ACTIVE_HIGH，没有改变RTL。
BD验证与输出生成通过；仍有自定义axis_tx_reset_ctrl输出复位缺少关联时钟元数据的告警，后续完善打包。摄像头TKEEP截断提示仍在。
引脚检查使用真实wrapper与BD黑盒并禁用普通IOBUF插入，仅验证端口名称、XDC语法与PACKAGE_PIN赋值；不代表实际GT布局布线或完整时序通过。
未执行完整工程实现或上板测试。完整帧启动与采集输入时序预算仍未完成。
请重新打开工程加载BD，避免旧GUI覆盖新增TX_DISABLE端口。
