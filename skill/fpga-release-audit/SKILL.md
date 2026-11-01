---
name: fpga-release-audit
description: Audit a multi-IP FPGA source release with file hashes, interface checks, explicit evidence stages and non-overlapping latency statistics.
---

# FPGA源码发布与证据审计

面向“完整源码已经整理，但能否复现不清楚”的交付。首先锁定用户认可的成功基线和未完成支线，不能把后来更多功能的硬件或BSP混进已验证版本。只修改获准的提交副本。

建立源码清单并核对RTL/HLS/PS/上位机、XDC、BD、自定义IP引用、本地include、标定、许可证与版本。标准工具库应注明安装版本；第三方原声明不能被统一许可覆盖。缺文件不应仅通过删除清单记录来“修复”。

把验证证据分成：静态存在性、软件模型/单元测试、HLS/RTL仿真、综合、实现时序、上板实测。每份证据绑定版本、日期与适用范围。旧的routed报告或其他模式的资源不能替代当前基线。

可用工具位于 ../../scripts：
- `python scripts/check_submission.py --check-manifest`：核对归档、IP引用、活动约束和三模式关键参数。硬编码检查是本项目适配层，复用时替换，不当作通用器件识别器。
- `python scripts/parse_perf.py LOG --out RESULT`：独立统计ROI和CAPTURE时间，排除-1样本和未知单位。只支持这里的PERF文本语法；DEPTH/FUSION行另统计，不能混入ROI。
- `python scripts/update_manifest.py`：审查改动后刷新清单；刷新动作本身不证明功能正确。

延迟报告先标定测量边界。并发G/H/I是同一背压流管线，不能把三者相加；D若重叠也不能再全额加入。ROI total与完整CAPTURE round分开给出1e6/平均微秒，未记录round时不输出端到端测量率。HDMI刷新率与结果更新率分别验证。

硬件发布时绑定同次bit/XSA/ELF/初始化脚本/标定的哈希。调试器给DDR下载数据前先到该ELF的初始化完成点，核对符号布局和哨兵。数组符号与指针变量不同，不能把指针变量本身的地址当作图像缓冲地址。NEEDS_REFRESH与负WNS应保留为失败条件，不通过强行清状态隐藏。

本次观察：旧清单漏掉后来加入的文件，同时缺两份原Digilent声明；现有routed报告实际为9月版本；以ROOT源码静态PASS替代整机复现会产生错误结论。正反例测试见 ../../scripts/tests/test_submission_tools.py，实际执行结果在 ../../validation/local。

可复用提示词：
> 请对提交目录做只读预检，区分源码缺失、构建入口缺失和验证证据不足。对每个结论提供文件依据，检查报告日期与当前版本。再在获准副本补充可确定内容，并列出必须由作者或板级测试确认的项目。禁止补造性能、时序通过或真实对话。
