# Linux C++ 多协议物联网边缘网关

[![CI](https://github.com/JKyao-333/linux-iot-edge-gateway/actions/workflows/ci.yml/badge.svg)](https://github.com/JKyao-333/linux-iot-edge-gateway/actions/workflows/ci.yml)

基于 Linux C++17 实现的多协议物联网边缘网关，用于连接 STM32 多传感器终端、MQTT Broker 和 TCP Server。
网关接收物理串口或 Python 虚拟串口数据，完成自定义协议解析、CRC16 校验、数据清洗、JSON 封装、MQTT/TCP 双通道上报、MQTT 离线缓存、恢复补传、串口自动重连和日志记录。

## 验证范围

- 已完成 STM32 下位机到 Linux 网关的物理串口数据接入验证，包括连续帧、半包、粘包、CRC 错误和串口断开恢复场景。
- 已在 ARM64 Linux 开发板上完成串口采集、MQTT/TCP 上报、离线缓存补传和异常恢复的稳定性测试。
- 已在 FIT IoT-LAB OpenM3 兼容 STM32 节点和 Raspberry Pi 4 Model B 64-bit Linux 环境中完成核心链路复现验证。
- 仓库继续保留 Python 数据模拟器、`socat` 虚拟串口、CTest/CI 和 QEMU AArch64 启动校验，作为不依赖实验室硬件的可重复验证路径。
- 公开仓库使用[公开复现证据台账](docs/reproducible_evidence_ledger.md)统一记录可公开、已脱敏和可复现的验证字段及边界。

## 核心能力

- 可选 HTTP Health / Ready Endpoint
- Prometheus 文本格式运行指标
- Docker Compose 本地复现环境
- termios 串口配置与原始字节接收
- 多串口独立工作线程与多设备并发接入
- 256 字节固定容量环形缓冲区
- 自定义二进制协议解析
- CRC16-Modbus 校验
- Modbus RTU Master，支持功能码 03/04
- Linux SocketCAN 标准帧接入与 vcan 复现
- UART、Modbus RTU、SocketCAN 统一设备输入接口
- 设备在线状态、心跳时间和协议错误指标
- 半包、粘包和异常帧处理
- 多传感器 Payload 解析
- 传感器数据范围检查
- JSON 数据封装
- MQTT Topic 动态生成
- 基于 POSIX Socket 的原生 TCP 客户端
- 使用 JSON Lines 解决 TCP 消息边界问题
- MQTT 与 TCP 双通道数据上报
- Broker 离线消息落盘
- Broker 恢复后缓存补传
- 串口断开检测与自动重连
- SIGINT/SIGTERM 优雅退出
- systemd 后台服务与故障拉起
- DEBUG、INFO、WARN、ERROR 日志
- Python、Shell 和 CTest 自动化测试
- AArch64 交叉编译、QEMU 启动校验和部署包生成
- 语义化版本、SHA-256 校验与自动化 GitHub Release

## 数据链路

UART / Modbus RTU / SocketCAN
-> DeviceInterface 输入适配器
-> DeviceManager
-> DeviceData
-> SensorData
-> 数据清洗
-> JSON
-> PublisherGroup
-> MQTT ReliablePublisher / TCP Publisher
-> MQTT Broker / TCP Server

发布失败时：

MQTT 发布失败
-> MessageCache 统一缓存接口
-> SqliteCache 持久化队列
-> 周期性重试
-> Broker 恢复
-> 缓存补传

## 协议格式

| 字段 | 长度 |
|---|---:|
| 帧头 `AA 55` | 2 B |
| Payload 长度 | 1 B |
| 命令字 | 1 B |
| 设备 ID | 1 B |
| Payload | N B |
| CRC16 | 2 B |

示例帧：

`AA 55 0B 01 10 00 FD 02 60 01 2C 0E 74 00 00 01 4D 58`

示例数据：

- 设备 ID：16
- 温度：25.3 摄氏度
- 湿度：60.8%
- 气体浓度：300 ppm
- 电池电压：3700 mV
- 状态：正常
- 序号：1

生成的 JSON：

`{"device_id":16,"cmd":1,"temperature_c":25.3,"humidity_percent":60.8,"gas_ppm":300,"battery_mv":3700,"status":0,"sequence":1,"valid":true}`

MQTT Topic：

`sensor/16/data`

## 目录结构

- `src/protocol/`：协议帧、CRC16 和流式解析器
- `src/device/`：统一设备接口、输入适配器和设备状态管理
- `src/modbus/`：Modbus RTU 帧组包、响应解析和寄存器映射
- `src/can/`：SocketCAN 接口与 CAN Payload 解析
- `src/app/`：SensorData、数据清洗和 JSON
- `src/mqtt/`：MQTT 客户端和可靠发布器
- `src/tcp/`：原生 TCP Socket 客户端
- `src/publisher/`：统一 Publisher 接口、TCP 适配器和多通道发布组
- `src/cache/`：SQLite 默认缓存、文件兼容缓存和统一缓存接口
- `src/log/`：分级日志
- `deploy/systemd/`：systemd 服务单元
- `scripts/`：模拟器和自动化测试脚本
- `tests/`：C++ 测试
- `docs/`：架构、协议、测试和调试文档
- `config/`：配置文件目录

## 环境依赖

推荐环境：

- Ubuntu 22.04 / WSL2
- GCC 11 或更高版本
- CMake 3.16 或更高版本
- Python 3
- pyserial
- socat
- Mosquitto
- mosquitto-clients
- libmosquitto-dev
- pkg-config
- yaml-cpp
- SQLite3
- Linux SocketCAN；运行 vcan 测试时需要 `iproute2`，手工收发可选 `can-utils`
Ubuntu 安装命令：

`sudo apt update`

`sudo apt install -y build-essential cmake git python3 python3-pip socat mosquitto mosquitto-clients libmosquitto-dev openssl pkg-config libyaml-cpp-dev libsqlite3-dev sqlite3`
安装 pyserial：

`python3 -m pip install pyserial`
## YAML 配置

网关运行参数统一保存在 `config/gateway.yaml` 中。

主要配置项：

| 配置项 | 说明 |
|---|---|
| `serial.devices` | 串口设备路径列表，每个设备启动一个独立工作线程 |
| `serial.baud_rate` | 串口波特率 |
| `serial.reconnect_interval_seconds` | 串口断线重连间隔 |
| `modbus.*` | Modbus RTU 端口、从站、功能码、寄存器范围、轮询与超时 |
| `can.enabled` | 是否启用 SocketCAN 输入 |
| `can.interface` | SocketCAN 网络接口，例如 `can0` 或 `vcan0` |
| `can.heartbeat_timeout_seconds` | CAN 设备数据心跳超时 |
| `mqtt.host` | MQTT Broker 地址 |
| `mqtt.port` | MQTT Broker 端口 |
| `mqtt.topic_prefix` | MQTT Topic 前缀 |
| `mqtt.cache_retry_interval_seconds` | 离线缓存补传周期 |
| `mqtt.username` | MQTT 用户名，留空时不启用认证 |
| `mqtt.password` | MQTT 密码，配置后必须同时提供用户名 |
| `mqtt.tls.enabled` | 是否启用 MQTT TLS |
| `mqtt.tls.ca_file` | 用于验证 Broker 证书的 CA 文件 |
| `mqtt.tls.certificate_file` | 可选的客户端证书，用于双向 TLS |
| `mqtt.tls.private_key_file` | 可选的客户端私钥，必须与客户端证书同时配置 |
| `mqtt.tls.insecure` | 是否跳过 Broker 主机名校验，仅限本地调试 |
| `tcp.enabled` | 是否启用 TCP 上报 |
| `tcp.host` | TCP Server 地址 |
| `tcp.port` | TCP Server 端口 |
| `http.enabled` | 是否启用 HTTP 可观测性服务，默认关闭 |
| `http.host` | HTTP 监听地址，默认 `127.0.0.1` |
| `http.port` | HTTP 监听端口，默认 `8080` |
| `cache.type` | 缓存后端，支持 `sqlite` 或 `file` |
| `cache.path` | MQTT 离线缓存数据库或文件路径 |
| `log.path` | 网关日志文件路径 |
| `log.level` | 最低日志级别 |

程序启动格式：

`edge_gateway [配置文件] [串口设备覆盖值]`

使用默认配置启动：

`~/linux-iot-edge-gateway-build/edge_gateway config/gateway.yaml`

临时覆盖串口设备：

`~/linux-iot-edge-gateway-build/edge_gateway config/gateway.yaml /tmp/tty_gateway`

配置文件可在 `serial.devices` 中声明多个串口。命令行提供串口设备覆盖值时，仅启动该指定设备，便于单串口调试和 systemd 部署。

输入源可以按部署需求组合：

- UART-only：`serial.devices` 非空，关闭 Modbus 与 CAN；
- Modbus-only：`serial.devices: []`，启用 `modbus.enabled`；
- CAN-only：`serial.devices: []`，启用 `can.enabled`；
- mixed：同时启用两个或三个输入源。

配置必须至少启用一个输入源。`serial.devices` 为空且 Modbus、CAN 均关闭时，程序以 `at least one input source must be enabled` 拒绝启动。各输入源一旦启用，仍会分别检查设备路径、波特率、轮询参数、CAN 接口和日志级别等字段。
## 编译

推荐将构建目录放在 WSL Linux 文件系统中，避免 Windows 挂载目录的时间戳和链接问题。

配置：

`cmake -S . -B ~/linux-iot-edge-gateway-build`

编译：

`cmake --build ~/linux-iot-edge-gateway-build --parallel`

生成的网关程序：

`~/linux-iot-edge-gateway-build/edge_gateway`

### ARM64 交叉编译

安装交叉编译与模拟运行工具：

`sudo apt install -y g++-aarch64-linux-gnu qemu-user file pkg-config`

准备 ARM64 依赖 sysroot。脚本通过 `AARCH64_TOOLS_ROOT` 指定工具目录；未设置时默认使用 `~/Tools/linux-iot-edge-gateway`：

`./scripts/setup_aarch64_sysroot.sh`

WSL 用户如需使用 Windows D 盘，可显式设置：

`export AARCH64_TOOLS_ROOT=/mnt/d/Tools/linux-iot-edge-gateway`

构建、检查 ELF 架构、使用 QEMU 验证程序能够启动，并生成部署包：

`./scripts/build_aarch64.sh`

默认输出：

- 构建目录：`~/linux-iot-edge-gateway-build-aarch64`
- ARM64 ELF：`~/linux-iot-edge-gateway-build-aarch64/edge_gateway`
- 依赖 sysroot：`${AARCH64_TOOLS_ROOT:-$HOME/Tools/linux-iot-edge-gateway}/aarch64-sysroot`
- 部署包：`${AARCH64_TOOLS_ROOT:-$HOME/Tools/linux-iot-edge-gateway}/artifacts/linux-iot-edge-gateway-<version>-aarch64.tar.gz`
- 校验文件：`${AARCH64_TOOLS_ROOT:-$HOME/Tools/linux-iot-edge-gateway}/artifacts/linux-iot-edge-gateway-<version>-aarch64.tar.gz.sha256`

CI 会在独立的 Ubuntu 22.04 环境中重复执行原生测试和 ARM64 交叉构建，并上传 ARM64 部署包。

## 一键测试

启动 Mosquitto：

`sudo service mosquitto start`

运行全部 smoke test：

`./scripts/run_smoke_test.sh`

`run_smoke_test.sh` 会完成：

1. CMake 配置
2. C++ 工程编译
3. 当前构建中注册的全部 CTest 测试
4. YAML 正常配置与异常配置测试
5. SIGTERM 优雅退出验证
6. 虚拟串口创建
7. Python 半包发送
8. 串口协议解析与 JSON 生成
9. MQTT 发布验证
10. 双虚拟串口并发接入与不同设备 Topic 验证
11. SQLite 缓存迁移验证
12. 原生 TCP 发布验证
13. MQTT 用户认证、CA 校验和 TLS 加密发布验证

成功时最终输出：

`[PASS] all smoke tests passed`

### 专项工程验证

- `./scripts/run_fault_injection_test.sh`：验证异常帧、网络离线、缓存补传、串口重连和 SIGTERM 退出路径。
- `./scripts/run_stability_test.sh --duration-seconds 300 --rate-hz 1 --collect-env`：执行持续运行观察并归档采样与摘要。
- `./scripts/run_observability_test.sh`：验证 `/health`、`/ready`、`/metrics` 和 404 路径。
- `./scripts/run_protocol_integration_test.sh`：通过 PTY Modbus 从站和 `vcan0` 验证 UART、Modbus RTU、SocketCAN 三条输入链路最终发布到 MQTT。
- `./scripts/collect_env.sh --include-git --include-packages --output artifacts/env_report.md`：生成脱敏环境报告。

Docker Compose 本地复现：

```bash
mkdir -p artifacts/docker
docker compose up --build
```

该环境用于复现 MQTT、TCP、HTTP 与 metrics 链路，不构成量产性能结论。
- `python3 scripts/serial_replay.py --input data/test_frames/valid_frames.hex --serial /tmp/tty_stm32 --dry-run`：检查测试帧格式并执行 replay dry run。
- `./scripts/check_gateway_health.sh --service linux-iot-edge-gateway`：检查网关服务、进程、日志、缓存和通信端点状态。
- `./scripts/run_validation_workflow_check.sh`：执行串口 Replay、日志脱敏、Health Check 和环境采集的轻量工具链检查。
- `python3 scripts/sanitize_logs.py --input artifacts/fault_injection_raw.log --dry-run`：快速查看脱敏效果，不写文件。
- `python3 scripts/sanitize_logs.py --input artifacts/fault_injection_raw.log --output artifacts/fault_injection_sanitized.log`：生成脱敏日志归档。

这些工具用于复现、连续性检查、状态检查、问题回放和日志脱敏，不构成量产性能或工业现场可靠性结论。

## 手动运行

创建虚拟串口：

`socat -d -d pty,raw,echo=0,link=/tmp/tty_stm32 pty,raw,echo=0,link=/tmp/tty_gateway`

启动网关：

`~/linux-iot-edge-gateway-build/edge_gateway config/gateway.yaml /tmp/tty_gateway`

发送正常数据：

`python3 scripts/mock_serial_sender.py /tmp/tty_stm32`

发送半包：

`python3 scripts/inject_stream_cases.py /tmp/tty_stm32`

发送粘包：

`python3 scripts/inject_sticky_frames.py /tmp/tty_stm32`

订阅 MQTT：

`mosquitto_sub -h localhost -p 1883 -t 'sensor/+/data' -v`

## systemd 服务部署

完成编译后，可将网关安装为 Linux 后台服务。参数为实际串口设备，省略时默认使用 `/dev/ttyUSB0`：

`sudo ./scripts/install_systemd_service.sh /dev/ttyUSB0`

常用管理命令：

- 查看状态：`systemctl status linux-iot-edge-gateway`
- 实时日志：`journalctl -u linux-iot-edge-gateway -f`
- 重启服务：`sudo systemctl restart linux-iot-edge-gateway`
- 停止服务：`sudo systemctl stop linux-iot-edge-gateway`

停止服务时，systemd 发送 SIGTERM。网关会退出串口读取或重连循环，关闭文件描述符和网络客户端，记录停止日志并以状态码 `0` 退出。完整部署说明见 [systemd 部署指南](docs/deployment.md)。

## 可靠性设计

### 串口侧

- 使用固定容量环形缓冲区保存流式字节，消费数据时无需整体搬移内存
- 未完成帧保存在解析器内部
- 一次读取可解析多个连续帧
- CRC 错误帧不会进入业务层
- 非法长度不会导致缓冲区无限增长
- 串口不存在时每 2 秒重试
- 运行中断线后自动重新连接
- 重连时清除断线前残留半帧
- 每个串口使用独立线程、文件描述符和 `FrameParser`，单路断线不会阻塞其他串口
- MQTT 缓存发布和 TCP 发送使用互斥保护，允许多个串口线程安全共享上报通道

### 进程生命周期

- 使用 `sigaction()` 捕获 SIGINT 和 SIGTERM
- 信号处理函数只设置异步信号安全标志
- 串口读取、首次连接和重连等待均响应退出请求
- systemd 异常退出自动拉起，正常停止不会重启

### MQTT 侧

- 发布失败后消息写入 SQLite 持久化队列
- 缓存同时保存 Topic 和 Payload
- 每 5 秒尝试补传
- 按自增 ID 保持 FIFO 补传顺序
- 收到 PUBACK 后从队首删除成功消息
- 失败消息继续保留
- WAL 与 `synchronous=FULL` 降低异常掉电时的数据损坏风险
- 采用至少一次投递语义，极端崩溃窗口允许重复但不主动丢弃消息

## 测试结果

当前自动化测试结果：

- CTest：新增 Modbus RTU、CAN 解析、Modbus 超时和设备管理测试；实际数量以当前 `ctest` 输出为准
- 环形缓冲区回绕、半包保留与粘包解析测试通过
- CRC、半包、粘包和异常帧测试通过
- 数据解析、清洗和 JSON 测试通过
- MQTT 发布、离线缓存和补传测试通过
- MQTT 用户认证和 TLS 加密 smoke test 通过
- 串口到 MQTT 端到端 smoke test 通过
- 双串口并发接入与多设备 MQTT Topic smoke test 通过
- C++ TCP 客户端与 Python TCP 服务端 smoke test 通过
- 串口数据同时上报 MQTT 与 TCP 的手工联调通过
- 串口断开与恢复测试通过
- SIGINT/SIGTERM 单元测试和进程优雅退出 smoke test 通过
- ARM64 交叉编译、ELF 架构检查和 QEMU 启动测试通过

## 项目文档

- [系统架构](docs/architecture.md)
- [协议说明](docs/protocol.md)
- [多协议设备接入架构](docs/protocol_architecture.md)
- [Modbus RTU 接入](docs/modbus.md)
- [SocketCAN 接入](docs/socketcan.md)
- [测试用例](docs/test_cases.md)
- [调试记录](docs/debug_record.md)
- [systemd 部署指南](docs/deployment.md)
- [版本发布指南](docs/release.md)
- [模块实现说明](docs/implementation_notes.md)
- [STM32 硬件接入验证](docs/hardware_validation.md)
- [ARM64 稳定性测试](docs/arm64_stability_test.md)
- [OpenM3 与 Raspberry Pi 4 复现验证](docs/reproducible_validation.md)
- [公开复现证据台账](docs/reproducible_evidence_ledger.md)
- [故障注入测试](docs/fault_injection.md)
- [环境采集](docs/environment_capture.md)
- [稳定性测试](docs/stability_test.md)
- [串口 Replay 工具](docs/serial_replay.md)
- [Health Check](docs/health_check.md)
- [日志规范与脱敏](docs/logging.md)
- [工程验证工作流](docs/validation_workflow.md)
- [HTTP 可观测性](docs/http_observability.md)
- [Docker 本地复现](docker/README.md)
- [技术参考资料](docs/references.md)
- [变更记录](CHANGELOG.md)

## 当前实现说明

当前 MQTT 客户端基于 `libmosquitto` 实现原生长连接，使用独立网络循环处理连接状态和消息回调，并支持用户名/密码认证、CA 证书校验以及可选的双向 TLS。

传感器消息采用 QoS 1 发布，网关等待 Broker 返回 PUBACK 后才确认发送成功；连接中断时自动重连，离线消息默认写入 SQLite 持久化队列，连接恢复后按原顺序补传。`MessageCache` 接口隔离业务层与缓存实现，仍可通过 `cache.type: file` 使用兼容文件后端。

MQTT 与 TCP 均实现统一的 `Publisher` 接口。`PublisherGroup` 根据 YAML 配置注册启用通道并依次扇出同一条 JSON；单个通道失败只记录该通道结果，不会阻止其他通道继续发布。MQTT 的离线缓存和补传仍由 `ReliablePublisher` 单独负责。

旧版文件缓存可迁移到 SQLite：

`python3 scripts/migrate_file_cache.py data/pending_messages.cache data/pending_messages.db`

迁移脚本会校验全部旧记录、在事务中写入数据库，并将原文件重命名为 `.migrated` 备份。
## 后续计划

- 持续通过脱敏环境报告、稳定性摘要和公开复现证据台账记录可公开的验证信息
- 增加多板卡、多串口和不同数据频率下的对照测试
- 为 ARM64 部署包增加可复现构建元数据和设备侧自动验收脚本

## 许可证

本项目以 [MIT License](LICENSE) 发布。
