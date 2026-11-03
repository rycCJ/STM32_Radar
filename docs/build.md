# 编译、烧录与回退

## 编译

开发环境保持Keil MDK与ARM Compiler 5.06 update 7 (build 960)。当前机器Keil入口为 `D:\APPS\Keil539\UV4\UV4.exe`；换机器时通过脚本 `-Keil` 参数指定位置，不修改工程源文件路径。

```powershell
powershell -ExecutionPolicy Bypass -File tools/build.ps1 -Rebuild
# 自定义安装位置
powershell -ExecutionPolicy Bypass -File tools/build.ps1 -Keil 'C:\Keil_v5\UV4\UV4.exe'
```

脚本只构建，不连接或烧录调试器。日志位于build/controller.log与build/radar.log。Keil退出码1可表示有警告，因此脚本同时要求日志出现0 Error(s)。

| 固件 | Target | 生成HEX |
| --- | --- | --- |
| 中控 | Flash | `firmware/controller/Project/Bin/DD20250310.hex` |
| 雷达 | Jupiter | `firmware/radar/proj/V1.1.2_chirpmean_44+86/Objects/Jupiter.hex` |

厂商库、启动文件、内存布局和FLM配置保留。工程引用均使用目标目录内的相对路径；原中控工程5条不存在的HARDWARE/SYSTEM include目录已删除，未凭空补建驱动。

## 人工烧录

分别打开对应工程，按已有板卡流程连接J-Link和相应芯片的SWD端口，核实对应FLM后再下载。雷达工程配置为通用Cortex-M7，但其实际启动文件是SKY32B750；不要按STM32芯片选择烧录算法。

本次没有访问J-Link或自动烧录。更换电脑若缺少厂商FLM，按原资料配置，不用通用STM32 FLM替代。生产版中控默认DIRECT，因此两套固件都更新后，雷达不再由中控上电自动START，而由采集工具明确启动。

## 分阶段回退

Git历史中保留独立迁移基线、只拆分职责的重构提交和采集扩展提交。可在新worktree检出早期提交，避免覆盖正在工作的目标目录。早期中控默认主动控制雷达；回退后不能继续假定PA2为高阻。

源文件原有编码保持不变；新增文件采用ASCII或UTF-8。编辑原中文注释时先确认编码，不对全部厂商文件执行批量转换。

`.gitattributes`禁止Git对固件进行自动换行转换，以保证厂商文件原始字节和迁移SHA-256清单在检出后仍一致。历史中包含一次Git存储格式修正：将首次提交时全局autocrlf归一化的库文件恢复为供应资料的原始字节，库逻辑未改动。
