# 多谱1DFFT IQ分析

## 已确认这次采集是什么

`1d_data/Serial Debug 2026-10-30 200503.txt` 是纯hex文本，转为92883个字节后：

- 57个有效1DFFT块，每块1204字节：`66 BB + 150个小端float32实部 + 150个小端float32虚部 + BB 66`。
- 每个FFT块后有一个完整变长点云包，57个点云包长度及点数关系均匹配，解析没有残字节；浮点值全部有限，57帧内容各不相同。
- 解码结果为 `(57,150)` 复数距离FFT，**没有再次做距离FFT**，也不按相邻字节配成int16 IQ。
- 固件提取 `j=2` 的Chirp、虚拟通道槽 `m=6`，对应的代码位于 `App_Func.c` 中的 `realPart/imagePart` 读取。物理天线编号仍需结合通道重排核实。
- 点云包帧间隔99或100ms；重建第一帧到最后一帧的相对跨度5.577s，约10Hz。第一帧的间隔是相对记录外上一帧，故不计入本段跨度。

## 与参考 plot_iq_analysis.py 的区别

参考脚本读取的是24GHz另一块板的**复数距离FFT结果**，并非原始ADC：

```
5A5A5A5A + frameIndex(u32) + FFT数量(u32)
+ [I(int16), Q(int16)]逐点交错 + A5A5A5A
```

默认128个复数点，整理为 `(frame,rx=2,chirp=8,bin=8)`，随后取 `frames[:,0,0,range_bin]`。
主四联图：I/Q随帧变化、模值 `sqrt(I²+Q²)`、分别减均值、复平面散点。
附加图：硬编码50Hz下的0.1–0.8Hz IQ带通；相位angle/unwrap/线性去趋势/带通/Hann窗/FFT，按频带取峰。
实际主函数没有跨Chirp平均；文档中提到的多Chirp平均不能套在这段代码的行为上。

本次芯片已经输出float32 IQ，原脚本的解析函数、固定50Hz、int16饱和门限±32747都不能沿用。
float32 FFT幅值不会直接告诉你ADC是否饱和；需要看前次原始ADC或硬件状态。

## 已生成的图

结果在 `1d_data/measured_1d_20261030_200503/index.html`：

1. `01_iq_analysis.png`：与原脚本相似的四联图。
2. `02_static_iq_signal.png`：原始I/Q与减均值后I/Q；本次短数据不伪装成带通后的呼吸曲线。
3. `03_range_map.png`：平均距离谱、第一帧距离谱、距离—帧热图，用来选择bin。
4. `04_phase.png`：angle、unwrap、线性去趋势。
5. `05_static_phase_spectrum.png`：短记录Hann窗相位诊断谱，Hz和cycles/min两种坐标，**不标注呼吸/心率估计**。

默认在0.2–7.5m内按平均幅值选最强仓，这批数据选择bin9，条件距离约0.450369m。
这个强反射可能是目标或杂波，不能自动宣布是人体。可以通过 `--range-bin` 改为已知目标仓。
距离系数0.050041m/bin来自本工程3.22GHz/55us、10MHz有效采样、512点配置。修改波形后需同时修改系数。

原始I/Q和交流I/Q很接近是因为本次该仓复数均值较小（约0.08596+j0.1905），不是漏做去均值。

## 运行

```powershell
cd D:\STM32_Radar\process_data
python -m pip install -r requirements_adc.txt
python .\analyze_1d_iq.py '.\1d_data\Serial Debug 2026-10-30 200503.txt' --output '.\1d_data\measured_1d_20261030_200503'

# 与参考脚本的默认bin2对照；保留主结果另存
python .\analyze_1d_iq.py '.\1d_data\Serial Debug 2026-10-30 200503.txt' --range-bin 2 --output '.\1d_data\bin2_comparison'
```

输出还包括 `parsed_1d.npz`（全距离FFT矩阵）、`selected_iq.csv` 和 `report.json`（每帧偏移、点数和间隔）。
原有 `plot_iq_analysis.py` 不修改。

记录达到30秒后，在帧间隔可用且相对均匀的情况下，脚本可自动增加0.1–2Hz零相位带通频谱；否则保留短数据诊断图。
包内时长只精确到ms，也没有递增帧号或CRC，若怀疑丢帧应提供主机时间戳。
不要为了强制出呼吸曲线而把帧率改成50Hz或降低时长要求。

## 固件仍需关注的独立问题

当前 `realSUM` 和 `imageSUM` 是自动局部数组：在 `Func02_Handle()` 的case0计算，返回后下一次case1才发送。
语言层面不能保证上次调用的数组仍有效。建议改为持久帧缓冲或在同次调用内发送，再复采对照；本次任务未修改固件。
这里确认的是数据格式符合1DFFT协议，不等于已经证明每个数值都没有受栈覆盖影响。

## 自检

`python .\test_1d_iq.py`：块状实/虚部配对、变长点云跳过、坏尾重同步、截断包、ADC拒绝和时间信息降级。
