# 多谱雷达 ADC 分步解析

## 先确认输入

### 当前实测文件（2026-10-30 18:12:32）

`Serial Debug 2026-10-30 181232.txt` 是雷达串口的纯十六进制文本。
核对当前修改后的 `App_Func.c/Func01_Handle()`，已将发送1MiB改为只发送 `ADC_SAMPLE_ADDR` 开始的 `ADC_MAX_LEN * ADC_BYTE`，即512点/1024字节；中控启动命令也增加了 `AT+PROG=01`。
因此脚本**当前默认每包仅1条导出流**：`8字节0102030405060708头 + 1024字节小端int16 ADC`。
文件解码后36574字节，开头454字节为两次启动参数打印，其后35个完整包，无残帧和包间杂字节。
硬件仍然4TX/4RX，导出1流不代表硬件变为1TX/1RX；准确物理通道/发射Chirp位置还需核对采集安排。

```powershell
python .\analyze_adc.py '.\Serial Debug 2026-10-30 181232.txt'
```

二进制和纯hex文本默认自动识别；也可显式 `--input-kind hex`。
实测图已放在 `measured_adc_20261030_181232/index.html`，仅前5步：没有真实帧率/时间戳，不猜测时间轴、不生成呼吸心率。
观测范围-1200到1279，两端重复值约3.28%/11.89%，存在明显平台/削顶迹象；原因可能在模拟链路或数字处理，不能仅用该范围宣布ADC理论满量程。
启动参数打印带宽模式0、Chirp间隔80us，与本次默认配置一致；距离换算仍取决于采样时序/射频硬件正确。

### 原始整帧版本（兼容，可选）

本工具先处理 **ADC实数采样**。现有 `20.bin` 与本板无关，不作为默认输入，也未作为本板结果分析。

原工程 `Func01_Handle()` 输出：

```
01 02 03 04 05 06 07 08 + 1048576字节ADC
```

恢复原始1MiB发送时，需显式传 `--chirps 64 --tx 4 --rx 4`，解为 `(frame, chirp=64, tx_slot=4, rx_slot=4, sample=512)`，小端16位存储。
每1024字节是一通道的一条Chirp，连续4个RX槽为一组发射时隙，四个TX时隙组成一次完整循环。
物理RX顺序在 `BB_PrepConfig()` 内被重排，TX发射顺序也需与波形表核对；脚本的编号仅代表内存槽位。
**ADC 12位有效码在16位存储中的符号/对齐尚未拿到实测文件确认**，默认 `int16` 是可覆盖的解码假设。
可选 `--adc-encoding twos12`（低12位补码）、`offset12`（低12位偏置码）、`left12`（高12位补码）、`uint16`。

原工程 `Func02_Handle()` 输出的 `66 BB + 150实部float + 150虚部float + BB 66 + 点云` 是1DFFT，不能作为ADC输入。
原STM32中控 USART1 的 `length:`、`[debug]:range:` 是日志，不含ADC，无法逆推出原始ADC。
如果从STM32保存数据，必须先确认固件已将雷达ADC二进制完整转发到电脑；串口助手选择二进制保存，避免文本转码。
脚本不会配置或更改板上固件；有任何中控自定义包头需按实际协议修改 `parse_adc()`。

## 安装与执行（PowerShell）

```powershell
cd D:\STM32_Radar\process_data
python -m pip install -r requirements_adc.txt
python .\analyze_adc.py 'D:\实际采集路径\adc.bin'
```

纯十六进制文本（允许空格、换行和0x，不允许地址/时间戳）：

```powershell
python .\analyze_adc.py '.\adc_hex.txt' --input-kind hex
```

确认为去除了帧头的连续ADC负载，且长度为完整帧的整数倍：

```powershell
python .\analyze_adc.py '.\adc_payload.bin' --format raw
```

选择查看数据及目标距离：

```powershell
python .\analyze_adc.py '.\adc.bin' --frame 0 --chirp 0 --tx-index 0 --rx-index 0 --min-range 0.5 --max-range 3
python .\analyze_adc.py '.\adc.bin' --range-bin 20
```

采集改了点数/TX/RX时必须对应覆盖；这也会改变期望的每帧字节数：

```powershell
python .\analyze_adc.py '.\adc.bin' --samples 512 --chirps 64 --tx 4 --rx 4 --adc-encoding int16
```

6.5GHz带宽或其他采样率示例：

```powershell
python .\analyze_adc.py '.\adc.bin' --bandwidth-hz 6500000000 --sweep-us 55 --adc-clock-hz 20000000 --decimation 2
```

所有数字必须与**采集当时**的实际波形一致，源码默认值不能证明硬件采用了该配置。
默认来源：`App_Def.h` 的512点、64Chirp、4TX/4RX、抽取2；`ParamConfig.h` 的20MHz、55us、58.5GHz；`Default_Param_Config()` 的3.22GHz及80us。
有效采样率=20MHz/2=10MHz；距离轴=FFT拍频×c/(2×扫频斜率)。窗函数使用离线Hann，未实现芯片ZO/NCO和硬件校准。
512点实际覆盖51.2us，物理距离分辨能力使用覆盖带宽计算；补零增加绘图网格密度，不增加物理分辨率。

## 每一步看什么

| 输出 | 处理步骤 | 观察重点 |
|---|---|---|
| 01_adc_raw.png | ADC码值波形、直方图 | 直流偏置、削顶、异常跳变；不是未经标定的电压 |
| 02_dc_window.png | 每条Chirp减均值、Hann加窗 | 去掉基线、加窗压低边缘 |
| 03_range_fft.png | 矩形窗/Hann距离FFT对比 | 峰的位置、窗的旁瓣/主瓣变化；距离以配置正确为前提 |
| 04_range_chirp.png | 单Chirp导出时是距离—包序号图；多Chirp时是帧内图 | 序列复均值相减前后；静态消除也可能损伤微动 |
| 05_iq.png | 选仓、同TX/RX多Chirp复数平均、IQ轨迹 | 最强幅度可能是墙；人工指定距离区间或bin更可靠 |
| 06_phase.png | wrapped/unwrap/DACM近似、去趋势、位移 | 仅多帧+实际时间信息时生成；低IQ幅值处相位不可靠 |
| 07_bandpass.png | 四阶Butterworth SOS前后向带通 | 仅时长和Nyquist允许时生成 |
| 08_spectrum.png | Hann慢时间FFT、带内候选峰 | 候选不等于已验证呼吸/心率，未做体动/谐波排除 |

每张图右侧列出波形参数、解码假设和当前选择。输出目录默认是输入旁的 `<文件名>_adc_steps`。
`index.html` 可双击浏览全部步骤；`report.json` 包含丢弃/截断字节和参数；`intermediate.npz` 保存各处理中间数组；`selected_iq.csv` 保存目标IQ。
图中的热图各自归一化到本图最大值，不能用颜色直接比较绝对功率；距离谱采用相干增益归一化的ADC幅值。

## 慢时间：不要把帧内Chirp和帧间时间混用

参照两个现有流程文档：距离FFT复数序列 → 同通道多Chirp复数平均 → 固定距离仓 → 相位解缠 → 去线性趋势 → 带通 → 加窗频谱。
默认不跨RX/TX直接平均（需要校准），不默认裁剪首尾10秒（短数据会被删完），不将复数除均值当成去除静态背景。
ADC采样本身是实数，复数IQ在距离FFT后产生，不能直接把相邻ADC样点当I/Q。

```powershell
# 仅在外部采集时间确认均匀50帧/秒时，才填写50；绝不是默认板上帧率
python .\analyze_adc.py '.\adc.bin' --fps 50 --max-frames 2000
# 或用每个完整帧一行、单位秒、无表头的单列时间戳CSV
python .\analyze_adc.py '.\adc.bin' --timestamps '.\frame_times.csv'
```

`--fps` 与 `--timestamps` 互斥；时间戳不均匀超过1%时不强行做均匀采样带通。
默认至少30秒才做带通/候选峰；采样率必须覆盖对应频带。没有SciPy时仍输出01–06并在报告说明跳过原因。
默认最多加载256帧；提高上限会增加内存使用量，原始ADC每帧解成float32约2MiB。

**原始整帧版本Func01的1MiB ADC通过921600波特率8N1发送，仅传输就至少约11.38秒/帧。当前512点版本不受这个大包时长限制，但仍有200ms固定延时、采集和其他处理，实际帧率不能直接假设为5Hz或10Hz。**
若按此方式连续录制，帧间采样率不足以分析0.1–0.6Hz呼吸，更不足以分析心率；不能照搬文档50Hz。
可先用ADC验证距离处理，再修改采集输出（例如选定通道/Chirp或输出1DFFT）获得足够快且有时间戳的多帧序列。
本脚本不修改采集固件，也不为当前缺失的ADC文件生成虚构结果。

## 自检

```powershell
python .\test_adc_analysis.py
```

测试仅使用合成信号，覆盖存储布局、符号扩展、残帧/丢字节、拒绝错误格式、FFT峰位置和相干幅度。
`_synthetic_adc_demo` 中的图若生成，仅为合成链路演示，**不是实测**。
