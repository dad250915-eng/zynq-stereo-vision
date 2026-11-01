# 验证证据与适用范围

| 位置 | 来源与含义 | 不支持的结论 |
|---|---|---|
| local/submission_check.json | 本次静态检查：文件、约束、IP引用、三模式约定与命名 | 不等于综合或板级通过 |
| local/offline_python_tests.txt / offline_python_result.json | 本次既有上位机109项：102通过、4失败、3错误 | 不等于全套测试通过或机械臂实机抓取成功 |
| local/perf_summary.json | 对用户提供的历史日志片段重新统计 | 不等于本次新采样或全场景帧率 |
| local/script_tests.txt | 新增工具脚本的正反例测试 | 不等于 Vivado/XSCT 实际执行完成 |
| local/tcl_structure.json / tcl_mock_result.json | Tcl词法结构检查；模拟调用因当前执行环境文件访问问题未完成 | 不能写成Tcl控制流或Xilinx API已验证 |
| historical/triple_mode_20261020/full_utilization.rpt | 10月20日三模式完整综合 | 不是最终布局布线资源或时序 |
| historical/triple_mode_20261020/*validation.json | 原生K算法、调度模型与UI综合历史记录 | UI仿真受工具故障阻塞；AXIS RTL背压未完成仿真 |
| fpga2_bd/constraints/routed_*.rpt | 9月10日旧版本实现报告 | 不能作为最终三模式时序通过依据 |

三模式历史综合：LUT 142697/171900，FF 168547/343800，BRAM Tile 412/500，DSP 430/900。412 Tile 是综合资源估算，占82.4%，不应据此承诺剩余BRAM能全部自由使用。

新增 build_and_export.tcl 会在真正重建后生成本次的时序、资源和DRC报告。未重建前不替换历史数据，也不把已知近零负时序裕量改写为通过。

本地结果目录被 .gitignore 排除，避免复查污染源码清单。本次已生成的结果如需进入 GitHub，应审阅内容后用 git add -f validation/local，或移入新的带日期证据目录并更新清单。

本次已将结果快照放入 verified_20261101，随归档和清单保留；local目录用于以后复查输出。已知上位机未通过项见 python_known_failures.md。

本次补齐从10月31日延续至11月1日。技能包基本frontmatter核对通过；官方quick_validate受缺少PyYAML限制未完成，详细结果保留。
