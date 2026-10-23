# FPGA 源码提交说明

本目录按赛事“完整源码”整理，现已另附 dummy_control_new 机械臂上位机程序。来源为当前 FPGA1、FPGA2 和 Vitis 应用；打包过程仅写本提交目录，原工程未修改。

## 内容与版本
- fpga1_bd：双目初始化与采集、视频编码与发送、复位和监测 RTL、BD、约束。
- fpga2_bd：解码、双目矫正、显示与时钟、复位、A/B/C/D/E/G/H/I/J/K 源码、按键、BD、约束。
- K 和模式按键采用三模式 1.1 版，和当前 BD 一致；其余版本见 CUSTOM_IP_VERSIONS.json。
- third_party：当前实际使用的 Vitis Vision 头文件、SGM 支撑头文件、原许可证及来源信息。
- fpga2_bd/reference/xf_sgbm_banked.hpp：实际修改过的 SGM 核心；H 的提交副本已改为相对 include。
- fpga2_vitis：当前应用 C/H、链接脚本及 PS 初始化配套文件。
- calibration：配套左右矫正映射表，分别为 1280×720×4 字节。

## 原创源码与工具生成内容
HLS IP 保留 C++ 和依赖源码，通过 scripts/build_hls.tcl 记录原有器件、100 MHz 时钟和封装版本；不以 HLS 生成 HDL 替代算法源码。手写 RTL IP 则保留其真正的 HDL、封装描述及引用文件。AMD 内置 VDMA、Aurora、PS 等由 Vivado 2023.2 根据 XCI/BD 生成，不复制安装目录的闭源实现和工具自带 ap_int/hls_stream 头文件。

## 可选重建方法
使用 Vivado/Vitis/Vitis HLS 2023.2。先对实际使用的十个 HLS 模块逐个运行 `vitis_hls -f scripts/build_hls.tcl -tclargs 模块名`，再运行 `python scripts/unpack_hls_exports.py`。随后在 Vivado 执行 scripts/create_fpga1_bd_project.tcl 或 create_fpga2_bd_project.tcl，新工程只生成到本目录 build 下。辅助脚本本次未启动综合、实现或上板，源码完整性检查不等于重新构建通过。

路径含中文或空格时，建议复制到较短的英文路径再使用旧版 Xilinx 工具。原始 XPR 和原 HLS 配置以 project_reference 保存，仅作配置依据，含原机器路径，不要直接运行。constraints 内原有历史 Tcl 若含原盘符操作命令，已在提交副本逐行注释，原文仍可查阅；请使用 scripts 中的新入口。

## 自适应 E 扩展
保留已有 roi_e_region_adaptive 源码及测试资料。当前随附 BD 实例化的是 roi_e_region 1.0，自适应版本是独立补充源码，不能写成已经替代当前上板版本。

## 说明
Git、日志、备份、综合 DCP 和旧 XCI 等已有文件未在此次补齐中擅自删除；它们不是完整源码的必需项。Vitis 平台/BSP 由配套硬件重新生成；FPGA 部分不含机械臂底层固件、整体不等于可直接下载的 bit/ELF 运行包。
SOURCE_MANIFEST.json 是整理后的逐文件 SHA-256 清单。原文件前后比对与提交目录整理前备份保存在本次审计目录中，未放入上传文件夹。

## 约束的启用状态
CONSTRAINT_SETTINGS.json 从原 XPR 提取每个约束的启用状态、综合/实现用途和处理顺序，新的建工程脚本保持这些设置。FPGA2 的旧 an5642.xdc 和 hdmi.xdc 在原工程中已禁用，不能把目录中所有 XDC 全部无差别启用；receiver_reset_sync、rx_admission_cdc 等仅在实现阶段使用。FPGA1 的 camera_timing_reference.xdc 是已有参考文件，不在当前工程活动约束清单内。dual_mode_ui.xdc 另在 constraints 下提供便于查找的同内容副本，建工程脚本只加载原相对位置的一份，避免重复。

