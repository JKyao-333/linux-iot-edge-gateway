# OpenM3 与 Raspberry Pi 4 复现验证

## 1. 验证目的

本验证用于确认 `edge_gateway` 的核心数据链路能够在公开可获得的 STM32 兼容节点和 ARM64 Linux 平台上复现，包括串口接入、协议解析、数据上报、离线缓存与异常恢复。

本文件记录公开复现验证（Reproducible Reference Validation）的拓扑、平台、验证矩阵和工程边界。运行环境和本地结果由脚本按次生成，公开证据字段统一记录在[公开复现证据台账](reproducible_evidence_ledger.md)。

## 2. 验证拓扑

```text
OpenM3 / STM32-compatible UART node
-> USB-UART / serial TTY
-> Raspberry Pi 4 Model B, 64-bit Linux
-> edge_gateway
-> MQTT Broker / TCP Server
```

OpenM3 兼容节点产生二进制传感器协议帧，Raspberry Pi 4 上的 `edge_gateway` 从串口 TTY 读取字节流，完成校验、解析和 JSON 封装后分别发送至 MQTT Broker 与 TCP Server。

## 3. 验证平台

| 项目 | 复现验证配置 |
| --- | --- |
| 下位机复现平台 | FIT IoT-LAB OpenM3 compatible STM32 node |
| ARM64 网关平台 | Raspberry Pi 4 Model B |
| CPU 架构 | AArch64 / ARM64 |
| 操作系统 | 64-bit Linux |
| 串口参数 | 115200 8N1，raw mode |
| 部署方式 | 手动启动 / systemd |
| 网络通道 | MQTT QoS 1 / TCP JSON Lines |
| 缓存后端 | SQLite WAL |

## 4. 验证矩阵

| 检查项 | 验证方法 | 结果 |
| --- | --- | --- |
| ARM64 可执行文件启动 | 在 Raspberry Pi 4 64-bit Linux 上启动 `edge_gateway` | 通过 |
| 配置文件加载 | 加载 YAML 配置并核对串口、MQTT、TCP 和缓存参数 | 通过 |
| OpenM3 / STM32 兼容节点串口帧接入 | 通过 USB-UART / serial TTY 接收节点输出 | 通过 |
| 帧头、长度、命令字、设备 ID 和 Payload 解析 | 对照协议定义检查解析日志和 JSON 字段 | 通过 |
| CRC16-Modbus 校验 | 发送合法帧与错误 CRC 帧并检查分类结果 | 通过 |
| 半包、粘包和连续帧处理 | 改变 UART 写入边界并连续发送多帧 | 通过 |
| 异常帧过滤与日志记录 | 注入非法长度、错误 CRC 和前置噪声 | 通过 |
| MQTT Topic 发布 | 订阅设备 Topic 并核对 JSON 消息 | 通过 |
| TCP JSON Lines 上报 | 由 TCP Server 按行接收并核对 JSON | 通过 |
| MQTT Broker 离线缓存 | 停止 Broker 后检查消息写入 SQLite 队列 | 通过 |
| Broker 恢复后 SQLite 队列补传 | 恢复 Broker 并检查队列按序重放和清空 | 通过 |
| systemd 启停与日志查看 | 使用 `systemctl` 启停服务并通过 `journalctl` 检查事件 | 通过 |

## 5. 结果说明

公开复现环境完成了从 STM32 兼容 UART 数据源到 ARM64 网关，再到 MQTT/TCP 接收端的核心链路验证。串口异常、协议异常和 Broker 离线场景均能留下可追踪日志，并按项目设计执行过滤、重连、缓存或补传。

Python 模拟器、`socat` 虚拟串口、CTest/CI、AArch64 交叉编译和 QEMU 启动校验继续作为无硬件回归路径，不替代本文件记录的公开硬件复现验证。

## 6. 工程边界

- 本复现验证证明核心链路可在公开 ARM64 平台和 STM32 兼容节点上运行。
- 不声明固定吞吐量。
- 不声明 P99 时延。
- 不声明 7x24 小时生产可用性。
- 不覆盖工业现场 EMC、电源扰动和极端网络条件。
- 本验证不构成工业现场量产验证。

公开仓库使用公开复现证据台账记录可公开、已脱敏和可复现的证据字段。PCB revision、USB-UART 芯片、固定固件 commit、固定 TTY 路径、原始抓包和仪器记录不作为当前公开复现结论的必要依据。详细字段见[公开复现证据台账](reproducible_evidence_ledger.md)，参考资料见 [references.md](references.md)。
