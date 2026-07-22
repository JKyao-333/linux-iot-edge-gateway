# 公开复现证据台账

## 1. 台账范围

本台账记录当前公开仓库能够复现、脱敏归档和脚本化验证的证据。公开仓库只记录可公开、已脱敏、可复现的信息；真实设备序列号、未脱敏日志、原始示波器截图、逻辑分析仪工程文件和本机私有路径不提交到仓库。

当前公开复现路径包括：

- `socat` PTY 虚拟串口；
- Python mock serial sender；
- Python mock Modbus RTU slave；
- Linux `vcan0`；
- Raspberry Pi 4 Model B 64-bit Linux；
- FIT IoT-LAB M3 / OpenM3-compatible STM32 node；
- Docker Compose 本地复现环境；
- CTest、smoke test、protocol integration test、observability test 和 validation workflow。

本地运行可使用以下命令生成脱敏证据摘要：

```bash
./scripts/collect_evidence_summary.sh
```

输出位于 `artifacts/evidence_summary.md`，默认不提交。

## 2. 公开复现平台

| 字段 | 当前记录 | 证据来源 | 说明 |
| --- | --- | --- | --- |
| 下位机公开复现平台 | FIT IoT-LAB M3 / OpenM3-compatible STM32 node | [references.md](references.md) 与公开平台资料 | 用于公开复现 STM32-compatible UART 数据源 |
| MCU 型号 | STM32F103REY，ARM Cortex-M3 | FIT IoT-LAB M3 公开硬件资料 | 作为公开复现平台的 MCU 型号 |
| PCB 版本 | 当前公开复现路径不声明具体 PCB revision | 当前仓库未保存 PCB revision 证据 | PCB revision 不作为当前公开复现结论的必要字段 |
| 固件提交号 | 当前仓库未提交 OpenM3 固件源码或固件 commit | 当前仓库 | 当前公开复现主要验证网关侧接入、解析、发布和异常恢复，不声明某一特定 OpenM3 固件 commit |
| USB 转串口芯片与接线 | 当前公开复现路径不声明具体 USB-UART 芯片型号 | 当前仓库未保存转接板型号与接线照片 | 脚本化验证使用 `socat` PTY；真实 USB-UART 接线不作为当前公开复现结论的必要字段 |

## 3. Linux 网关运行环境

| 字段 | 当前记录 | 证据来源 | 说明 |
| --- | --- | --- | --- |
| ARM64 公开复现平台 | Raspberry Pi 4 Model B，64-bit Linux | [reproducible_validation.md](reproducible_validation.md) / [arm64_stability_test.md](arm64_stability_test.md) | 用于公开 ARM64 网关复现 |
| Linux 内核版本 | 由 `scripts/collect_env.sh` 或 `scripts/collect_evidence_summary.sh` 在每次运行时采集 | `artifacts/env_report.md` 或 `artifacts/evidence_summary.md`，默认不提交 | 不同复现环境的 kernel version 可能不同，公开文档不固定写死 |
| TTY 节点 | 脚本化验证使用 `/tmp/tty_gateway`、`/tmp/tty_stm32` 和临时 PTY；真实硬件使用实际系统枚举路径 | `scripts/run_smoke_test.sh`、`scripts/run_protocol_integration_test.sh`、`scripts/run_stability_test.sh` | TTY 节点由运行环境决定，不在公开文档中声明固定设备路径 |
| 测试日期 | 由各测试脚本在 `artifacts/` 报告中生成 | `fault_injection_summary.md`、`stability_summary.md`、`observability_test_summary.md`、`validation_workflow_check.md` | 运行产物默认不提交；公开仓库保留可复现脚本和报告格式 |

## 4. 协议与数据统计

| 字段 | 当前记录 | 证据来源 | 说明 |
| --- | --- | --- | --- |
| 发送周期 | 由脚本参数控制，例如 `--rate-hz`、`poll_interval_ms` 和 mock sender 参数 | `scripts/run_stability_test.sh`、`scripts/run_protocol_integration_test.sh`、`config/gateway.yaml` | 公开仓库不声明固定发送周期；具体运行以测试参数为准 |
| 累计帧数 | 由运行脚本统计或通过 MQTT 接收日志计算 | `artifacts/stability_summary.md`、`artifacts/mqtt_received.log`、observability metrics | 不写死累计帧数；每次运行由脚本生成 |
| CRC 错误计数 | 由 `FrameParser`、`UartDevice` 和 `RuntimeMetrics` 统计 | `iot_gateway_crc_errors_total`、fault injection、`uart_device_error_stats_test` | 计数属于运行期观测，进程重启后重新计数 |
| 断线次数 | 由稳定性测试、故障注入测试或运行日志归档 | `scripts/run_fault_injection_test.sh`、`scripts/run_stability_test.sh`、`gateway.log` | 公开仓库提供统计机制，不声明固定断线次数 |
| Modbus 错误计数 | `gateway_modbus_error_total` | Prometheus metrics、Modbus timeout / CRC / exception tests | 覆盖当前 FC03/FC04 单从站轮询参考模型 |
| CAN 错误计数 | `gateway_can_error_total` | Prometheus metrics、SocketCAN parser tests | 覆盖当前标准 CAN frame / DLC=8 参考模型 |

## 5. 原始记录与公开归档策略

### 原始串口抓包

- 当前公开仓库不提交真实原始抓包；
- 可提交脱敏后的协议测试帧；
- 当前公开测试帧位于 `data/test_frames/`。

### 示波器与逻辑分析仪记录

- 当前公开仓库不保存示波器截图或逻辑分析仪工程文件；
- 如需公开，只能归档脱敏摘要，不包含设备序列号、个人路径或现场信息。

### 本地运行产物

- `artifacts/` 默认忽略运行产物；
- 该目录用于本地生成 env report、stability summary、fault injection summary、health check、observability summary 和 evidence summary；
- 脱敏摘要经人工复核后，可转写到对应公开工程文档。

## 6. 当前公开复现结论

当前公开仓库能够支撑以下结论：

1. 网关支持 UART、自定义协议、Modbus RTU 与 SocketCAN/`vcan` 三类输入；
2. 三类输入可统一转换为 `SensorData` 并进入 MQTT/TCP 发布链路；
3. 项目提供 CTest、smoke test、故障注入、稳定性测试、串口 replay、protocol integration、health check、observability test 和 validation workflow；
4. OpenM3-compatible STM32 node 与 Raspberry Pi 4 可作为公开复现平台，用于验证核心链路；
5. 未声明工业现场电气层、EMC、量产可靠性、固定吞吐、P99 或 7x24 可用性。

## 7. 当前公开复现路径不声明的内容

- 不声明固定 PCB revision；
- 不声明固定 USB-UART 芯片型号；
- 不声明固定 OpenM3 固件 commit；
- 不声明固定 Linux kernel version；
- 不声明固定 TTY 路径；
- 不声明固定累计帧数、固定 CRC 错误数或固定断线次数；
- 不公开原始串口抓包、示波器截图或逻辑分析仪工程文件；
- 不使用上述未公开字段支撑当前公开复现结论。
