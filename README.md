# STM32 Radar

中控 STM32F407ZG 与 SKY32B750 雷达的独立 Keil 工程，以及面向离线分析的数据采集工具。当前阶段提供可靠采集接口，不实现呼吸或心跳检测算法。

## 工程入口

| 内容 | 路径 |
| --- | --- |
| 中控工程 | `firmware/controller/Project/MDK-ARM(uV4)/project.uvprojx` |
| 雷达工程 | `firmware/radar/proj/V1.1.2_chirpmean_44+86/Jupiter.uvprojx` |
| 架构和修改入口 | [docs/architecture.md](docs/architecture.md) |
| 接线、命令、采集步骤 | [docs/capture_guide.md](docs/capture_guide.md) |
| 新旧数据协议 | [docs/protocol.md](docs/protocol.md) |
| 构建与烧录 | [docs/build.md](docs/build.md) |
| 验证记录及待上板清单 | [docs/validation.md](docs/validation.md) |
| 保留的原有问题 | [docs/known_issues.md](docs/known_issues.md) |

## 编译与离线检查

保留 ARM Compiler 5.06 update 7，不迁移 HAL、RTOS 或编译器。

```powershell
powershell -ExecutionPolicy Bypass -File tools/build.ps1 -Rebuild
python tools/check_projects.py
python -m unittest discover -s tests -v
```

## 首次采集

先阅读接线说明并烧录两套新固件。中控默认释放雷达控制权；旧中控固件没有该功能。

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r process_data/requirements_capture.txt
.\.venv\Scripts\python.exe process_data/capture_serial.py --port COM5 --link direct --mode IQ --duration 180 --output captures/iq_first
```

`COM5` 是示例，必须替换为实际端口。现有 `process_data` 数据和脚本保留；采集数据、构建产物、本地虚拟环境不提交到 Git。原始会员资料目录未修改，也不作为新工程的构建依赖。
