"""单路实数ADC -> 距离FFT -> 固定距离仓跨包IQ。保持雷达固件不变。

输入：8B头0102030405060708 + 512个小端int16样点，支持纯hex文本或bin。
运行：python analyze_single_rx_adc_iq.py data/采集.txt --range-bin 20
不指定文件时寻找脚本旁data目录最新的Serial Debug*.txt，绝不读取20.bin。
"""
from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import asdict, dataclass
from html import escape
import json
from pathlib import Path
import re
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


@dataclass(frozen=True)
class Waveform:
    samples: int = 512
    adc_clock_hz: float = 20e6
    decimation: int = 2
    bandwidth_hz: float = 3.22e9
    sweep_us: float = 55
    start_frequency_hz: float = 58.5e9
    chirp_interval_us: float = 80
    hardware_tx: int = 4
    hardware_rx: int = 4

    @property
    def fs(self):
        return self.adc_clock_hz / self.decimation

    @property
    def slope(self):
        return self.bandwidth_hz / (self.sweep_us * 1e-6)


WAVEFORM = Waveform()  # 对应当前源码；一次仅导出1个RX槽、1条Chirp
HEADER = bytes(range(1, 9))
C = 299_792_458.0


def read_bytes(path):
    raw = Path(path).read_bytes()
    try:
        text = raw.decode("utf-8-sig")
    except UnicodeDecodeError:
        return raw, "binary"
    cleaned = re.sub(r"0[xX]", "", text)
    if cleaned.strip() and re.fullmatch(r"[0-9a-fA-F\s]+", cleaned):
        return bytes.fromhex(cleaned), "hex text"
    return raw, "binary"


def parse_packets(data, samples=512):
    """解码为(N,512)实数；包头不是帧号，无法据此检查丢帧。"""
    positions = [m.start() for m in re.finditer(re.escape(HEADER), data)]
    if not positions:
        raise ValueError("未发现ADC包头0102030405060708。不能输入中控点云日志或66BB/5A距离FFT包。")
    packet_bytes = 8 + samples * 2
    if len(positions) > 1:
        spacing, count = Counter(np.diff(positions)).most_common(1)[0]
        if spacing != packet_bytes and count > (len(positions)-1)/2:
            raise ValueError(f"主要包头间隔为{spacing}字节，当前单路512点格式应为{packet_bytes}，请确认固件导出方式。")
    arrays, offsets = [], []
    broken = 0
    for i, start in enumerate(positions):
        end = start + packet_bytes
        if end > len(data) or (i+1 < len(positions) and positions[i+1] < end):
            broken += 1
            continue
        arrays.append(np.frombuffer(data, dtype="<i2", count=samples, offset=start+8).copy())
        offsets.append(start)
    if not arrays:
        raise ValueError("没有完整512点ADC包。")
    prefix = data[:positions[0]].decode("ascii", errors="replace")
    boot = dict(re.findall(r"(?m)^([A-Za-z][A-Za-z0-9_]*)=([^\r\n]+)", prefix))
    return np.stack(arrays).astype(np.float64), {
        "packets":len(arrays), "samples_per_packet":samples,
        "packet_bytes":packet_bytes, "decoded_bytes":len(data),
        "prefix_bytes":positions[0], "incomplete_or_broken_packets":broken,
        "trailing_bytes":max(0,len(data)-(offsets[-1]+packet_bytes)),
        "offsets":offsets,"boot_parameters":boot,
        "layout":"one exported ADC memory slot, one chirp per packet; little-endian signed int16",
    }


def compute_fft(adc, nfft):
    if nfft < adc.shape[1]:
        raise ValueError("nfft不能小于ADC样点数。")
    dc = adc - adc.mean(axis=1, keepdims=True)
    win = np.hanning(adc.shape[1])
    windowed = dc * win
    spectrum = np.fft.rfft(windowed, n=nfft, axis=1) / win.sum()
    spectrum[:,1:-1 if nfft % 2 == 0 else None] *= 2
    return dc, windowed, spectrum


def relative_db(x):
    mag=np.abs(x)
    return 20*np.log10(np.maximum(mag,1e-12)/max(float(mag.max()),1e-12))


def save_figure(fig, output, filename, title, sidebar):
    fig.suptitle(title,x=.36,y=.97,fontsize=15)
    fig.subplots_adjust(left=.07,right=.70,top=.89,bottom=.09,hspace=.5,wspace=.35)
    fig.text(.73,.89,sidebar,va="top",fontsize=9,linespacing=1.45,
             bbox={"boxstyle":"round,pad=.7","facecolor":"#eef4f8","edgecolor":"#bbccd8"})
    for ax in fig.axes:
        ax.grid(alpha=.2)
    fig.savefig(output/filename,dpi=145)
    plt.close(fig)
    return filename


def process(args):
    wave=WAVEFORM
    if args.input is None:
        root=Path(__file__).resolve().parent
        files=list((root/"data").glob("Serial Debug*.txt")) or list(root.glob("Serial Debug*.txt"))
        if not files:
            raise ValueError("未找到采集文件，请在命令行提供文件路径。")
        args.input=max(files,key=lambda p:p.stat().st_mtime)
    data,kind=read_bytes(args.input)
    adc,diagnostics=parse_packets(data,wave.samples)
    if not 0 <= args.packet < len(adc):
        raise ValueError(f"packet需在0到{len(adc)-1}之间。")
    if args.fps is not None and (not np.isfinite(args.fps) or args.fps <= 0):
        raise ValueError("fps必须是实际测得的正数，不能用10MHz或Chirp间隔换算。")
    dc,windowed,fft=compute_fft(adc,args.nfft)
    ranges=np.fft.rfftfreq(args.nfft,1/wave.fs)*C/(2*wave.slope)
    profile=np.abs(fft).mean(axis=0)
    search=(ranges>=args.min_range)&(ranges<=args.max_range)
    if not np.any(search):
        raise ValueError("指定距离区间没有距离bin。")
    if args.range_bin is not None:
        k=args.range_bin
        choice="手动指定距离仓"
    else:
        k=int(np.flatnonzero(search)[np.argmax(profile[search])])
        choice="区间内最强幅度仓（不等于人体）"
    if not 0 <= k < len(ranges):
        raise ValueError("range-bin超出FFT范围。")
    iq=fft[:,k]  # 与参考脚本frames[:, RX, CHIRP, range_bin]语义对应
    i,q=iq.real,iq.imag
    i_ac,q_ac=i-i.mean(),q-q.mean()
    index=np.arange(len(iq))
    x=index if args.fps is None else index/args.fps
    xlabel="采集包序号（不是Chirp内样点）" if args.fps is None else "时间 (s，按提供帧率换算)"
    low,high=float(adc.min()),float(adc.max())
    low_rate,high_rate=float(np.mean(adc==low)),float(np.mean(adc==high))
    sidebar=(f"单路ADC → 电脑FFT → 固定bin IQ\n有效包: {len(adc)}，每包512个实数\n"
        f"当前ADC图: 包{args.packet}\n数据: {kind} / little-endian int16\n"
        f"硬件: 4TX / 4RX，当前仅导出1流\n\n"
        f"波形参数：当前源码\n原采样: 20 MHz，抽取/2\n有效采样: 10 MHz\n带宽: 3.22 GHz\n扫频: 55 us\n"
        f"起始频率: 58.5 GHz\nChirp间隔: 80 us\n\n"
        f"离线FFT: {args.nfft}点 / Hann窗\n距离网格: {ranges[1]-ranges[0]:.5f} m\n"
        f"选仓: bin{k} ≈ {ranges[k]:.3f} m\n{choice}\n"
        "距离换算依赖上述源码参数\n未复现ZO/NCO及硬件校准\n\n"
        f"观测码值: {low:g} 至 {high:g}\n极小/极大值占比:\n{low_rate:.2%} / {high_rate:.2%}\n"
        "占比不是标定饱和率\n\n"
        +(f"实际采集率（用户提供）: {args.fps:g} Hz" if args.fps else "没有时间戳：只画包序号\n不估计呼吸或心率"))
    output=args.output or Path(__file__).resolve().parent/"single_rx_results"/args.input.stem
    output.mkdir(parents=True,exist_ok=True)
    plt.rcParams["font.sans-serif"]=["Microsoft YaHei","SimHei","DejaVu Sans"]
    plt.rcParams["axes.unicode_minus"]=False
    images=[]
    fast_us=np.arange(wave.samples)/wave.fs*1e6
    fig,axes=plt.subplots(3,1,figsize=(16,11))
    for ax,y,label in zip(axes,[adc[args.packet],dc[args.packet],windowed[args.packet]],
                          ["原始实数ADC","每条Chirp减去自身均值","去均值后乘Hann窗"]):
        ax.plot(fast_us,y,lw=1)
        ax.set(title=label,xlabel="Chirp内采样时间 (us)",ylabel="ADC code")
    images.append(save_figure(fig,output,"01_adc_preprocess.png","01 单路ADC：原始 → 去直流 → 加窗",sidebar))
    show=ranges<=args.max_range
    fig,axes=plt.subplots(3,1,figsize=(16,11))
    axes[0].plot(ranges[show],np.abs(fft[args.packet,show]))
    axes[0].set(title=f"包{args.packet}：距离FFT幅值（相干增益归一化）",ylabel="Amplitude")
    axes[1].plot(ranges[show],relative_db(fft[args.packet])[show])
    axes[1].set(title="同一包的相对幅度谱",ylabel="Relative dB")
    axes[2].plot(ranges[show],profile[show])
    axes[2].set(title="全部包平均幅值谱：选择同一距离仓",ylabel="Mean amplitude")
    for ax in axes:
        ax.axvline(ranges[k],color="crimson",ls="--",label=f"bin{k}")
        ax.set_xlabel("按源码参数换算的距离 (m)")
        ax.legend()
    images.append(save_figure(fig,output,"02_range_fft.png","02 实数ADC → 复数距离FFT",sidebar))
    # 仿照plot_iq_analysis.py的四宫格，不将ADC量程阈值套到FFT结果。
    fig,axes=plt.subplots(2,2,figsize=(16,11))
    axes[0,0].plot(x,i,label="I = FFT.real")
    axes[0,0].plot(x,q,label="Q = FFT.imag")
    axes[0,0].set(title="固定距离bin的IQ跨包变化",xlabel=xlabel,ylabel="FFT amplitude")
    axes[0,0].legend()
    axes[0,1].plot(x,np.abs(iq),color="green")
    axes[0,1].set(title="幅值 sqrt(I²+Q²)",xlabel=xlabel,ylabel="Magnitude")
    axes[1,0].plot(x,i_ac,label="I - mean(I)")
    axes[1,0].plot(x,q_ac,label="Q - mean(Q)")
    axes[1,0].set(title="IQ去复数均值：交流变化",xlabel=xlabel,ylabel="AC amplitude")
    axes[1,0].legend()
    axes[1,1].scatter(i,q,c=index,cmap="viridis",s=18)
    axes[1,1].plot(i,q,lw=.5,alpha=.4)
    halfspan=max(float(np.ptp(i)),float(np.ptp(q)),1.)*.65
    ci,cq=float((i.min()+i.max())/2),float((q.min()+q.max())/2)
    axes[1,1].set(title="原始IQ复平面轨迹（颜色按包序）",xlabel="I",ylabel="Q",
        xlim=(ci-halfspan,ci+halfspan),ylim=(cq-halfspan,cq+halfspan))
    axes[1,1].set_aspect("equal",adjustable="box")
    images.append(save_figure(fig,output,"03_iq_analysis.png","03 参考布局：IQ / 幅值 / 交流IQ / 星座图",sidebar))
    fig,axes=plt.subplots(2,1,figsize=(16,11))
    axes[0].plot(x,np.angle(iq),label="wrapped phase")
    axes[0].plot(x,np.unwrap(np.angle(iq)),label="unwrap phase")
    axes[0].set(title="原始IQ相位与解缠（弱回波处可能不可靠）",xlabel=xlabel,ylabel="Phase (rad)")
    axes[0].legend()
    timespan=(0,max(len(iq)-1,1)) if args.fps is None else (0,max(len(iq)-1,1)/args.fps)
    im=axes[1].imshow(relative_db(fft[:,show]),origin="lower",aspect="auto",cmap="turbo",
        extent=[ranges[show][0],ranges[show][-1],*timespan])
    axes[1].axvline(ranges[k],color="white",ls="--")
    axes[1].set(title="全部包距离幅度图",xlabel="按源码参数换算的距离 (m)",ylabel=xlabel)
    fig.colorbar(im,ax=axes[1],label="Relative dB")
    images.append(save_figure(fig,output,"04_phase_range_map.png","04 相位及距离—包序列观察",sidebar))
    np.savez_compressed(output/"intermediate.npz",adc=adc,dc_removed=dc,windowed=windowed,
        range_fft=fft,range_m=ranges,selected_iq=iq,iq_ac=i_ac+1j*q_ac)
    np.savetxt(output/"iq.csv",np.column_stack([index,x,i,q,np.abs(iq),i_ac,q_ac,np.angle(iq),np.unwrap(np.angle(iq))]),
        delimiter=",",header="packet,x_axis,I,Q,magnitude,I_ac,Q_ac,phase_rad,unwrap_rad",comments="")
    report={"input":str(args.input.resolve()),"waveform":asdict(wave),"parser":diagnostics,
        "selection":{"bin":k,"range_m":float(ranges[k]),"method":choice},"actual_fps":args.fps,
        "observed_adc_min":low,"observed_adc_max":high,"min_fraction":low_rate,"max_fraction":high_rate,
        "notes":["每包一条导出Chirp，不是全帧4T4R数据。","FFT复数I/Q不是相邻ADC样点交错解码。",
                 "IQ四宫格使用同一距离仓跨包序列。","FFT幅值未标定为电压或功率，未使用32747作为饱和阈值。",
                 "源码参数不包含实际包间时间；只有提供实测fps才能换算秒。"],"images":images}
    (output/"report.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    body="".join(f'<h2>{escape(name)}</h2><img src="{name}" style="width:100%">' for name in images)
    (output/"index.html").write_text('<!doctype html><meta charset="utf-8"><title>单路ADC和IQ</title>'
        '<body style="font-family:sans-serif;max-width:1600px;margin:auto"><h1>单路 ADC → 距离FFT → IQ</h1>'
        f'<p>输入：{escape(args.input.name)}；{len(adc)}包；bin{k}。IQ不必是标准正余弦。</p>'+body+'</body>',encoding="utf-8")
    print(json.dumps({"input":str(args.input),"output":str(output.resolve()),"packets":len(adc),
        "selected_bin":k,"range_m":float(ranges[k]),"plots":len(images)},ensure_ascii=False,indent=2))
    return report


def build_parser():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("input",nargs="?",type=Path)
    p.add_argument("--output",type=Path)
    p.add_argument("--packet",type=int,default=0,help="ADC和单包FFT图查看哪一包，0开始")
    p.add_argument("--range-bin",type=int,help="IQ固定距离仓；不提供时自动选强度最大仓")
    p.add_argument("--min-range",type=float,default=.2)
    p.add_argument("--max-range",type=float,default=8.)
    p.add_argument("--nfft",type=int,default=512)
    p.add_argument("--fps",type=float,help="仅填实测均匀采集率；不影响ADC快时间采样率")
    return p


if __name__=="__main__":
    try:
        process(build_parser().parse_args())
    except (ValueError,OSError) as exc:
        print("无法处理:",exc,file=sys.stderr)
        sys.exit(2)
