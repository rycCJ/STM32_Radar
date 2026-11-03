# 单路ADC → 电脑距离FFT → IQ观察

主程序：`analyze_single_rx_adc_iq.py`。不修改板上程序，不修改原来的`plot_iq_analysis.py`。

依赖：`python -m pip install numpy matplotlib packaging`。本程序不需要SciPy。

## 运行

```powershell
cd D:\STM32_Radar\process_data
# 自动寻找data目录最新Serial Debug*.txt
python .\analyze_single_rx_adc_iq.py
# 手动指定文件
python .\analyze_single_rx_adc_iq.py '.\data\Serial Debug 2026-10-30 183302.txt'
# 指定距离仓及当前展示的ADC包
python .\analyze_single_rx_adc_iq.py '.\data\Serial Debug 2026-10-30 183302.txt' --range-bin 20 --packet 10
# 或限定自动选仓距离范围；自动选中的强分量未必是人体
python .\analyze_single_rx_adc_iq.py '.\data\Serial Debug 2026-10-30 183302.txt' --min-range 0.5 --max-range 3
```

默认结果在`single_rx_results/输入文件名/`；打开`index.html`看全套图。

## 处理步骤和图

1. 自动识别纯hex文本/二进制；跳过启动日志。单包8字节头`01 02 03 04 05 06 07 08`加1024字节ADC，按512个小端有符号int16解析。
2. 每条Chirp减自身均值，乘Hann窗，做512点实数输入FFT（正频率结果是复数）。按照窗相干增益归一化幅度。
3. `01_adc_preprocess.png`：同一包的原始ADC、去直流、加窗，横轴是Chirp内微秒。
4. `02_range_fft.png`：该包线性/对数距离谱，全部包平均幅度谱，以及所选距离bin。
5. `03_iq_analysis.png`：仿参考脚本四宫格，固定同一个bin，每个包取一个复数。分别画I/Q、幅值、I/Q各自减跨包均值、原始IQ复平面。没有跨RX平均。
6. `04_phase_range_map.png`：相位/解缠及距离—包序列图。未做呼吸心率滤波或频率估计。
7. `iq.csv`保存每包I/Q及相位；`intermediate.npz`保存原始ADC和各步数组；`report.json`记录解析统计和配置。

## 参数与边界

- 波形按当前源码：ADC原采样20MHz、抽取2、有效10MHz，512点，3.22GHz带宽、55us扫频、58.5GHz起始频率、80us Chirp间隔。
- 板子4TX4RX，串口当前只导出缓存起始的一路512点；槽位不是经确认的物理天线编号。
- 包之间没有时间戳；默认横轴包序号。只有实际测得均匀帧率时才加`--fps`，不能由TIME=100ms直接推断。源码中的采集和200ms延时也影响实际周期。
- 默认512点距离网格约5cm。`--nfft`可补零，但不增加物理分辨能力；网格改变后bin编号也随之改变。
- 离线Hann FFT未逐点复现硬件ZO/NCO、校准及专用窗表。I/Q幅值不是标定电压或功率。
- 参考脚本输入是另一块板已经算好的int16复数距离FFT，而本程序从实数ADC开始；不使用它的50Hz或32747阈值。
- 码值平台/极值重复提示检查削顶或瞬态，不能仅凭极值宣布ADC量程。IQ轨迹不一定是圆，I/Q也不一定是正余弦。

## 查看中间数组

```python
import numpy as np
d = np.load('single_rx_results/文件名/intermediate.npz')
print(d['adc'].shape)        # (采集包数, 512)
print(d['range_fft'].shape)  # (采集包数, 257)，默认nfft512
print(d['selected_iq'].shape)# (采集包数,)
```
