# 优化观察与可证明的边界

| 历史阶段 | B目标选择 | C轮廓 | D底图 | GHI | ROI total |
|---|---:|---:|---:|---:|---:|
| 早期，450×166 ROI | 2338.422ms | 376.020ms | 119.795ms | 39.504ms | 2800.472ms |
| B/C优化后，494×208 ROI | 77.492ms | 9.265ms | 121.090ms | 46.195ms | 325.371ms |
| D优化并行后，412×165 ROI | 62.439ms | 9.266ms | 10.854ms | 36.106ms | 173.280ms |

原行见data/optimization_*_excerpt.txt。不同ROI、场景和历史配置，表格仅证明日志中出现过这些量级，不声明统一测试条件下的精确加速比，也不据此宣称达到20fps或领先同类论文。D/GHI并行由overlap_D_GHI=10816us和mask_source=B直接支持，当前源码也体现该依赖。

data/historical_serial_excerpt.txt的连续137～139帧，ROI约179ms，完整round约320ms。统计值以validation/local/perf_summary.json为准。ROI处理速率与完整测量更新率分开给出；全图深度/融合帧率没有在该片段中采样。

资源证据：10月20日三模式综合BRAM为412/500（82.4%）。当时K新增融合功能的独立综合BRAM保持2 Tile，完整综合BRAM与前版相同；这不代表后续任意功能都不增加BRAM。最终实现时序与资源需补匹配发布版本的报告。
