# 毫米波静态相位 + IQ 信号分析流程详解

## 一、系统概述

本脚本（[`static_phase_iq_analysis.py`](../static_phase_iq_analysis.py)）针对 24GHz 毫米波雷达（矽典微 FMCW 体制）在人体**静止**场景下采集的原始距离维数据，直接从复数 IQ 信号提取相位微动，经"解缠 → 去趋势 → 带通 → FFT 频谱"的**轻量确定性流水线**估计呼吸率（RR）与心率（HR）。与生产版 `nn_process.py` 不同，本脚本**不做圆心去偏置、不做 ONNX 深度学习、无质控拒绝逻辑**，是一套面向"信号质量较好"场景的快速分析与可视化工具，采样率 50Hz、分析窗 30 秒、输出单位 BPM。

**核心流水线：**
```
原始 .bin 帧 → 帧解析 → rx0 多chirp相干累计 → 首尾裁剪 → 距离仓选择
→ DACM 相位解缠 → 线性去趋势 → Butterworth 带通 + filtfilt → Hanning窗 + FFT
→ 呼吸/心跳带峰值检测 → 质量指标(占比/SNR) → 滑窗时序估计 → 多类可视化输出
```

---

## 二、逐步详解

### 步骤1：原始帧解析（parse_radar_frames）

**实现方式：** 逐帧读取二进制文件，按固定帧格式解析复数 IQ 数据，帧头错位时自动重新同步。

**代码位置：** [`static_phase_iq_analysis.py:27-112`](../static_phase_iq_analysis.py:27)

**帧格式：**
```
[4B Header 0x5A5A5A5A][4B frameIndex][4B rangeFFT_num][N×4B Data][4B Tailer 0xA5A5A5A5]
每个复数点: 实部 int16 + 虚部 int16
存储顺序: chirp × rx × range_bin, reshape 后转置为 (rx, chirp, range)
```
**返回：** `np.ndarray` shape=(N, rx, chirp, range_bin)，默认 rx=2 / chirp=8 / bin=8。

**其他可选方法：**
| 方法 | 说明 |
|------|------|
| 一次性 numpy.frombuffer 全量解析 | 最快但要求文件无坏帧、无错位 |
| 结构化 dtype 批量解析 | 优雅但对变长/错位帧不鲁棒 |
| 逐帧 + 帧头搜索重同步（本方法） | 稍慢但能容忍数据流中的丢帧/错位 |

**本方法优势：** 帧头不匹配时在 1024 字节窗口内搜索 `0x5A5A5A5A` 重新对齐，对采集过程中的丢包/截断具备鲁棒性；向量化拆分实部/虚部（`int16_data[0::2] + 1j*int16_data[1::2]`）解析高效。

---

### 步骤2：多 chirp 相干累计 + 首尾裁剪（load_board）

**实现方式：** 取 rx0 通道的前 `NUM_CHIRP_AVG`（默认 8）个 chirp 做相干（复数）累计均值，得到每帧一维距离谱 `seq[N, bin]`；再裁掉首尾各 `TRIM_SEC`（默认 10s）以去除起测/收测的姿态扰动。

**代码位置：** [`static_phase_iq_analysis.py:235-250`](../static_phase_iq_analysis.py:235)

**公式：**
```
seq[n, bin] = mean( cube[n, 0, 0:k, bin] ),  k = min(NUM_CHIRP_AVG, chirp数)
裁剪帧数 trim = round(TRIM_SEC × FPS)
```

**其他可选方法：**
| 方法 | 说明 |
|------|------|
| 单 chirp 直接取用 | 简单但信噪比低 |
| 多 rx 通道融合 | 可提升 SNR，但需相位对齐 |
| 幅度（非相干）累计 | 破坏相位信息，不适合微动提取 |

**本方法优势：** 复数域相干累计在保留相位的同时提升 SNR（√k 增益）；首尾裁剪去除人员进出/落座的大幅瞬态，让后续频谱更干净。

---

### 步骤3：距离仓选择（pick_bin）

**实现方式：** 若 `RANGE_BIN` 为整数则固定使用该 bin；若为 `None` 则按各 bin 幅度时序均值 `mean(|seq|)` 自动选最强 bin。当前默认 `RANGE_BIN=1`。

**代码位置：** [`static_phase_iq_analysis.py:253-265`](../static_phase_iq_analysis.py:253)

**其他可选方法：**
| 方法 | 说明 |
|------|------|
| 固定先验 bin（本默认） | 最稳定，适合固定安装距离 |
| 全局最大幅度自动选（本可选） | 自适应但易受强反射杂波干扰 |
| 相位方差/呼吸带能量选 bin | 更贴近微动目标，但计算量增大 |
| 多仓众数投票 | 抗抖动，nn_process 生产版采用 |

**本方法优势：** 双模式兼顾稳定性与灵活性——静止实验中目标距离已知时固定 bin 最可靠；探索性分析时可切自动选强。

---

### 步骤4：相位解缠（DACM）

**实现方式：** 使用 DACM（Differential Arc-length Continuous Model）从复数信号提取连续相位：对 IQ 功率做 5 点滑动平均归一化，对相邻采样计算微分角频率 `(I·dQ − Q·dI)/power` 后累加，回退方案为 `np.unwrap(np.angle(s))`。

**代码位置：** [`static_phase_iq_analysis.py:117-136`](../static_phase_iq_analysis.py:117)、[`static_phase_iq_analysis.py:268-274`](../static_phase_iq_analysis.py:268)

**公式：**
```
Δφ[n] = ( I[n]·(Q[n+1]−Q[n]) − Q[n]·(I[n+1]−I[n]) ) / power[n]
power  = movavg(I²+Q², 5) + 1e-6
φ[n]   = φ[0] + Σ Δφ
```

**其他可选方法：**
| 方法 | 说明 |
|------|------|
| np.unwrap（Itoh 差分） | 简单但对噪声/大位移敏感，易跳变 |
| DACM（本方法） | 功率归一化的差分累积，较鲁棒 |
| Extended DACM | 功率加权 + 平滑，nn_process 生产版采用，最鲁棒 |
| 圆拟合解缠 | 先拟合单位圆再取弧长，精度高但计算量大 |

**本方法优势：** DACM 对幅度归一化，抑制了低信噪区的相位跳变，比朴素 unwrap 更稳；且不依赖圆心估计，实现极轻量。**局限**：未做圆心去偏置，当 IQ 星座偏离原点较大时精度不及生产版。

---

### 步骤5：线性去趋势（detrend）

**实现方式：** 对解缠相位做 `scipy.signal.detrend(type="linear")`，去除缓慢基线漂移（体动、热漂）。

**代码位置：** [`static_phase_iq_analysis.py:320`](../static_phase_iq_analysis.py:320)、[`static_phase_iq_analysis.py:374`](../static_phase_iq_analysis.py:374)、[`static_phase_iq_analysis.py:578`](../static_phase_iq_analysis.py:578)

**其他可选方法：**
| 方法 | 说明 |
|------|------|
| 线性去趋势（本方法） | 去一次趋势，快速稳健 |
| 多项式(高阶)拟合去趋势 | 抑制曲线漂移，但阶数高易过拟合 |
| 高通滤波 | 干净但可能损失低频呼吸分量 |
| EMD/小波去趋势 | 自适应但计算量大，实时性差 |

**本方法优势：** 线性去趋势计算量极低，配合后续带通已足以压制静止场景下的缓慢漂移，避免过度处理引入失真。注意：本脚本相位**未缩放为位移微米**，直接以弧度参与频谱分析（区别于生产版的 μm 缩放）。

---

### 步骤6：带通滤波（Butterworth + filtfilt）

**实现方式：** 4 阶 Butterworth 带通 + `filtfilt` 零相位滤波。频谱分析统一使用合并带 `[RESP_BAND[0], HEART_BAND[1]]`（默认 0.2~1.5Hz）；IQ 图另用 `IQ_BAND`（默认 0.1~0.8Hz）。信号长度不足 `padlen` 时自动跳过滤波。

**代码位置：** [`static_phase_iq_analysis.py:285-291`](../static_phase_iq_analysis.py:285)（bandpass）、[`static_phase_iq_analysis.py:398`](../static_phase_iq_analysis.py:398)（频谱分析带）

**其他可选方法：**
| 方法 | 说明 |
|------|------|
| FIR 滤波 | 线性相位但阶数高、延迟大 |
| 切比雪夫 | 过渡带陡但通带波纹 |
| Butterworth + filtfilt（本方法） | 通带平坦 + 零相位，波峰位置准确 |

**本方法优势：** Butterworth 通带平坦不引入幅度失真；`filtfilt` 双向滤波消除相位延迟，保证时序波峰位置准确；4 阶在性能与计算量间平衡。

---

### 步骤7：频谱分析（Hanning 窗 + rFFT）

**实现方式：** 对滤波后相位加 Hanning 窗抑制泄漏，做 `np.fft.rfft`（**不补零**，点数=窗内帧数 N），频率轴 `np.fft.rfftfreq(N, 1/FPS)`。

**代码位置：** [`static_phase_iq_analysis.py:408-411`](../static_phase_iq_analysis.py:408)

**频率分辨率：** `FPS / N`。以 30s 窗（N=1500）计约 0.033Hz ≈ 2 BPM（区别于生产版补零 2048 点的 ~0.024Hz）。

**其他可选方法：**
| 方法 | 说明 |
|------|------|
| 补零 FFT | 提高频率插值分辨率，nn_process 补零 2048 点 |
| Welch 周期图 | 方差小但分辨率低 |
| MUSIC/ESPRIT | 超分辨率但计算量极大 |

**本方法优势：** 直接 rFFT 计算高效、实现简单；Hanning 窗有效抑制频谱泄漏。**局限**：未补零，频率分辨率取决于窗长，短窗时分辨率偏低。

---

### 步骤8：呼吸/心跳峰值检测（find_peak）

**实现方式：** 分别在呼吸带 `RESP_BAND`（默认 0.2~0.4Hz）与心跳带 `HEART_BAND`（默认 0.8~1.5Hz）内取幅度谱最大值对应的频率，`BPM = f × 60`。

**代码位置：** [`static_phase_iq_analysis.py:277-282`](../static_phase_iq_analysis.py:277)

**其他可选方法：**
| 方法 | 说明 |
|------|------|
| 简单带内峰值（本方法） | 快速直观，信号好时准确 |
| 峰值 + 40%带宽扩展 | 给出频率置信区间，生产版采用 |
| 抛物线插值 | 三点插值提高频率精度 |
| 谐波产品法 | 利用谐波增强基频检测 |

**本方法优势：** 带内取最大峰实现最简，在静止且信噪比高的场景下已足够准确；分呼吸/心跳两带独立检测，避免呼吸谐波误判为心率。

---

### 步骤9：呼吸信号质量指标（占比 + 峰值 SNR）

**实现方式：** 两个无量纲指标衡量呼吸信号纯净度：
- **呼吸带能量占比 `resp_ratio`** = 呼吸带功率 / 分析带（0~2.5Hz）总功率
- **呼吸峰值信噪比 `resp_snr`** = 呼吸带内峰值功率 / 带内其余频点平均功率

**代码位置：** [`static_phase_iq_analysis.py:310-353`](../static_phase_iq_analysis.py:310)（_compute_peaks）

**公式：**
```
resp_ratio = Σ power[resp_band] / Σ power[freq ≤ 2.5Hz]
resp_snr   = peak_power / mean(其余呼吸带功率)
```

**其他可选方法：**
| 方法 | 说明 |
|------|------|
| 峰值/均值比（本 SNR） | 简单，反映峰突出度 |
| ±0.05Hz 窄带信号/噪声比 | 物理意义更严格，生产版采用（dB） |
| 频带能量比（本 ratio） | 反映呼吸能量集中度 |

**本方法优势：** 两指标从"能量集中度"与"峰突出度"两个角度联合判断呼吸质量，用于人工判读与多板横向对比；**注意本脚本仅计算展示、不做自动拒绝**（区别于生产版 rr_snr≤18 硬阈值拒窗）。

---

### 步骤10：滑窗时序估计（run_sliding_estimate）

**实现方式：** 以 `SLIDE_WIN_SEC`（30s）为窗、`SLIDE_STEP_SEC`（15s）为步滑动，每窗独立执行"去趋势 → 带通 → Hanning+FFT → 峰值"得到该窗呼吸/心率，绘制随时间变化的双 Y 轴折线（呼吸左蓝 / 心率右红）。

**代码位置：** [`static_phase_iq_analysis.py:575-653`](../static_phase_iq_analysis.py:575)

**其他可选方法：**
| 方法 | 说明 |
|------|------|
| 整段单次估计 | 简单但无法反映时变 |
| 滑窗重叠估计（本方法） | 时间分辨率高，可观察稳定性 |
| 卡尔曼/中值平滑跟踪 | 抑制窗间抖动，但引入延迟 |

**本方法优势：** 30s/15s（50% 重叠）滑窗既保证单窗频率分辨率，又提供每 15s 一个估计点的时间分辨率，便于观察呼吸/心率的时变趋势与结果稳定性。

---

## 三、可视化输出一览

| 开关 | 输出 PNG | 内容 |
|------|----------|------|
| `SHOW_PHASE` | `static_phase_timeseries.png` | 图A：各板去趋势解缠相位时序（每板一行） |
| `SHOW_PHASE` | `static_phase_spectrum.png` | 图B：相位频谱双子图（Hz 带呼吸/心跳阴影 + bpm 轴） |
| `SHOW_IQ` | `static_iq_signal.png` | 各板 bin 的 I/Q 实虚部信号（可带通 `IQ_BAND`） |
| `SHOW_BOARD_COMPARE` | `board_iq_compare.png` | 多板 IQ 复平面 2×4（前 8 bin，含 ±32768 溢出线） |
| `SHOW_BOARD_COMPARE` | `board_amp_timeseries.png` | bin1 幅度时序对比 + 每板滑窗呼吸/心率 |
| `SHOW_SLIDING` | `sliding_vital_estimate.png` | 各板滑窗呼吸/心率折线（双 Y 轴） |

**代码位置：** [`run_phase:368`](../static_phase_iq_analysis.py:368)、[`run_iq:474`](../static_phase_iq_analysis.py:474)、[`run_board_compare:509`](../static_phase_iq_analysis.py:509)、[`run_sliding_estimate:638`](../static_phase_iq_analysis.py:638)

---

## 四、完整流程图

```
┌──────────────────────────────────────────────┐
│         原始 .bin 帧文件 (FILES 列表)          │
└───────────────────────┬──────────────────────┘
                        │  parse_radar_frames
             ┌──────────▼──────────┐
             │ (N, rx, chirp, bin) │
             └──────────┬──────────┘
                        │  rx0 前8chirp相干累计均值 + 首尾裁剪10s
             ┌──────────▼──────────┐
             │     seq[N, bin]     │
             └──────────┬──────────┘
                        │  pick_bin (固定 bin1 或自动选最强)
             ┌──────────▼──────────┐
             │   单 bin 复数序列    │
             └──────────┬──────────┘
                        │  DACM 解缠 → 线性去趋势
             ┌──────────▼──────────┐
             │ 去趋势相位 (rad)  │
             └──────────┬──────────┘
                        │  Butterworth 带通 + filtfilt (0.2~1.5Hz)
                  │  Hanning 窗 + rFFT
             ┌──────────▼──────────┐
             │      幅度谱 mag      │
             └──────────┬──────────┘
          ┌─────────────┼─────────────┐
          │             │             │
    呼吸带峰值      心跳带峰值     质量指标
    (0.2~0.4Hz)    (0.8~1.5Hz)   占比/峰值SNR
          │             │             │
          └─────────────┼─────────────┘
                        │  整段报告 + 30s/15s 滑窗时序
             ┌──────────▼──────────┐
             │  输出: 呼吸/心率 BPM  │
             │  输出: 6 类可视化 PNG │
             └─────────────────────┘
```

---

## 五、关键参数速查

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `FPS` | 50 Hz | 帧率 |
| `NUM_CHIRP_AVG` | 8 | rx0 相干累计 chirp 数 |
| `TRIM_SEC` | 10 s | 首尾各裁剪秒数 |
| `RANGE_BIN` | 1 | 固定 bin；None=自动选最强 |
| `RESP_BAND` | 0.2~0.4 Hz | 呼吸带（12~24 BPM） |
| `HEART_BAND` | 0.8~1.5 Hz | 心跳带（48~90 BPM） |
| `IQ_BAND` | 0.1~0.8 Hz | IQ 图带通（=BREATH_BAND） |
| `FILTER_ORDER` | 4 | Butterworth 阶数 |
| 频谱分析带 | 0.2~1.5 Hz | `[RESP_BAND[0], HEART_BAND[1]]` |
| FFT 点数 | =窗内帧数 N | **不补零** |
| 频率分辨率 | FPS/N（30s≈0.033Hz） | 取决于窗长 |
| DACM 平滑窗 | 5 点 | 功率归一化滑动窗 |
| `SLIDE_WIN_SEC` | 30 s | 滑窗窗长 |
| `SLIDE_STEP_SEC` | 15 s | 滑窗步进（50% 重叠） |
| `NUM_BINS` | 8 | 复平面对比 bin 数 |
| `OVERFLOW_LINE` | 32768 | int16 满量程溢出参考线 |

---

## 六、与 nn_process 生产流程的差异对比

| 环节 | 本脚本（静态相位分析） | nn_process 生产流程 |
|------|----------------------|---------------------|
| 定位 | 快速分析/可视化工具 | 生产级实时检测 |
| 静态杂波抑制 | 无（仅去趋势） | 均值归一化去直流 |
| 圆心去偏置 | 无 | IterativeCircleEstimator 去偏置 |
| 相位解缠 | DACM（5点） | Extended DACM（功率加权+平滑） |
| 相位缩放 | 保留弧度 | to_micrometer 缩放为 μm 位移 |
| 距离仓选择 | 固定/最强幅度 | 先验+局部搜索+众数投票 |
| FFT | rFFT 不补零 | 补零 2048 点 |
| 峰值估计 | 带内最大峰 | 峰值+40%带宽扩展 |
| SNR | 峰值/带内均值（无量纲） | ±0.05Hz 窄带 SNR（dB） |
| 心率 | 心跳带 FFT 峰值 | traditional(VMD+IBI+SDNN) 或 ONNX |
| 质控拒绝 | **无**（仅展示指标） | move>600 / rr_snr≤18 / hr_snr<18 拒窗 |
| 适用场景 | 信号好、静止、离线分析 | 含体动/弱信号的实时鲁棒检测 |

**结论：** 本脚本以最少处理换取实现简洁与可解释性，在**静止且信噪比高**的场景下能给出准确的呼吸/心率与丰富可视化；生产版 `nn_process.py` 通过圆心去偏置、Extended DACM、补零 FFT、多重质控在**弱信号/体动**场景下更鲁棒但更复杂。两者互补：本脚本适合数据探查与算法对照，生产版适合部署。