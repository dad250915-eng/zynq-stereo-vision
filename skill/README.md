# 可复用技能包

本目录把项目中的观察、失败和修正提炼为可移植工作流，不要求使用 PYNQ，也不声称已实现 PYNQ 专项加分。SKILL.md 含可直接交给智能体执行的范围与步骤；配套 Python 工具仅需标准库。

| 技能 | 可用于其他题目的范围 | 使用方法 | 验证与失效边界 |
|---|---|---|---|
| [hls-ddr-stream](hls-ddr-stream/SKILL.md) | AXI DDR 图像/信号扫描核的带宽瓶颈与并行调度 | 输入核源码、综合报告、统一口径日志，按证据定位 | 本项目D历史约121ms→约10.9ms；非同ROI受控基准；新硬件仍须综合上板 |
| [stereo-frame-contract](stereo-frame-contract/SKILL.md) | 双流图像、裁剪、采集缓冲和显示坐标契约 | 先填左右/参考/偏移/写者生命周期，再推导数据路 | 当前源码确认RX与参考关系；不能替代硬件曝光同步、RTL背压验证 |
| [fpga-release-audit](fpga-release-audit/SKILL.md) | 多IP FPGA工程源码交付、证据追踪和延迟统计 | 使用根目录scripts中的检查/日志工具，审阅缺项 | 当前提交执行正反例检查；静态PASS不能当作全重建或时序PASS |

来源案例见 [协作与纠错](../report/ai_collaboration.md)。本次工具验证记录见 validation/local；历史证据见 validation/historical。陌生队伍使用时替换器件、地址、日志解析契约；不要继承本项目的绝对地址或性能承诺。
