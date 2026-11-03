# 第五幅相位频谱图修复（2026-11-01）

## 15bpm_3.txt 的实际原因

完整FFT块986个，完整配套点云包985个；最后FFT块完整，但其点云载荷在文件末尾截断，因此时间元信息不被采纳。
旧版time_axis要求每一帧间隔有效，一个缺失就将整份文件退回帧序号，导致正常带通谱和未带通诊断谱同时跳过。采集时间长本身不能绕过这个条件。

## 这次修改

- 全部986个FFT块、选仓IQ仍保留，原有前四图照常输出。
- 新增spectrum_segment()：单独为频谱选择最长连续有效时间段，不跨未知间隔拼接，不估算尾帧时间。
- 本文件频谱使用0起算帧0–984，即前985帧，时长98.316秒。末帧仅退出频谱，仍留在IQ数据中。
- 有效间隔98–101ms；中位数帧率10Hz；等间隔重采样Fs约10.00854Hz。
- 把近似均匀判据的2%边界改为包含边界并容忍1ns浮点误差，避免98/100ms在浮点计算中被误拒。
- 记录足够长、间隔近似均匀且SciPy可用时，生成0.1–2Hz零相位带通相位及Hz、每分钟周期数两种频谱。
- 时长不足、抖动较大、SciPy缺失等情况下，仍可输出明确标注“未带通”的诊断谱，并将具体原因写入报告，不再一律提示“短记录”。
- 没有任何可信连续时间段时仍不生成Hz频谱；不盲目填50Hz或10Hz。

report.json新增spectrum_timing和spectrum_resolution_hz，记录实际选段、排除帧、重采样Fs和带通状态。
原timing字段描述全部IQ的横轴：有尾帧缺时间时仍可能显示fps=null；这不影响独立的spectrum_timing。
parsed_1d.npz新增spectrum_frame_indices、spectrum_time_s、spectrum_detrended_phase，便于核对。

## 运行

```powershell
cd D:\STM32_Radar\process_data
python .\analyze_1d_iq.py '.\1d_data\15bpm_3.txt' --range-bin 16
```

保留本次原结果的bin16选择，没有为了匹配预期频率改动选仓或IQ。
输出：`1d_data/15bpm_3_1d_iq/05_static_phase_spectrum.png`，以及更新后的index.html和报告。
若本机Python缺少依赖：`python -m pip install numpy matplotlib packaging scipy`。

## 验证

8项测试：尾帧缺时间、内部缺失不拼接、首帧间隔不必有效、2%边界、完全无时间、大间隙分段、显式FPS校验、已知0.25Hz信号保峰。
实际15bpm_3数据运行完成，已核对图；与旧NPZ逐项比较，range_fft、range_m、selected_iq完全相同。

代码修复解决的是“有效时间段被整体否决”，不证明测量得到的相位一定等于真实位移，也不排除固件缓冲区、跨帧相干性等其他因素。
