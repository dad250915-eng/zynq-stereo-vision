# 源码提交说明

当前包含 FPGA1、FPGA2、三模式 Vitis 与电脑端机械臂 Python 源码。主入口为 [README.md](README.md)，从零操作顺序见 [docs/reproduction.md](docs/reproduction.md)。

HLS 保留算法 C++、本地头文件及构建脚本，通过 HLS 重新导出封装；AMD 工具内置 IP 由匹配版本工具根据 BD/XCI 生成。工具安装目录的 ap_int、hls_stream、BSP 等不作为自编源码复制。源码归档不要求把 bit、ELF 当作算法源码；板级发布时须绑定匹配的 bit/XSA/ELF。

K 和按键为三模式 1.1；其余封装版本见 CUSTOM_IP_VERSIONS.json。当前 BD 使用 roi_e_region 1.0，roi_e_region_adaptive 是独立扩展和测试资料，没有作为当前上板创新成果申报。

scripts 中新入口使用相对路径。project_reference 的 XPR 及 constraints 内历史 Tcl 只作参考，存在原机路径或已注释命令，不作为新机重建入口。活动约束状态见 CONSTRAINT_SETTINGS.json，禁止把所有 XDC 无差别加载。

SOURCE_MANIFEST.json 覆盖本仓库发布源码；文本使用LF规范化字节，排除自身、运行日志、缓存和构建临时产物；用 scripts/check_submission.py --check-manifest 复查。机械臂测试及运行入口见其 README，中文文档文件名已改为英文，映射见 docs/renamed_files.json。

本次只在提交副本中补充文档、脚本、历史证据与许可声明，不修改 D:/、E:/ 原工程，也不修改已有 C/C++/RTL/Python 算法源码。完整重建与新的上板测量仍待执行。
