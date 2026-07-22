# ARM64 Linux 开发板稳定性测试

## 1. 文档范围

本文件记录 Raspberry Pi 4 Model B 64-bit Linux 上的 ARM64 公开复现方法，覆盖串口接收、MQTT/TCP 上报、离线缓存补传、异常恢复和持续运行趋势观察。

公开仓库使用[公开复现证据台账](reproducible_evidence_ledger.md)记录可公开、已脱敏和可复现的证据字段。内核版本、运行时长、采样值和测试日期由每次运行的脚本报告生成，不在公开文档中固定写死。

## 2. 验证环境

| 项目 | 本项目记录 |
| --- | --- |
| 处理器架构 | AArch64 / ARM64 |
| 开发板型号 | Raspberry Pi 4 Model B |
| 软件基线 | Ubuntu 22.04 arm64 运行时依赖与 systemd 部署方式 |
| 串口输入 | STM32 物理串口；Python + socat 可作为无硬件回归输入 |
| MQTT | Mosquitto，QoS 1，断线缓存与恢复补传 |
| TCP | POSIX Socket 客户端，JSON Lines 消息边界 |
| 缓存 | SQLite WAL，`synchronous=FULL` |
| 日志 | 分级文件日志与 systemd journal |

Raspberry Pi 4 Model B + 64-bit Linux 作为公开 ARM64 复现平台，用于验证 `edge_gateway` 的 ARM64 启动、systemd 管理、串口接入、MQTT/TCP 上报、SQLite 缓存和异常恢复。

## 3. 测试负载

稳定性验证覆盖以下持续负载和故障注入：

- STM32 周期上报多传感器协议帧。
- MQTT 与 TCP 双通道同时发送同一条 JSON。
- MQTT Broker 停止后将消息写入 SQLite，恢复后顺序补传。
- TCP 服务端停止与恢复，验证单通道失败不阻塞 MQTT。
- 串口断开、设备节点消失和恢复，验证自动重连。
- SIGTERM 停止服务，检查线程、Socket、数据库和日志正常收尾。

仓库无硬件 smoke test 默认以 1 帧/秒运行。公开仓库不把该默认参数写成固定数据频率或稳定性时长，实际值由测试命令与本地摘要记录。

## 4. 观测项目

| 指标 | 观测方法 | 当前公开记录 |
| --- | --- | --- |
| 运行时长 | `systemctl show`、单调时钟或测试脚本 | 由 `stability_summary.md` 按次记录 |
| 数据频率 | 下位机配置、脚本参数与序列号增量 | 由 `--rate-hz` 或真实数据源配置按次记录 |
| CPU | `pidstat`、`top` 或测试脚本采样 | 由 `stability_samples.csv` 按次记录，不声明固定值 |
| 内存 | RSS/PSS、cgroup memory 或测试脚本采样 | 由 `stability_samples.csv` 按次记录，不声明固定值 |
| 日志 | 文件日志与 `journalctl` | 启动、断线、缓存、补传和恢复事件可追踪 |
| 消息完整性 | 设备 ID、sequence、Broker/TCP 接收记录 | 连续接收和恢复补传通过 |
| 缓存深度 | SQLite 队列计数 | 离线增长、恢复下降至零的流程通过 |

## 5. 测试步骤

1. 在 ARM64 开发板安装运行依赖并部署 AArch64 构建产物。
2. 记录 `uname -a`、`cat /etc/os-release`、`lscpu`、内存容量和部署包 SHA-256。
3. 启动 Mosquitto、TCP 服务端和 systemd 网关服务。
4. 启动 STM32 周期上报，确认 MQTT Topic 与 TCP JSON Lines 均持续增长。
5. 周期采集 CPU、RSS、文件描述符、线程数、缓存深度和日志错误计数。
6. 停止并恢复 Broker，确认离线数据入队、连接恢复和队列清空。
7. 停止并恢复 TCP 服务端，确认 MQTT 通道继续工作。
8. 断开并恢复串口，确认服务进程不退出并恢复接收。
9. 使用 SIGTERM 停止服务，检查退出日志和资源释放。

## 6. 测试结果

| 场景 | 结果 |
| --- | --- |
| ARM64 原生启动与配置加载 | 通过 |
| STM32 串口连续接收与协议解析 | 通过 |
| MQTT QoS 1 持续发布 | 通过 |
| TCP JSON Lines 持续发布 | 通过 |
| Broker 离线缓存与恢复补传 | 通过 |
| TCP 故障与通道隔离 | 通过 |
| 串口断开与自动重连 | 通过 |
| SIGTERM 优雅退出 | 通过 |
| Raspberry Pi 4 公开 ARM64 复现链路 | 通过 |
| 稳定运行期间资源行为 | 未观察到影响链路的异常增长或进程退出 |

## 7. 异常恢复记录

- Broker 不可用时 MQTT 发布失败，消息进入 SQLite 队列；Broker 恢复后按顺序重放。
- TCP 服务端不可用时只记录 TCP 通道失败，MQTT 发布仍继续。
- 串口设备消失时读取返回错误，网关关闭旧描述符并重试；设备恢复后重新打开并重置解析器。
- 重复投递属于 QoS 1 与本地确认窗口下允许的行为，服务端应按设备 ID 和 sequence 去重。

## 8. 结论

Raspberry Pi 4 Model B + 64-bit Linux 上的公开复现覆盖核心数据链路和故障恢复路径。该结果不用于推导固定吞吐量、P99 时延、7x24 可用性或工业现场可靠性。仓库中的交叉编译、QEMU 启动、CI、Python 模拟器和 `socat` 测试继续提供无硬件软件回归路径。

## 9. 公开证据记录方式

- 使用 `scripts/collect_env.sh` 或 `scripts/collect_evidence_summary.sh` 记录每次运行的内核、工具与脱敏设备摘要；
- 使用 `scripts/run_stability_test.sh` 记录测试参数、资源采样、消息计数、缓存深度和退出状态；
- 本地结果默认写入 `artifacts/` 并由 Git 忽略；
- 公开文档不固定声明系统镜像摘要、运行时长、资源数值、消息总数或故障次数。

字段来源和公开边界见[公开复现证据台账](reproducible_evidence_ledger.md)，平台与工具来源见 [references.md](references.md)。
