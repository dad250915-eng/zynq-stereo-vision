# 复现步骤与验证状态

## 工具与边界

使用 Windows、Vivado/Vitis/Vitis HLS 2023.2，器件支持包需含 Artix-7 和 Zynq-7000。本项目未迁移到赛事推荐的新工具版本；升级后的 IP、驱动、RTL 生成和时序结果须重新验证。本次整理完成源码静态核查、离线脚本验证和既有 Python 测试；没有重新进行完整综合、布局布线或板级动作。

在短英文路径解压，例如 `C:/fpga/stereo_vision`。下列命令在相应工具已加入 PATH 的终端运行。CPU 主机工具 Python 建议 3.10 或更新。不要从 project_reference 中的原机 XPR 直接开始，也不要使用 constraints 中旧 Tcl 的原盘符命令。

## 1. 检查归档

```powershell
cd C:/fpga/stereo_vision
python scripts/check_submission.py --check-manifest
python scripts/parse_perf.py data/historical_serial_excerpt.txt --out validation/local/perf_summary.json
```

`--check-manifest` 校验逐文件 SHA-256（本Git仓库对文本使用LF规范化字节，对二进制使用原始字节）。构建输出、缓存和本地验证输出不作为源码基线。其余文件变更后，应先审查差异再执行 `python scripts/update_manifest.py`；重新生成清单不能证明变更正确。

## 2. 重建 HLS 封装

```powershell
$ipNames = 'roi_a_gray','roi_b_target','roi_c_contour','roi_d_overlay','roi_e_region','roi_g_supply','roi_h_sgm','roi_i_measure','roi_j_display','roi_k_depth'
foreach ($ipName in $ipNames) {
  vitis_hls -f scripts/build_hls.tcl -tclargs $ipName
  if ($LASTEXITCODE -ne 0) { throw "HLS failed: $ipName" }
}
python scripts/unpack_hls_exports.py
```

输入为本目录 C++ 和 third_party 头文件，器件 xc7z035ffg900-2，周期 10ns。K 导出 1.1，其他活动 HLS 模块 1.0。工具内置 ap_int/hls_stream 无需另行复制。E adaptive 是可选扩展，不在这十个活动 IP 内。

## 3. 建立两个 Vivado 工程

```powershell
vivado -mode batch -source scripts/create_fpga1_bd_project.tcl
vivado -mode batch -source scripts/create_fpga2_bd_project.tcl
```

输出分别为 build/stereo_match_fpga1、build/stereo_match_fpga2。脚本加载手写 RTL、IP 仓库、原 BD，并保留约束启用与实现用途。必须核对 IP Status、Validate Design、顶层管脚及约束。HLS 封装缺失时先返回第2步，避免依赖原电脑的全局 IP 路径。

## 4. 综合、布局布线与 XSA

```powershell
vivado -mode batch -source scripts/build_and_export.tcl -tclargs build/stereo_match_fpga1/stereo_match_fpga1.xpr fpga1 4
vivado -mode batch -source scripts/build_and_export.tcl -tclargs build/stereo_match_fpga2/stereo_match_fpga2.xpr fpga2 4
```

这是完整构建入口，会运行综合和实现；不能在旧 GUI 同时覆盖新工程。输出 build/release 下的报告和 FPGA2 XSA。检查 implementation 状态、时序、DRC 与未约束路径。负 WNS 不能写成“时序通过”；有 bitstream 不等于所有路径达到约束。脚本在失败或过期实现上停止，不清除 NEEDS_REFRESH 伪装通过。

本入口为本次新增，尚未完成真实 Vivado 全构建；本次做 Tcl 结构检查，模拟调用测试在当前执行环境受限，见 validation/local。若工具报 IP 升级或授权问题，保留完整错误并在隔离输出工程中处理。

## 5. Vitis 应用

```powershell
xsct scripts/create_vitis_workspace.tcl build/release/fpga2/stereo_fpga2.xsa build/vitis
```

从匹配的 XSA 创建 Cortex-A9 #0 standalone 平台，stdin/stdout 使用 ps7_uart_0；导入 fpga2_vitis 下的 C/H 与 lscript.ld，链接 libm，生成 ELF。源码主入口 display_demo.c。原有头文件会被主应用 include，不要人为把 header 当作独立源文件。若 Vitis 创建默认域名与脚本不同，使用 GUI 建立 standalone_domain 并保持同样设置；该新入口尚未在真实 XSCT 中完成全编译。

lscript.ld 及 ROI 配置使用固定 DDR 区间，不能只修改 RAM 长度掩盖冲突；用 ELF map 和地址表核对静态区域与 ROI DDR 不重叠。BSP 必须根据本次硬件生成，不能随意复用后续四模式的 BSP。

## 6. FPGA2 下载与映射表

`scripts/prepare_board_config.py` 从 ELF32 符号提取左右映射表和 `rectify_maps_wait_marker` 地址，并记录 bit/ELF/ps7_init/映射表哈希：

```powershell
python scripts/prepare_board_config.py --elf build/vitis/stereo_app/Debug/stereo_app.elf --bit build/stereo_match_fpga2/stereo_match_fpga2.runs/impl_1/design_1_wrapper.bit --ps-init fpga2_vitis/platform_init/ps7_init.tcl --out board/board_config.tcl
xsct scripts/run_fpga2.tcl board/board_config.tcl
```

**示例 bit 路径需按 build_and_export 的实际输出核对。ps7_init 必须换成同次 XSA 生成的配套文件**；归档的 platform_init 是历史配套，脚本无法仅凭文件名证明它与新硬件一致。新构建还应核对 HWH 的时钟、DDR、UART 设置。

run_fpga2 只用于用户明确执行下载时：检查哈希→选唯一 A9目标→FPGA配置→PS初始化→ELF下载→运行到本 ELF 的等待标记→确认两表中心仍是 0xFFFFFFFF→下载左右表→逐字回读比对→继续。没有确认等待点就不写表。重复失败时保留串口日志，不沿用旧地址重试。

建议先启动 FPGA2 接收端，再下载并启动 FPGA1 发送端。板卡 JTAG 目标和光模块链路须人工确认；不要把两个 FPGA 的比特流下载到相反器件。等待链路时观察串口，不通过反复复位掩盖故障。

## 7. 三模式与 PC 机械臂

测距→深度→融合→测距。先确认三模式单独运行、无效目标、边界目标、红灯 500mm 阈值、显示翻转和 mode 切换没有错误。串口出现 XYZ valid=1 只表示视觉样本通过筛选，不等于机械臂坐标变换和抓取已验证。

```powershell
python -m venv .venv
.venv/Scripts/python -m pip install -r dummy_control_new/requirements.txt
.venv/Scripts/python -B -m unittest discover -s dummy_control_new/tests -p 'test_*.py' -v
.venv/Scripts/python dummy_control_new/main.py
```

Python 需支持 tkinter。requirements 是最小依赖范围，实际环境版本见 validation/local；没有宣称跨所有 Python/NumPy 版本均通过。相机串口选择 FPGA2 输出端，机械臂串口选择 Dummy V2 USB 端；按 camera_mode_notes.md 与 camera_two_pick_notes.md 核对使能、标定和两次抓放步骤。未连接机械臂时离线测试不发送实体运动命令。

当前既有测试109项中7项未通过，命令会以非零状态结束，详见 validation/python_known_failures.md；不要把失败隐藏为通过。GUI尚未验证，旧main.py的vendored Tcl/Tk路径对不同Python发行版可能需要适配。先在短英文路径确认GUI和串口列表，再执行实际设备流程。

## 8. 留存结果

每次发布绑定源码清单、bit、XSA、ELF、ps7_init、映射表哈希，保存三模式串口日志、资源/时序/DRC 报告、实物与界面图及抓取统计。ROI total 与 CAPTURE round 分开统计。剩余待实测项目见 report/remaining_items.md。
