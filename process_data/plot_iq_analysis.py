"""
震动台数据 IQ 信号分析脚本。

针对指定 24G bin 文件, 取某个 range bin (rx0/chirp0) 生成三张图:
  1. IQ 原始时序图      : I(实部) 与 Q(虚部) 随帧变化, 并标 ±32747 溢出线。
  2. IQ 幅值时序图      : |I + jQ| 随时间变化。
  3. 去直流后交流信号图 : I/Q 各自减去均值(去掉直流分量)后的交流波动。

用法:
    python3 震动台数据/plot_iq_analysis.py [bin文件路径] [range_bin索引]
默认: range_data_2026-07-21_13:49:22.bin, bin0
"""
import sys
import struct
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy.signal import detrend, butter, filtfilt


# ===================== 内联依赖: parse_radar_frames =====================
# 原实现位于 py/src/load_radar.py, 为使脚本独立可移植, 直接内联
def parse_radar_frames(filename,
                       num_rx: int = 2,
                       num_chirps: int = 8,
                       num_range_bins: int = 8,
                       max_frame_num=None) -> np.ndarray:
    """
    解析矽典微24GHz毫米波雷达二进制帧数据文件 (优化版本)

    帧格式:
    [4B Header: 0x5A5A5A5A] [4B frameIndex] [4B rangeFFT_num] [N×4B Data] [4B Tailer: 0xA5A5A5A5]

    Data格式:
    每4字节为一个复数点: Word0(实部, int16) + Word1(虚部, int16)
    存储顺序: rx0_chirp0_bins → rx1_chirp0_bins → rx0_chirp1_bins → rx1_chirp1_bins...

    返回: np.ndarray, shape=(N, rx, chirp, range_bin), dtype=complex64
    """
    frames = []

    with open(filename, 'rb') as f:
        data = f.read()
 
    offset = 0
    frame_count = 0

    header_fmt = struct.Struct('<I')
    frame_index_fmt = struct.Struct('<I')
    range_fft_fmt = struct.Struct('<I')
    tailer_fmt = struct.Struct('<I')

    while offset < len(data):
        if offset + 4 > len(data):
            break

        header = header_fmt.unpack_from(data, offset)[0]

        if header != 0x5A5A5A5A:
            search_end = min(offset + 1024, len(data) - 4)
            found = False
            search_data = np.frombuffer(data[offset:search_end], dtype=np.uint32)
            header_matches = np.where(search_data == 0x5A5A5A5A)[0]
            if len(header_matches) > 0:
                search_offset = offset + int(header_matches[0]) * 4
                offset = search_offset
                header = 0x5A5A5A5A
                found = True
                if header_matches[0] > 0:
                    print(f"Info: 在偏移量 {search_offset} 处重新同步帧头")
            if not found:
                break

        if offset + 12 > len(data):
            print(f"Warning: 文件截断, 无法读取帧头后的元数据, 偏移量: {offset}")
            break

        frame_index_raw = frame_index_fmt.unpack_from(data, offset + 4)[0]
        frame_index = frame_index_raw & 0x0F

        range_fft_num = range_fft_fmt.unpack_from(data, offset + 8)[0]

        if range_fft_num != num_rx * num_chirps * num_range_bins:
            print(f"Warning: 帧 {frame_index} 的FFT数量({range_fft_num})与"
                  f"期望值{num_rx * num_chirps * num_range_bins}不符, 将继续解析")

        data_bytes = range_fft_num * 4

        if offset + 12 + data_bytes + 4 > len(data):
            print(f"Warning: 帧 {frame_index} 数据不完整, 偏移量: {offset}")
            break

        frame_data_offset = offset + 12

        raw_data = np.zeros(range_fft_num, dtype=np.complex64)
        if range_fft_num > 0:
            int16_data = np.frombuffer(
                data[frame_data_offset:frame_data_offset + data_bytes],
                dtype=np.int16)
            real_parts = int16_data[0::2]
            imag_parts = int16_data[1::2]
            raw_data = real_parts + 1j * imag_parts

        tailer_offset = frame_data_offset + data_bytes
        tailer = tailer_fmt.unpack_from(data, tailer_offset)[0]
        if tailer != 0xA5A5A5A5:
            print(f"Warning: 帧 {frame_index} 尾标无效 (0x{tailer:08X}), "
                  f"但数据已解析, 偏移量: {tailer_offset}")

        data_cube = None
        if range_fft_num == num_rx * num_chirps * num_range_bins:
            reshaped_data = raw_data.reshape((num_chirps, num_rx, num_range_bins))
            # (chirp, rx, range) -> (rx, chirp, range)
            data_cube = np.transpose(reshaped_data, (1, 0, 2))

        frames.append(data_cube)

        offset = tailer_offset + 4
        frame_count += 1
        if max_frame_num is not None and frame_count >= max_frame_num:
            break

    print(f"Info: 共解析 {len(frames)} 个有效帧")
    return np.array(frames)
# ===================== 内联依赖结束 =====================

# 中文字体
plt.rcParams["font.sans-serif"] = ["Arial Unicode MS", "PingFang SC", "SimHei"]
plt.rcParams["axes.unicode_minus"] = False

OVERFLOW = 32747
RX = 0
CHIRP = 0

# ---- 呼吸/心率/频谱参数 ----
FPS = 50.0                  # 帧率
RESP_BAND = (0.1, 0.5)      # 呼吸带 6~30 bpm
HEART_BAND = (0.8, 2.0)     # 心跳带 48~120 bpm
IQ_BAND = (0.1, 0.8)        # IQ 图呼吸带通 (看呼吸起伏)
FILTER_ORDER = 4


def _bandpass(x, band, order=FILTER_ORDER, fs=FPS):
    b, a = butter(order, [band[0], band[1]], btype="band", fs=fs)
    if x.shape[0] <= 3 * max(len(a), len(b)):
        return x
    return filtfilt(b, a, x)


def _find_peak(freqs, mag, band):
    m = (freqs >= band[0]) & (freqs <= band[1])
    if not np.any(m):
        return None
    return freqs[m][int(np.argmax(mag[m]))]

DEFAULT_BIN_FILE = Path("标准20次呼吸/range_data_2026-07-22_17:45:44.bin")
DEFAULT_RANGE_BIN = 2


def main():
    bin_file = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_BIN_FILE
    range_bin = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_RANGE_BIN

    if not bin_file.exists():
        raise FileNotFoundError(f"未找到 bin 文件: {bin_file}")

    frames = parse_radar_frames(bin_file)  # (N, rx, chirp, bin)
    if frames.ndim != 4:
        raise ValueError(f"数据维度异常: {frames.shape}")
    print(f"帧数={frames.shape[0]}, shape={frames.shape}, 使用 bin{range_bin}")

    sig = frames[:, RX, CHIRP, range_bin]     # 复数时序 (N,)
    i_vals = np.real(sig)
    q_vals = np.imag(sig)
    amp = np.abs(sig)

    # 去直流(减均值)后的交流分量
    i_ac = i_vals - i_vals.mean()
    q_ac = q_vals - q_vals.mean()

    x = np.arange(len(sig))

    # 饱和统计：I 或 Q 触及 ±OVERFLOW 即视为饱和帧
    sat_mask = (np.abs(i_vals) >= OVERFLOW) | (np.abs(q_vals) >= OVERFLOW)
    sat_count = int(np.sum(sat_mask))
    sat_ratio = sat_count / len(sig) if len(sig) else 0.0
    print(f"饱和帧数={sat_count}/{len(sig)} ({sat_ratio*100:.4f}%)")

    fig = plt.figure(figsize=(16, 14))
    ax1 = fig.add_subplot(2, 2, 1)
    ax2 = fig.add_subplot(2, 2, 2)
    ax3 = fig.add_subplot(2, 2, 3)
    ax4 = fig.add_subplot(2, 2, 4)
    title_tag = f"{bin_file.name}  bin{range_bin} (rx{RX}/chirp{CHIRP})"

    # --- 图1: IQ 原始时序 ---
    ax1.plot(x, i_vals, color="tab:blue", linewidth=0.8, label="I (实部)")
    ax1.plot(x, q_vals, color="tab:orange", linewidth=0.8, label="Q (虚部)")
    ax1.set_title(f"① IQ 原始信号  {title_tag}")
    ax1.set_ylabel("幅度")
    ax1.grid(True, alpha=0.3)
    ax1.legend(loc="upper right", fontsize=9)

    # --- 图2: IQ 幅值时序 ---
    ax2.plot(x, amp, color="tab:green", linewidth=0.8)
    ax2.set_title(f"② IQ 幅值 |I + jQ| 随时间变化  (均值 {amp.mean():.0f})")
    ax2.set_ylabel("幅值")
    ax2.grid(True, alpha=0.3)

    # --- 图3: 去直流后的交流信号 ---
    ax3.plot(x, i_ac, color="tab:blue", linewidth=0.8, label="I 交流")
    ax3.plot(x, q_ac, color="tab:orange", linewidth=0.8, label="Q 交流")
    ax3.set_title("③ 去直流后的交流信号 (I/Q 减去各自均值)")
    ax3.set_xlabel("帧索引")
    ax3.set_ylabel("交流幅度")
    ax3.grid(True, alpha=0.3)
    ax3.legend(loc="upper right", fontsize=9)

    # --- 图4: IQ 复平面星座图 (判断饱和) ---
    # 饱和时散点会被削平贴在 ±OVERFLOW 边界, 形成方框边缘/直线
    sc_color = np.where(sat_mask, "red", "tab:purple")
    ax4.scatter(i_vals, q_vals, s=6, c=sc_color, alpha=0.5, linewidths=0)
    ax4.set_aspect("equal", adjustable="box")
    sat_tag = "饱和!" if sat_ratio > 0 else "未饱和"
    ax4.set_title(f"④ IQ 复平面星座图  [{sat_tag}] 饱和帧 {sat_count}/{len(sig)} ({sat_ratio*100:.3f}%)")
    ax4.set_xlabel("I (实部)")
    ax4.set_ylabel("Q (虚部)")
    ax4.grid(True, alpha=0.3)

    fig.tight_layout()
    out_path = bin_file.parent / f"{bin_file.stem}_iq_analysis.png"
    fig.savefig(out_path, dpi=150)
    print(f"已保存: {out_path}")

    # ===================== 附加图 1: 呼吸 IQ 起伏 (static_iq_signal) =====================
    t = np.arange(len(sig)) / FPS
    i_bp = _bandpass(i_vals.astype(float), IQ_BAND)
    q_bp = _bandpass(q_vals.astype(float), IQ_BAND)
    # 呼吸带 IQ 起伏幅度 (峰峰值 / 标准差)
    i_pp = float(i_bp.max() - i_bp.min())
    q_pp = float(q_bp.max() - q_bp.min())
    print(f"呼吸带({IQ_BAND[0]}~{IQ_BAND[1]}Hz) IQ 起伏: I 峰峰={i_pp:.1f} std={i_bp.std():.1f}, "
          f"Q 峰峰={q_pp:.1f} std={q_bp.std():.1f}")

    figI, (axi1, axi2) = plt.subplots(2, 1, figsize=(14, 8))
    axi1.plot(t, i_vals, color="#1f77b4", lw=0.8, label="I (实部)")
    axi1.plot(t, q_vals, color="#d62728", lw=0.8, label="Q (虚部)")
    axi1.set_title(f"原始 IQ 信号  {title_tag}")
    axi1.set_ylabel("IQ 幅值")
    axi1.grid(True, alpha=0.3)
    axi1.legend(loc="upper right", fontsize=9)

    axi2.plot(t, i_bp, color="#1f77b4", lw=1.0, label="I 带通")
    axi2.plot(t, q_bp, color="#d62728", lw=1.0, label="Q 带通")
    axi2.set_title(f"呼吸带通 {IQ_BAND[0]}~{IQ_BAND[1]}Hz  IQ 起伏  "
                   f"(I峰峰={i_pp:.1f} Q峰峰={q_pp:.1f})")
    axi2.set_xlabel("时间 (s)")
    axi2.set_ylabel("交流幅值")
    axi2.grid(True, alpha=0.3)
    axi2.legend(loc="upper right", fontsize=9)
    figI.tight_layout()
    out_iq = bin_file.parent / f"{bin_file.stem}_static_iq_signal.png"
    figI.savefig(out_iq, dpi=150)
    print(f"已保存: {out_iq}")

    # ===================== 附加图 2: 相位频谱 (static_phase_spectrum) =====================
    phase = np.unwrap(np.angle(sig))
    phase = detrend(phase, type="linear")
    N = len(phase)
    b_bp, a_bp = butter(4, [RESP_BAND[0], HEART_BAND[1]], btype="band", fs=FPS)
    try:
        xph = filtfilt(b_bp, a_bp, phase)
    except Exception:
        xph = phase
    win = np.hanning(N)
    mag = np.abs(np.fft.rfft(xph * win))
    freqs = np.fft.rfftfreq(N, d=1.0 / FPS)
    fr = _find_peak(freqs, mag, RESP_BAND)
    fh = _find_peak(freqs, mag, HEART_BAND)
    resp_bpm = fr * 60 if fr else float("nan")
    heart_bpm = fh * 60 if fh else float("nan")
    print(f"呼吸 {resp_bpm:.1f}bpm  心率 {heart_bpm:.1f}bpm")

    figS, (axs1, axs2) = plt.subplots(2, 1, figsize=(14, 9))
    band_hz = freqs <= 2.5
    axs1.plot(freqs[band_hz], mag[band_hz], color="tab:purple", lw=1.2)
    if fr:
        axs1.axvline(fr, color="gray", ls="--", lw=1.0)
    if fh:
        axs1.axvline(fh, color="orange", ls=":", lw=1.0)
    axs1.axvspan(*RESP_BAND, color="gray", alpha=0.10)
    axs1.axvspan(*HEART_BAND, color="orange", alpha=0.10)
    axs1.set_title(f"相位频谱 (Hz)  呼吸{resp_bpm:.1f}bpm  心率{heart_bpm:.1f}bpm  "
                   f"灰:呼吸带{RESP_BAND}  橙:心跳带{HEART_BAND}")
    axs1.set_xlabel("频率 (Hz)")
    axs1.set_ylabel("幅度")
    axs1.grid(True, alpha=0.3)

    bpm = freqs * 60
    band_bpm = bpm <= 150
    axs2.plot(bpm[band_bpm], mag[band_bpm], color="tab:green", lw=1.2)
    axs2.axvspan(RESP_BAND[0] * 60, RESP_BAND[1] * 60, color="gray", alpha=0.10)
    axs2.axvspan(HEART_BAND[0] * 60, HEART_BAND[1] * 60, color="orange", alpha=0.10)
    axs2.set_title("相位频谱 (bpm)")
    axs2.set_xlabel("频率 (bpm 次/分)")
    axs2.set_ylabel("幅度")
    axs2.grid(True, alpha=0.3)
    figS.tight_layout()
    out_sp = bin_file.parent / f"{bin_file.stem}_static_phase_spectrum.png"
    figS.savefig(out_sp, dpi=150)
    print(f"已保存: {out_sp}")


if __name__ == "__main__":
    main()