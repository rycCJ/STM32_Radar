# 架构、数据流与修改入口

## 总体结构

`firmware/controller` 与 `firmware/radar` 各自保留厂商目录层级与独立 Keil 工程。没有为了统一外观而移动底层库、启动文件、硬件地址或标定表。

```text
直连：电脑串口工具 <--> 雷达命令层 -> 模式调度 -> ADC/原处理流程 -> 数据封装 -> 电脑
转发：电脑 <--> STM32串口1 <--> 字节流队列 <--> STM32串口2 <--> 雷达
                                     |
                                     +-> 点云计数观察 -> STM32串口3 -> 原通信模块
```

通信模块源码不纳入本轮重构。新中控保留串口3初始化、接收缓冲接口和每100ms的6字节点数上报。ADC/IQ独立模式没有新点云，计数保持上次值；切换链路时归零。

## 中控

- `User/main.c`：只调用初始化与轮询。
- `User/controller_app.c`：生产入口初始化、轮询、串口3定时上报；原处理流程保留为 `ControllerLegacy_*`，不在生产入口运行。完整原行为可回到独立迁移/重构提交验证。
- `User/controller_link.c`：控制权、PA2高阻、PC3复位控制、主机命令、两个中断接收队列、非阻塞逐字节转发。雷达队列16384字节，主机命令队列256字节，队列满计数而不覆盖未读数据。
- `User/controller_points.c`：只观察转发数据，识别旧组合包与新POINT帧并更新点数，不修改转发内容。新POINT帧通过CRC后才更新计数。
- `User/bsp_stm32f4xx`：沿用标准外设库；生产启动不调用旧DMA空闲分帧与自动复位。原DMA接收代码仍保留，不与新RXNE队列同时启用。

日志在BRIDGE状态被抑制，链路命令应答、雷达自身回复仍由协议通道传输。异常处理器输出代表故障，不能当作有效二进制采集。

## 雷达

- `User/main.c`：系统初始化和主循环入口。
- `User/App_Runtime.c`：保留原系统初始化、工作状态及功能调度。
- `App/Radar_Calibration.c`：原标定/窗口/导向矢量常量、缓存指针和BB初始化，数据内容保持原样。
- `App/Radar_Acquisition.c`：原Func01单路ADC导出路径，保留采集流程及200ms等待。
- `App/App_Func.c`：原Func02完整处理链及导入输出，保留运算与寄存器顺序。
- `User/Usart_Protocol.c`、`ParamConfig.c`：原命令与持久参数；新增MODE命令不写Flash。
- `User/Radar_PointPacket.c`：原点云编码器，线格式未改变。
- `App/Capture_Output.c`：新增模式选择、帧开始RTC时间、序号、CRC以及新旧格式发送。
- `App/Inc/Radar_Internal.h`：仅为拆分原文件所需的内部共享声明，不作为跨芯片公共接口。

ADC模式记录缓存起始的一路，不将槽0声明为某根物理天线；IQ仍取原 `j=2,m=6`，不跨Chirp平均。三个新增独立模式只改变输出，不优化或裁剪原处理链。

## 后续算法入口

优先从 `process_data/capture_protocol.py` 的 `Frame.values()` 读取ADC数组、IQ两个平面或原点云包。采集模块负责边界、字节序、时间来源和丢帧统计；后续呼吸心跳算法单独建模块，不回填到串口驱动或雷达入口。
