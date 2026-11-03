# 接线与采集

## 先确认链路控制权

此工程面向雷达、STM32、4G同板的圆形模组。雷达TX(P1.2)连接中控PA3，雷达RX(P1.3)连接中控PA2，中控PC3控制雷达RSTN。

新中控固件上电默认DIRECT，PA2和PC3为输入无上下拉，不自动启动雷达。此行为必须上板确认，尤其是板上复位线外部偏置及PA2实际高阻状态。不能用旧中控固件替代这一条件。

- **直连**：USB串口RX接雷达TX、TX接雷达RX、GND共地。中控必须处于DIRECT；从BRIDGE返回时，要先通过中控串口1发送 `AT+LINK=DIRECT` 并收到回复，再接电脑到雷达的TX线。
- **转发**：先断开USB串口到雷达RX的TX线，避免两个输出端并接；将电脑USB串口接中控串口1（PA9/TX、PA10/RX、GND）。工具发送LINK=BRIDGE取得控制权，再经中控操作雷达。
- 不把USB串口电源脚作为默认供电方式；沿用模组既有供电。核对板上丝印，不凭排针位置猜TX/RX或电压。

双链路表示两种可切换接法，不表示两个主机可以同时向雷达发命令。采集工具的 `--link` 不会替你隔离硬件发送端。

## 建立环境

在仓库根目录运行：

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r process_data/requirements_capture.txt
.\.venv\Scripts\python.exe -m serial.tools.list_ports -v
```

本次实施已在本地 `.venv` 安装pyserial 3.5。已有分析脚本的numpy/scipy等依赖仍由 `requirements_adc.txt` 管理，不与轻量采集器绑定。

## 选择模式并记录

```powershell
# 电脑直连雷达；COM5改为实际雷达端口
.\.venv\Scripts\python.exe process_data/capture_serial.py --port COM5 --link direct --mode IQ --duration 180 --output captures/iq_001

# 电脑连接中控串口1；先断开直连雷达的电脑TX线
.\.venv\Scripts\python.exe process_data/capture_serial.py --port COM6 --link bridge --mode ADC --duration 180 --output captures/adc_bridge_001
```

`--mode` 可选ADC/IQ/POINT/LEGACY。默认921600波特率、180秒、LEGACY；独立ADC/IQ/POINT使用新帧。每次记录必须使用不存在的输出目录，工具拒绝覆盖已有会话。

工具流程：建立链路→AT握手（工作中的首次接收可能触发复位）→等待AT+OK→MODE应答→STARTED应答→开始记录。默认连续5秒没有数据会停止；没有有效帧、模式不符或握手失败均记录为failed。

正常结束及异常退出都会尝试使雷达回到配置状态。BRIDGE还会查询本次新增的队列溢出/UART错误并尝试恢复DIRECT；若停止或释放失败，session会明确记录失败，接线切换前仍需人工确认DIRECT状态。

## 文件与时间

- `raw.bin`：记录阶段实际收到的字节，保留错误或截断内容便于复盘。
- `frames.jsonl`：有效帧元信息以及识别该帧的电脑接收时间。
- `chunks.jsonl`：每次串口读取的原始文件偏移、字节数和电脑接收时间。
- `session.json`：参数、握手记录、最终状态、CRC/丢帧/重启统计、设备帧间隔、失败原因及桥接计数。

电脑接收时间是读取完成时的Unix纳秒值，可能受操作系统缓冲影响，不能代替设备采集时间。采集在指定时长结束时可能恰好截断最后一帧，trailing_bytes单独记录，不把残帧输出为有效帧。captured_with_errors表示录到了数据但存在协议、序号或重启问题；工具退出码为2。

```powershell
# 流式解码，输出目录应已存在，输出文件应尚不存在
python process_data/decode_capture.py captures/iq_001/raw.bin --format new --values --output captures/iq_001/decoded.jsonl
python process_data/decode_capture.py captures/legacy_001/raw.bin --format legacy --values --output captures/legacy_001/decoded.jsonl
# 旧ADC文件用 --format adc-legacy
```

在Python中可使用 `Decoder.feed(chunk)` 增量读取，`Frame.values()` 返回ADC样本、IQ两平面或点云原包。未加入带通、呼吸频率或心率估计。
