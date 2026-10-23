# Zynq Stereo Vision

基于 Zynq 的双目视觉竞赛项目源码，包含双 FPGA 图像处理、立体匹配、三维坐标测量，以及机械臂上位机控制程序。

## 目录

- `fpga1_bd/`：双目初始化、采集、视频编码与发送、RTL、BD 和约束。
- `fpga2_bd/`：双目矫正、立体匹配、深度测量、显示、RTL、HLS 源码和约束。
- `fpga2_vitis/`：嵌入式应用 C/H、链接脚本及 PS 初始化配套文件。
- `dummy_control_new/`：Python 机械臂上位机、相机坐标处理和标定资料；不代表包含机械臂底层固件。
- `calibration/`：左右矫正映射表，适用于原项目标定配置。
- `scripts/`：HLS 构建、IP 解包和 Vivado 建工程脚本。
- `third_party/`：第三方依赖、来源记录及已有许可证。

## FPGA 构建

使用 Vivado / Vitis / Vitis HLS 2023.2。HLS 脚本指定器件为 `xc7z035ffg900-2`，时钟周期 10 ns。建议放在简短的英文路径中。

按 `scripts/build_hls.tcl` 中 names 列表，对十个模块分别执行：

```text
vitis_hls -f scripts/build_hls.tcl -tclargs roi_h_sgm
```

上面的命令仅以 roi_h_sgm 为例。全部模块导出后执行：

```text
python scripts/unpack_hls_exports.py
vivado -mode batch -source scripts/create_fpga1_bd_project.tcl
vivado -mode batch -source scripts/create_fpga2_bd_project.tcl
```

具体版本、约束启用状态和已有重建说明见 `README_源码提交说明.md`、`CUSTOM_IP_VERSIONS.json` 和 `CONSTRAINT_SETTINGS.json`。Vitis 平台与 BSP 需结合配套硬件重新生成。

## 机械臂上位机

使用带 Tkinter 的 Python 3，安装依赖并启动：

```text
python -m pip install -r dummy_control_new/requirements.txt
python dummy_control_new/main.py
```

详细操作和协议说明见 `dummy_control_new/README.md`。标定参数与原硬件相关，移植时需要重新核对。连接实物前阅读控制程序的操作与使用边界说明。

## 验证状态与范围

此版本为源码整理版本，尚未完成干净环境下的 FPGA 全流程重建、综合实现及上板复验，不作为可直接烧录的 bit/ELF 发布包。硬件型号、接线与完整部署流程仍需补充。自适应 E 为补充源码，随附 BD 使用 roi_e_region 1.0。

`SOURCE_MANIFEST.json` 为原始整理目录的历史清单，不是当前仓库发布校验清单；日志与生成 DCP 等已通过忽略规则排除。

## 授权

第三方代码遵循各自随附的许可证和版权声明。自有代码的开源许可证尚待项目作者确定，本仓库暂未声明统一开源授权。
