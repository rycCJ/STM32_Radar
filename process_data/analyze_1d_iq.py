"""解析多谱150-bin float32距离FFT，生成与plot_iq_analysis.py类似的IQ诊断图。

输入可以为纯hex串口文本或二进制；默认使用相邻点云包的帧间隔重建相对时间。
"""
import argparse
import html
import json
from pathlib import Path
import re
import sys

import numpy as np
import matplotlib
# 使用不需要桌面窗口的后端：运行时直接保存PNG，不调用交互式绘图窗口。
matplotlib.use('Agg')
import matplotlib.pyplot as plt


# 【输入层】只处理文件存储格式，不在这里做信号滤波或FFT。
# 返回值：(实际串口字节流bytes，文件类型说明)。
def read_bytes(path):
    data=Path(path).read_bytes()
    try:
        text=data.decode('utf-8-sig')
    except UnicodeDecodeError:
        return data,'binary'
    # 串口助手的hex文本如“66 BB 00 58 ...”需要先还原成字节。
    # 使用全字符串匹配，避免把带时间戳、地址列、说明文字的文本错误当作纯hex。
    # utf-8-sig还会去掉文本文件可能带有的UTF-8 BOM。
    if re.fullmatch(r'[0-9A-Fa-f\s]+',text):
        return bytes.fromhex(text),'hex text'
    return data,'binary'


# 【解帧层】将连续串口流拆成“帧×距离仓”的复数矩阵。
# 注意：雷达已经完成距离维FFT，本函数仅解码，不再对150个bin做一次FFT。
# 返回：frames.shape=(有效帧数,bins)，meta为逐帧元信息，diag为完整性统计。
def parse_1d(data, bins=150):
    """正确配对两个连续数组，不能按交错int16/float解析。

    一个FFT块: 66BB | real[bins] float32 LE | imag[bins] float32 LE | BB66。
    可选点云包: 55AA | u32 length | u16 interval_ms | u8 numTLV |
    u8 type | u16 point_count | 9B*point_count。
    """
    # 头2B + I数组(bins×4B) + Q数组(bins×4B) + 尾2B。
    # bins=150时为1204B；数据块本身没有长度字段，所以必须与固件配置一致。
    size=4+8*bins
    frames,meta=[],[]
    pos=0
    diag=dict(decoded_bytes=len(data),fft_bytes=size,invalid_tail=0,nonfinite=0,
              skipped_bytes=0,point_packets=0,invalid_point_packets=0,trailing_bytes=0)
    # pos指向尚未消费的字节；遇到坏帧时继续搜索下一候选头，尝试重新同步。
    while pos<len(data):
        start=data.find(b'\x66\xbb',pos)
        if start<0:
            diag['trailing_bytes']=len(data)-pos
            break
        # 可能跳过启动日志、杂字节或未成功解析的点云内容。
        # 跳过字节不一定等于ADC样点丢失，具体需结合采集协议判断。
        diag['skipped_bytes']+=start-pos
        end=start+size
        if end>len(data):
            diag['trailing_bytes']=len(data)-start
            break
        # 按固定包长检查尾标，不能只凭“找到66BB”就接受数据。
        # 坏尾时只前进1字节，防止跳过后面实际存在的有效帧头。
        if data[end-2:end]!=b'\xbb\x66':
            diag['invalid_tail']+=1
            pos=start+1
            continue
        # '<f4'：小端IEEE754单精度float32，每个数4字节。
        # 跳过2B头，连续读出2*bins个数；随后升到float64便于后续数值计算。
        # 提升精度不会恢复float32传输时已经丢失的信息，也不会改变原始数值。
        values=np.frombuffer(data,dtype='<f4',count=2*bins,offset=start+2).astype(np.float64)
        # NaN/Inf会破坏幅值、均值及相位计算；这里拒绝整块，不将它们填成0。
        if not np.isfinite(values).all():
            diag['nonfinite']+=1
            pos=end
            continue
        # FFT块后可能紧接55AA点云包。点云不是本次绘制的IQ来源，
        # 读取它主要用于跳过变长内容、校验边界，并提取固件帧间隔。
        item=dict(offset=start,interval_ms=None,point_count=None,point_payload_bytes=None)
        pos=end
        if data[pos:pos+2]==b'\x55\xaa' and pos+12<=len(data):
            # 点云包内偏移：0..1头，2..5长度，6..7间隔(ms)，
            # 8为numTLV，9为type，10..11为点数。整数也按小端读取。
            length=int.from_bytes(data[pos+2:pos+6],'little')
            count=int.from_bytes(data[pos+10:pos+12],'little')
            packet_end=pos+6+length
            # 本实现只接受numTLV=1、type=1的点云布局。
            # length不含2B帧头和4B长度字段：6B固定字段 + 每点9B。
            # 检查长度、点数及文件边界相符后，才接受时间信息并消费整个包。
            if (length==6+9*count and packet_end<=len(data)
                    and data[pos+8:pos+10]==b'\x01\x01'):
                item.update(interval_ms=int.from_bytes(data[pos+6:pos+8],'little'),
                            point_count=count,point_payload_bytes=length)
                diag['point_packets']+=1
                pos=packet_end
            else:
                diag['invalid_point_packets']+=1
        # 关键配对方式：前150个数是I[0..149]，后150个数是Q[0..149]。
        # 第k个距离仓为I[k]+j*Q[k]，不是I0,Q0,I1,Q1那种交错排列。
        # 1j是Python的虚数单位；输出每一行是一帧的完整复数距离谱。
        frames.append(values[:bins]+1j*values[bins:])
        meta.append(item)
    if not frames:
        raise ValueError('没有完整float32 1DFFT块；检查是否ADC/点云日志，或--bins是否匹配固件。')
    diag['frames']=len(frames)
    return np.asarray(frames),meta,diag


# 【慢时间轴】这里处理“帧与帧之间”的时间，不是ADC快时间采样率。
# 10MHz描述一条Chirp内采样间隔；约10Hz描述本文件每秒输出多少帧。
# 二者不能混用，否则频谱Hz/bpm坐标会相差多个数量级。
def time_axis(meta,fps=None):
    if fps is not None:
        if not np.isfinite(fps) or fps<=0:raise ValueError('fps必须为有限正数')
        return np.arange(len(meta))/fps,dict(source='user fps',fps=fps,uniform=True)
    # 第n帧携带的是它与上一帧的间隔；把文件首帧设为t=0，
    # 首帧间隔涉及文件外的前一帧，因此从meta[1:]开始累加。
    # 毫秒除以1000转秒；缺少时间信息时临时置0，让下面的有效性检查失败。
    dt=np.array([m['interval_ms'] or 0 for m in meta[1:]],float)/1000
    if len(dt) and np.all(dt>0):
        # cumsum构造相对时间[0,dt1,dt1+dt2,...]。
        # 用间隔中位数估计帧率，减少ms取整及少量抖动的影响。
        # uniform仅表示最大间隔偏差小于中位间隔2%，不是“绝对无丢帧”的证明。
        return np.r_[0,np.cumsum(dt)],dict(source='firmware point packet interval (rounded ms)',
            fps=float(1/np.median(dt)),uniform=bool(np.max(np.abs(dt-np.median(dt)))<=.02*np.median(dt)+1e-9),
            interval_min_ms=float(dt.min()*1000),interval_max_ms=float(dt.max()*1000))
    # 无可靠时间则退回帧索引；此时只能描述跨帧变化，不进行Hz频谱换算。
    return np.arange(len(meta)),dict(source='frame index; no valid time',fps=None,uniform=False)


def spectrum_segment(meta,fps=None):
    """频谱仅用最长的有效连续时间段，保留IQ原始全帧，不插补未知间隔。

    帧i中的interval是(i-1)->i的边；缺失边两侧不能直接拼接。
    相比中位间隔超过2.5倍的边视为大间隙，避免跨越它插值滤波。
    首帧的interval涉及文件外数据，因此不要求它有效。
    """
    n=len(meta)
    if fps is not None:
        t,info=time_axis(meta,fps)
        return slice(0,n),t,dict(info,start_frame=0,end_frame=n-1,
            used_frames=n,excluded_frames=0,missing_interval_frames=[])
    intervals=np.array([m.get('interval_ms') or 0 for m in meta],float)/1000
    good=np.isfinite(intervals)&(intervals>0)
    usable=intervals[1:][good[1:]]
    if not len(usable):
        return slice(0,0),np.array([],float),dict(source='no valid interval',fps=None,
            uniform=False,used_frames=0,excluded_frames=n,reason='没有可建立Hz时间轴的有效连续间隔')
    median=float(np.median(usable))
    good &= intervals<=2.5*median
    start=0
    runs=[]
    for i in range(1,n):
        if not good[i]:
            runs.append((start,i))
            start=i
    runs.append((start,n))
    start,stop=max(runs,key=lambda r:r[1]-r[0])
    if stop-start<2:
        return slice(0,0),np.array([],float),dict(source='no continuous timing',fps=None,
            uniform=False,used_frames=0,excluded_frames=n,reason='没有至少两帧的连续有效时间段')
    t,info=time_axis(meta[start:stop])
    info.update(start_frame=start,end_frame=stop-1,used_frames=stop-start,
        excluded_frames=n-(stop-start),duration_s=float(t[-1]),
        missing_interval_frames=[int(i) for i in range(1,n) if not np.isfinite(intervals[i]) or intervals[i]<=0],
        large_gap_frames=[int(i) for i in range(1,n) if intervals[i]>2.5*median],
        reason='仅使用最长连续有效时间段，未推测或跨越缺失间隔')
    return slice(start,stop),t,info


# 【绘图公共函数】右侧留出空白写波形/选仓/帧率参数，不改变信号数据。
def setup_figure(title,sidebar,rows=2,cols=1):
    fig,axes=plt.subplots(rows,cols,figsize=(16,max(8,rows*3.5)),squeeze=False)
    fig.subplots_adjust(left=.07,right=.74,bottom=.09,top=.90,hspace=.42,wspace=.28)
    fig.suptitle(title,x=.40,fontsize=15)
    fig.text(.77,.89,sidebar,va='top',fontsize=9,linespacing=1.55,
             bbox=dict(boxstyle='round,pad=.7',facecolor='#eef4f8',edgecolor='#bccbd5'))
    for ax in axes.flat:ax.grid(alpha=.22)
    return fig,axes


# series是多条y序列，例如[I,Q]；x是公共的秒数或帧索引。
def plot_lines(ax,x,series,labels,title,xlabel,ylabel):
    for y,label in zip(series,labels):ax.plot(x,y,lw=1,marker='.' if len(x)<100 else None,ms=3,label=label)
    ax.set(title=title,xlabel=xlabel,ylabel=ylabel)
    ax.legend(fontsize=8)


# 【分析主流程】变量含义：
# z[n,k]：第n帧、第k个距离仓的复数FFT值；shape=(N,K)。
# ranges[k]：按当前波形配置换算的距离；不是从文件测得的独立标定值。
# iq[n]：固定一个距离仓后得到的慢时间复数序列；shape=(N,)。
# 处理顺序：解帧→距离坐标→选仓→时间轴→IQ/幅值/去均值→相位→频谱。
def analyze(args):
    data,input_type=read_bytes(args.input)
    z,meta,diag=parse_1d(data,args.bins)
    if len(z)<2:raise ValueError('至少需要两帧绘制跨帧IQ变化。')
    if args.bin_spacing<=0:raise ValueError('bin-spacing必须大于0')
    # 1. 构造距离坐标。bin_spacing是每个距离仓对应的米数，
    # 对FMCW通常由c*Fs/(2*扫频斜率*Nfft)计算，依赖实际采集配置。
    # 默认0.050041m/bin只适用于本工程相应波形；改变带宽/采样率后需更新。
    ranges=np.arange(args.bins)*args.bin_spacing
    # 2. np.abs对复数计算模sqrt(I²+Q²)；axis=0沿帧维求平均，
    # 得到每个距离仓的平均幅值，shape由(N,K)变成(K,)。
    # 这是幅值平均（非相干统计），不是先将复数I/Q相加，也不是功率平均。
    mean_amp=np.abs(z).mean(axis=0)
    # 自动选仓只在用户给定距离范围内搜索，尽量避开近零频强泄漏。
    search=(ranges>=args.min_range)&(ranges<=args.max_range)
    if not search.any():raise ValueError('选仓距离范围没有可用bin')
    # 3. 手工指定--range-bin时直接使用该仓；否则取搜索区平均幅值最大者。
    # argmax返回的是筛选后数组的局部索引；flatnonzero负责映射回原始bin编号。
    # 最强仓可能是墙壁/耦合等杂波；此处没有CFAR、人体识别或动态跟踪。
    # k选定后整段记录保持不变，避免不断切仓造成相位不连续。
    k=args.range_bin if args.range_bin is not None else int(np.flatnonzero(search)[np.argmax(mean_amp[search])])
    if not 0<=k<args.bins:raise ValueError('range-bin越界')
    # 4. 获得相对时间。优先使用用户显式FPS，否则读取固件点云包的间隔。
    t,timing=time_axis(meta,args.fps)
    xlabel='Time (s)' if timing['fps'] is not None else 'Frame index'
    # 5. 固定k，取所有帧的该仓：iq[n]=I[n,k]+jQ[n,k]。
    # 这里没有跨RX或Chirp做相干平均，因为文件只导出一个固定提取流。
    # amp是这个距离仓的复数模，不是ADC电压，也不能直接与int16满量程比较。
    iq=z[:,k];amp=np.abs(iq)
    # 6. I与Q分别减去整段记录的均值，移除慢时间复数直流偏置。
    # 等价于 iq - mean(iq)。它只是平移复平面中心，不是带通滤波，
    # 不会自动去掉线性漂移，也没有拟合真实IQ圆心；不能保证消除所有静态杂波。
    # 如果原始均值接近0，处理前后的时序看起来几乎一样是正常的。
    i_ac=iq.real-iq.real.mean();q_ac=iq.imag-iq.imag.mean()
    out=args.output or args.input.parent/(args.input.stem+'_1d_iq')
    out.mkdir(parents=True,exist_ok=True)
    plt.rcParams['font.sans-serif']=['Microsoft YaHei','SimHei','DejaVu Sans']
    plt.rcParams['axes.unicode_minus']=False
    notes=["源代码使用自动局部realSUM/imageSUM跨case调用发送，存在生命周期风险；包格式校验不能证明数据未被栈覆盖。",
           "float FFT幅值不是ADC码值，不使用int16的±32747判断ADC饱和。",
           "自动选仓仅按平均幅值，强反射不等于人体；请结合目标距离手动指定bin。",
           "包中仅有舍入到ms的帧间隔，无递增帧号/CRC/主机时间戳，无法彻底排除丢帧。"]
    # 侧栏描述当前默认固件及数据选择，不会反向修改采集硬件。
    # 其中150bin、Chirp j=2、虚拟通道m=6等文本若固件改变，需要同步核对。
    side=(f'协议: 66BB + I[150] + Q[150] + BB66\n编码: 小端float32，两个连续数组\n'
          f'输入: {input_type}\n完整FFT块: {len(z)}, 每块{diag["fft_bytes"]}B\n'
          f'完整点云包: {diag["point_packets"]}\n距离仓: bin{k} ≈ {ranges[k]:.3f} m\n'
          f'IQ均值: {iq.real.mean():.4g}, {iq.imag.mean():.4g}\n\n'
          '波形/提取配置（来自当前源码）\n原采样20MHz，/2抽取→10MHz\n512点距离FFT，导出前150 bin\n'
          '扫频3.22GHz / 55us\nChirp间隔80us，硬件4TX×4RX\n'
          '提取Chirp索引j=2，虚拟槽m=6\n未进行跨Chirp/RX平均\n'
          f'距离系数: {args.bin_spacing:g} m/bin\n距离为配置条件换算\n\n')
    if timing['fps']:
        side+=f'帧率约{timing["fps"]:.3f}Hz\n相对记录跨度{t[-1]:.3f}s\n'
        side+='时间来自用户指定FPS\n' if args.fps else '时间来自点云包帧间隔，非50Hz\n'
    else:side+='没有有效时间信息，横轴用帧号\n'
    side+='float FFT无ADC满量程阈值\n原ADC是否饱和需另查原始ADC'
    imgs=[]
    def save(fig,name):
        fig.savefig(out/name,dpi=140)
        plt.close(fig)
        imgs.append(name)
    # 图1沿用参考脚本的四联布局：原始I/Q、模值、减均值I/Q、复平面。
    # 时序观察幅度/基线变化；复平面观察轨迹形状，颜色仅表示帧先后。
    fig,a=setup_figure('1DFFT固定距离仓：IQ四联图',side,2,2)
    plot_lines(a[0,0],t,[iq.real,iq.imag],['I','Q'],'① 原始IQ跨帧变化',xlabel,'FFT units')
    plot_lines(a[0,1],t,[amp],['|I+jQ|'],'② IQ幅值',xlabel,'FFT units')
    plot_lines(a[1,0],t,[i_ac,q_ac],['I - mean(I)','Q - mean(Q)'],'③ 去直流后的交流分量',xlabel,'FFT units')
    a[1,1].scatter(iq.real,iq.imag,c=np.arange(len(iq)),cmap='viridis',s=22)
    a[1,1].set(title='④ IQ复平面（颜色随帧号变化）',xlabel='I',ylabel='Q')
    a[1,1].set_aspect('equal',adjustable='box')
    save(fig,'01_iq_analysis.png')
    # 图2放大比较处理前后。当前展示的AC仅指减均值结果，未做呼吸频带滤波。
    fig,a=setup_figure('原始IQ与去直流IQ时序',side,2)
    plot_lines(a[0,0],t,[iq.real,iq.imag],['I','Q'],f'bin{k} 原始复数信号',xlabel,'FFT units')
    plot_lines(a[1,0],t,[i_ac,q_ac],['I AC','Q AC'],'仅减均值，不冒充呼吸带通结果',xlabel,'FFT units')
    save(fig,'02_static_iq_signal.png')
    # 图3展示全部距离仓，便于检查选中的强分量是否位于预期目标距离。
    fig,a=setup_figure('距离谱与距离—帧图：确认选仓位置',side,2)
    plot_lines(a[0,0],ranges,[mean_amp,np.abs(z[0])],['Mean magnitude','First frame'],'150个距离仓', 'Conditional range (m)','FFT units')
    a[0,0].axvline(ranges[k],color='red',ls='--')
    mag=np.abs(z)
    # 7. 距离—帧热图：整个矩阵以同一个全局最大幅值归一化，
    # dB=20*log10(幅值/全局最大幅值)，最大值为0dB，其他值不大于0dB。
    # 因为输入是幅度而不是功率，这里用20而非10；1e-12防止log(0)。
    # 这不是dBm或标定功率。imshow的-70..0只限制颜色显示，不截断原始IQ。
    dB=20*np.log10(np.maximum(mag,1e-12)/max(float(mag.max()),1e-12))
    im=a[1,0].imshow(dB,aspect='auto',origin='lower',extent=[ranges[0],ranges[-1],0,len(z)-1],cmap='turbo',vmin=-70,vmax=0)
    a[1,0].axvline(ranges[k],color='white',ls='--')
    a[1,0].set(title='跨帧距离FFT幅值',xlabel='Conditional range (m)',ylabel='Frame index')
    fig.colorbar(im,ax=a[1,0],label='relative dB')
    save(fig,'03_range_map.png')
    # 8. 相位提取：angle(iq)=atan2(Q,I)，单位rad，通常在[-π,π]。
    # 注意这里使用原始iq，而不是去均值后的i_ac+j*q_ac；两者的相位一般不同。
    # unwrap沿慢时间修正跨±π边界时的2π跳变；再减首点，让曲线从0开始。
    # 这不保证恢复真实位移：低幅值噪声及相邻帧相位变化超过π都可能导致错误。
    phase=np.unwrap(np.angle(iq)); phase-=phase[0]
    # 9. 线性去趋势：polyfit(...,1)拟合a*t+b，polyval求各时刻拟合值，
    # 再从连续相位中减去直线，降低缓慢线性漂移对低频谱的影响。
    # 它也可能移除真实运动的一部分，且不等同于抑制所有体动。
    detrended=phase-np.polyval(np.polyfit(t,phase,1),t)
    fig,a=setup_figure('相位诊断：解缠与去线性趋势',side,2)
    plot_lines(a[0,0],t,[np.angle(iq),phase],['wrapped','unwrapped'],'相位解缠（IQ过弱时不可靠）',xlabel,'rad')
    plot_lines(a[1,0],t,[detrended],['linear detrended'],'去趋势相位；短记录不推断呼吸/心率',xlabel,'rad')
    save(fig,'04_phase.png')
    # 频谱与IQ绘图解耦：一个尾包缺时间，不能否决前面完整的长记录。
    # IQ图继续保留全部数据；频谱独立截取最长连续有效时间段并重新解缠/去趋势。
    spectrum_slice,st,spectrum_timing=spectrum_segment(meta,args.fps)
    spectrum_detrended=np.array([],float)
    spectrum_resolution=None
    spectrum_status='skipped: '+spectrum_timing.get('reason','没有有效连续时间段')
    if spectrum_timing.get('fps') and len(st)>=4:
        siq=iq[spectrum_slice]
        sp=np.unwrap(np.angle(siq));sp-=sp[0]
        spectrum_detrended=sp-np.polyval(np.polyfit(st,sp,1),st)
        uniform_t=np.linspace(0,st[-1],len(st))
        fs=(len(st)-1)/st[-1]
        resampled=np.interp(uniform_t,st,spectrum_detrended)
        filtered=None
        reason=None
        if st[-1]<args.min_duration:
            reason=f'有效时长{st[-1]:.2f}s小于{args.min_duration:g}s分析门槛'
        elif not spectrum_timing['uniform']:
            reason='间隔变化超过2%，仅生成重采样后的探索性诊断谱'
        elif fs/2<=2:
            reason='Nyquist不覆盖2Hz，跳过0.1–2Hz带通'
        else:
            try:
                from scipy.signal import butter,sosfiltfilt
                filtered=sosfiltfilt(butter(4,[.1,2],fs=fs,btype='bandpass',output='sos'),resampled)
            except (ImportError,ValueError) as e:
                reason='带通不可用: '+str(e)
        frequencies=np.fft.rfftfreq(len(st),1/fs)
        spectrum_resolution=fs/len(st)
        signal=resampled if filtered is None else filtered
        magnitude=np.abs(np.fft.rfft(signal*np.hanning(len(signal))))
        # 侧栏使用这张频谱自身的时间来源，而不是全帧回退后的帧索引。
        spectrum_side=(side.split('距离为配置条件换算')[0]+'距离为配置条件换算\n\n'
            f'频谱使用原始帧{ spectrum_timing["start_frame"] }–{ spectrum_timing["end_frame"] }（0起）\n'
            f'有效频谱帧: {len(st)}，排除{len(iq)-len(st)}帧\n'
            f'实际区间重采样Fs: {fs:.4f} Hz\n有效时长: {st[-1]:.3f}s\n'
            f'频率网格: {spectrum_resolution:.5f}Hz\n未猜测缺失间隔，未拼接时间断点\n')
        if reason:
            spectrum_side+='\n未带通；原因详见report.json'
            title='相位诊断频谱（未带通，不估计呼吸/心率）'
        else:
            spectrum_side+='\n0.1–2Hz零相位带通\n候选频谱，不是生理测量确认'
            title='相位带通与频谱（有效连续时间段）'
        fig,a=setup_figure(title,spectrum_side,3)
        plot_lines(a[0,0],uniform_t,[signal],['detrended' if filtered is None else '0.1–2 Hz'],
            '重采样后去趋势相位（未带通）' if filtered is None else 'Butterworth零相位带通后的相位','Time (s)','rad')
        keep=frequencies<=min(2.5,fs/2)
        plot_lines(a[1,0],frequencies[keep],[magnitude[keep]],['Hann + FFT'],
            '相位频谱（频率网格不等于测量准确度）','Frequency (Hz)','Magnitude')
        plot_lines(a[2,0],frequencies[keep]*60,[magnitude[keep]],['same spectrum'],
            '同一频谱换算为每分钟周期数，不是检测结论','Frequency (cycles/min)','Magnitude')
        for ax,scale in [(a[1,0],1),(a[2,0],60)]:
            ax.axvspan(.1*scale,.5*scale,color='gray',alpha=.12)
            ax.axvspan(.8*scale,2*scale,color='orange',alpha=.12)
        save(fig,'05_static_phase_spectrum.png')
        spectrum_status=('diagnostic spectrum generated without bandpass: '+reason if reason
            else 'generated; valid continuous segment, uniformly resampled, bandpass 0.1–2 Hz')
        spectrum_timing.update(resampled_fps=float(fs),bandpass_applied=filtered is not None,
            diagnostic_reason=reason)
    elif spectrum_timing.get('fps'):
        spectrum_status='skipped: 有效连续段少于4帧，无法生成有意义的相位频谱'
    notes.append('频谱状态: '+spectrum_status)
    # 【结果保存】报告保存解析状态、选择条件、时间来源及风险说明。
    # all_frames_unique只检查数值行是否重复，不是CRC校验或有效性证明。
    report=dict(input=str(args.input.resolve()),input_type=input_type,parser=diag,
        data_shape=list(z.shape),selection=dict(range_bin=k,conditional_range_m=float(ranges[k]),method='manual' if args.range_bin is not None else 'strongest mean magnitude within range'),
        timing=timing,duration_s=float(t[-1]) if timing['fps'] else None,
        spectrum_timing=spectrum_timing,spectrum_resolution_hz=spectrum_resolution,
        real_min=float(z.real.min()),real_max=float(z.real.max()),imag_min=float(z.imag.min()),imag_max=float(z.imag.max()),
        all_frames_unique=bool(len(np.unique(z,axis=0))==len(z)),spectrum_status=spectrum_status,
        frame_metadata=meta,notes=notes,plots=imgs)
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    # NPZ保留全帧全距离仓和中间相位结果，可用np.load再次检查，避免只剩图片。
    np.savez_compressed(out/'parsed_1d.npz',range_fft=z,range_m=ranges,relative_time_or_frame=t,selected_iq=iq,unwrapped_phase=phase,detrended_phase=detrended,
        spectrum_frame_indices=np.arange(len(z))[spectrum_slice],spectrum_time_s=st,
        spectrum_detrended_phase=spectrum_detrended)
    # CSV每行对应一帧：帧索引、秒/索引、I、Q、模值、去均值I/Q、解缠相位、去趋势相位。
    np.savetxt(out/'selected_iq.csv',np.column_stack([np.arange(len(z)),t,iq.real,iq.imag,amp,i_ac,q_ac,phase,detrended]),delimiter=',',comments='',header='frame,time_s_or_index,I,Q,magnitude,I_ac,Q_ac,unwrap_rad,detrended_rad')
    # HTML只是汇总PNG的离线浏览页面，不执行任何额外算法。
    body=''.join('<h2>'+html.escape(p)+'</h2><img style="max-width:100%" src="'+p+'">' for p in imgs)
    (out/'index.html').write_text('<!doctype html><meta charset="utf-8"><title>1DFFT IQ分析</title><body style="font-family:sans-serif;max-width:1600px;margin:auto"><h1>1DFFT IQ分析</h1><p>'+html.escape(spectrum_status)+'</p>'+body+'</body>',encoding='utf-8')
    print(json.dumps({key:report[key] for key in ['data_shape','selection','timing','duration_s','spectrum_timing','spectrum_status']},ensure_ascii=False,indent=2))
    print('输出:',out.resolve())
    return report


# 【命令行参数】bins是导出的距离仓数，不是ADC采样点数或完整硬件FFT点数；
# range-bin为从0开始的索引；fps为慢时间帧率；min-duration以秒为单位。
def cli():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('input',type=Path)
    p.add_argument('--output',type=Path)
    p.add_argument('--bins',type=int,default=150)
    p.add_argument('--range-bin',type=int)
    p.add_argument('--bin-spacing',type=float,default=.050041)
    p.add_argument('--min-range',type=float,default=.2)
    p.add_argument('--max-range',type=float,default=7.5)
    p.add_argument('--fps',type=float,help='显式覆盖固件帧间隔；请勿直接填写原参考脚本的50Hz')
    p.add_argument('--min-duration',type=float,default=30)
    args=p.parse_args()
    if args.bins<1:p.error('bins必须大于0')
    try:analyze(args)
    except (ValueError,OSError) as e:p.exit(2,str(e)+'\n')


if __name__=='__main__':cli()
