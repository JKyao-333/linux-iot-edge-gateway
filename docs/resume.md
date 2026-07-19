# 简历表述与证据索引

本文档用于把项目中的真实实现整理成可核验的简历内容。面试时应以仓库代码、测试输出和调试记录为依据，不填写未经测试的吞吐量、时延、在线率或真机运行时长。

## 1. 项目名称

基于 Linux C++ 的多协议物联网边缘网关

技术栈：Linux、C++17、CMake、termios、POSIX Socket、MQTT、SQLite、YAML、JSON、Python、Shell、CTest、systemd、AArch64

## 2. 简历项目描述

- 基于 Linux C++17 和 CMake 设计多协议物联网边缘网关，完成串口数据接收、自定义二进制协议解析、传感器数据清洗、JSON 封装以及 MQTT/TCP 双通道上报，打通 STM32 传感终端到边缘侧与服务端的数据链路。
- 设计 `0xAA 0x55` 帧头、长度、命令字、设备 ID、Payload 和 CRC16-Modbus 字段，使用固定容量环形缓冲区与增量状态解析处理半包、粘包、前置噪声、非法长度和 CRC 错误，并实现串口断开后的自动重连。
- 基于 libmosquitto 实现 MQTT QoS 1 长连接发布、认证与 TLS；使用 SQLite WAL 持久化离线消息并在 Broker 恢复后按序补传，通过抽象 Publisher 接口扩展原生 TCP/JSON Lines 上报。
- 使用 Python、Shell 和 CTest 构建自动化测试链路，覆盖协议解析、数据清洗、缓存持久化、MQTT 安全连接、串口端到端、多串口并发、优雅退出和 TCP 上报；使用 GitHub Actions 完成 Ubuntu 原生测试与 AArch64 交叉构建验证。

实际简历通常保留前三条或从四条中选择最贴合岗位 JD 的三条。

## 3. 精简版本

适用于版面紧张的单页简历：

- 基于 Linux C++17/CMake 实现物联网边缘网关，完成 termios 串口接收、自定义协议解析、JSON 封装及 MQTT/TCP 双通道上报。
- 使用环形缓冲区、CRC16-Modbus 和增量解析处理半包、粘包、异常帧与串口断连；基于 SQLite 实现 MQTT 离线缓存和恢复补传。
- 使用 Python/Shell/CTest 覆盖协议、缓存、网络和端到端场景，并完成 AArch64 交叉构建、QEMU 启动检查及 GitHub Actions 持续集成。

## 4. 代码证据索引

| 简历能力 | 主要实现 | 验证入口 |
| --- | --- | --- |
| termios 串口接收与重连 | `src/main.cpp` | `scripts/run_serial_smoke_test.sh`、`scripts/run_multi_serial_smoke_test.sh` |
| 环形缓冲区 | `src/protocol/byte_ring_buffer.h` | `tests/byte_ring_buffer_test.cpp`、`tests/frame_parser_ring_buffer_test.cpp` |
| 协议与 CRC | `src/protocol/frame_parser.cpp`、`src/protocol/crc16.cpp` | `tests/frame_parser_*`、`tests/crc16_test.cpp` |
| 数据解析与清洗 | `src/app/sensor_data.cpp` | `tests/sensor_data_*` |
| MQTT QoS 1、认证和 TLS | `src/mqtt/mqtt_client.cpp` | `tests/mqtt_client_test.cpp`、`scripts/run_mqtt_security_smoke_test.sh` |
| SQLite 离线缓存 | `src/cache/sqlite_cache.cpp` | `tests/sqlite_cache_*`、`tests/cache_replay_test.cpp` |
| TCP 上报 | `src/tcp/tcp_client.cpp`、`src/publisher/tcp_publisher.cpp` | `tests/tcp_client_test.cpp`、`scripts/run_tcp_smoke_test.sh` |
| 多通道发布 | `src/publisher/publisher_group.cpp` | `tests/publisher_group_test.cpp` |
| YAML 配置 | `src/config/gateway_config.cpp` | `tests/gateway_config_*` |
| 日志与优雅退出 | `src/log/logger.cpp`、`src/app/shutdown_signal.cpp` | `tests/logger_test.cpp`、`tests/shutdown_signal_test.cpp` |
| systemd 部署 | `scripts/install_systemd_service.sh` | `scripts/run_shutdown_smoke_test.sh`、`docs/deployment.md` |
| ARM64 构建 | `cmake/toolchains/aarch64-linux-gnu.cmake` | `scripts/build_aarch64.sh`、`.github/workflows/ci.yml` |

## 5. 可量化但不夸大的结果

- 仓库注册 28 个 CTest 测试，当前本地与 CI 均全部通过。
- 自动化链路覆盖 Ubuntu 原生构建和 AArch64 交叉构建。
- 协议测试覆盖半包、粘包、环形缓冲区回绕、CRC 错误和非法长度。
- 端到端 smoke test 覆盖串口到 MQTT、多串口接入、MQTT TLS、TCP 上报、缓存迁移和优雅退出。

这些数字描述的是测试资产和验证结果，不等同于生产环境性能指标。

## 6. 当前边界

- MQTT 可靠性采用 QoS 1 和至少一次投递语义，故障恢复窗口允许重复消息，不能表述为“严格一次投递”。
- ARM64 已完成交叉编译、ELF 架构检查和 QEMU 启动检查，但尚未完成真实开发板长时间稳定性测试。
- 当前网关以单进程、多串口工作线程方式运行，不应表述为分布式系统或高并发云平台。
- 项目没有经过正式压测，不填写吞吐量、P99 时延或在线率。

## 7. 面试展示顺序

1. 用 30 秒说明 STM32、串口网关、MQTT/TCP 服务端三段链路。
2. 展示 `docs/architecture.md` 和 `docs/protocol.md`。
3. 运行 `./scripts/run_smoke_test.sh`，说明 28 个 CTest 与端到端 smoke test 的区别。
4. 选择一个故障场景，结合 `docs/debug_record.md` 解释定位过程。
5. 最后说明真机长稳测试仍是下一步，体现工程边界意识。
