# 既有上位机测试的未通过项

本次在隔离的源码副本执行既有18个测试文件，共109项：**102项通过，4项断言失败，3项执行错误**。未连接串口或机械臂，没有为得到全绿结果更改算法、标定或删除测试。完整输出见 local/offline_python_tests.txt；归档快照见 verified_20261101。

| 测试 | 实际差异 | 后续核实 |
|---|---|---|
| test_temporary_targets_replace_converted_coordinates_exactly | 当前计划没有 converted_target 键 | 该用例针对临时固定目标试验，需核对试验功能是否仍属于成功基线 |
| test_one_click_completes_two_cycles_with_new_acquisition_only | 期望固定(200,0,160)，当前返回标定转换的坐标 | 确认最终两次抓放应使用真实视觉坐标还是旧固定目标试验 |
| test_new_samples_and_coordinate_version | 当前标定JSON无 robot_x_offset_mm | 核对旧X方向+10mm补偿与当前标定约定 |
| test_direct_calibration_does_not_add_legacy_offset | 用例仍期待额外X+10mm，实际不加 | 同上；不能为迎合测试给当前坐标偷偷加偏移 |
| test_each_group_flows_through_capture | 用例期待额外X+10mm | 同上 |
| test_fixed_target_experiment_converts_first_out_of_range_frame | rearm_after 不接受 force_single 参数 | 核对旧试验接口与当前GUI调用路径 |
| test_reported_bad_lock_is_rejected_then_good_target_locks | 用例期待额外X+10mm | 核对基线，不据此推断实机机械误差 |

这些结果证明当前“测试期望、接口和标定配置”存在不一致，不能仅凭名称宣布一定是过期测试，也不能据此认定当前上板功能必然损坏。应先固定最终行为契约，再修改对应测试或实现并重新验证。本次任务为提交资料补齐，保留现状并公开差异。

当前环境Python为3.12.14、NumPy为2.3.5；pyserial使用已有vendor/serial 3.5。GUI启动未验证。归档main.py会优先设置随附Tcl路径；换Python或出现Tcl初始化失败时，应先确认Tcl/Tk版本及英文路径，再修复启动适配，不能把离线测试通过等同于GUI运行通过。
