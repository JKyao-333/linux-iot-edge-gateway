# Linux C++ 多协议物联网边缘网关

基于 Linux C++17 实现的多协议物联网边缘网关，用于连接 STM32 多传感器终端、MQTT Broker 和 TCP Server。
网关接收物理串口或 Python 虚拟串口数据，完成自定义协议解析、CRC16 校验、数据清洗、JSON 封装、MQTT/TCP 双通道上报、MQTT 离线缓存、恢复补传、串口自动重连和日志记录。
## 核心能力

- termios 串口配置与原始字节接收
- 自定义二进制协议解析
- CRC16-Modbus 校验
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
- DEBUG、INFO、WARN、ERROR 日志
- Python、Shell 和 CTest 自动化测试

## 数据链路

STM32 / Python 模拟器
-> 串口
-> Linux termios
-> FrameParser
-> CRC16
-> SensorData
-> 数据清洗
-> JSON
-> ReliablePublisher
-> MQTT Broker

发布失败时：

MQTT 发布失败
-> FileCache 本地缓存
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
- `src/app/`：SensorData、数据清洗和 JSON
- `src/mqtt/`：MQTT 客户端和可靠发布器
- `src/tcp/`：原生 TCP Socket 客户端
- `src/cache/`：本地文件缓存
- `src/log/`：分级日志
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
- yaml-cpp
Ubuntu 安装命令：

`sudo apt update`

`sudo apt install -y build-essential cmake git python3 python3-pip socat mosquitto mosquitto-clients libyaml-cpp-dev`
安装 pyserial：

`python3 -m pip install pyserial`
## YAML 配置

网关运行参数统一保存在 `config/gateway.yaml` 中。

主要配置项：

| 配置项 | 说明 |
|---|---|
| `serial.device` | 串口设备路径 |
| `serial.baud_rate` | 串口波特率 |
| `serial.reconnect_interval_seconds` | 串口断线重连间隔 |
| `mqtt.host` | MQTT Broker 地址 |
| `mqtt.port` | MQTT Broker 端口 |
| `mqtt.topic_prefix` | MQTT Topic 前缀 |
| `mqtt.cache_retry_interval_seconds` | 离线缓存补传周期 |
| `tcp.enabled` | 是否启用 TCP 上报 |
| `tcp.host` | TCP Server 地址 |
| `tcp.port` | TCP Server 端口 |
| `cache.path` | MQTT 离线缓存文件路径 |
| `log.path` | 网关日志文件路径 |
| `log.level` | 最低日志级别 |

程序启动格式：

`edge_gateway [配置文件] [串口设备覆盖值]`

使用默认配置启动：

`~/linux-iot-edge-gateway-build/edge_gateway config/gateway.yaml`

临时覆盖串口设备：

`~/linux-iot-edge-gateway-build/edge_gateway config/gateway.yaml /tmp/tty_gateway`

配置文件包含类型检查和范围检查。端口、波特率、重连周期或日志级别无效时，程序将输出错误并拒绝启动。
## 编译

推荐将构建目录放在 WSL Linux 文件系统中，避免 Windows 挂载目录的时间戳和链接问题。

配置：

`cmake -S . -B ~/linux-iot-edge-gateway-build`

编译：

`cmake --build ~/linux-iot-edge-gateway-build --parallel`

生成的网关程序：

`~/linux-iot-edge-gateway-build/edge_gateway`

## 一键测试

启动 Mosquitto：

`sudo service mosquitto start`

运行全部测试：

`./scripts/run_smoke_test.sh`

该命令会完成：

1. CMake 配置
2. C++ 工程编译
3. 20 个 CTest 测试
4. YAML 正常配置与异常配置测试
5. 虚拟串口创建
6. Python 半包发送
7. 串口协议解析与 JSON 生成
8. MQTT 发布验证
9. 原生 TCP 发布验证
成功时最终输出：

`[PASS] all smoke tests passed`

## 手动运行

创建虚拟串口：

`socat -d -d pty,raw,echo=0,link=/tmp/tty_stm32 pty,raw,echo=0,link=/tmp/tty_gateway`

启动网关：

`~/linux-iot-edge-gateway-build/`~/linux-iot-edge-gateway-build/edge_gateway config/gateway.yaml /tmp/tty_gateway`

发送正常数据：

`python3 scripts/mock_serial_sender.py /tmp/tty_stm32`

发送半包：

`python3 scripts/inject_stream_cases.py /tmp/tty_stm32`

发送粘包：

`python3 scripts/inject_sticky_frames.py /tmp/tty_stm32`

订阅 MQTT：

`mosquitto_sub -h localhost -p 1883 -t 'sensor/+/data' -v`

## 可靠性设计

### 串口侧

- 未完成帧保存在解析器内部
- 一次读取可解析多个连续帧
- CRC 错误帧不会进入业务层
- 非法长度不会导致缓冲区无限增长
- 串口不存在时每 2 秒重试
- 运行中断线后自动重新连接
- 重连时清除断线前残留半帧

### MQTT 侧

- 发布失败后消息写入本地缓存
- 缓存同时保存 Topic 和 Payload
- 每 5 秒尝试补传
- 成功消息从缓存删除
- 失败消息继续保留

## 测试结果

当前自动化测试结果：

- CTest：18/18 通过
- CRC、半包、粘包和异常帧测试通过
- 数据解析、清洗和 JSON 测试通过
- MQTT 发布、离线缓存和补传测试通过
- 串口到 MQTT 端到端 smoke test 通过
- C++ TCP 客户端与 Python TCP 服务端 smoke test 通过
- 串口数据同时上报 MQTT 与 TCP 的手工联调通过
- 串口断开与恢复测试通过

## 项目文档

- [系统架构](docs/architecture.md)
- [协议说明](docs/protocol.md)
- [测试用例](docs/test_cases.md)
- [调试记录](docs/debug_record.md)

## 当前实现说明

当前 MQTT 客户端通过 `mosquitto_pub` 子进程发布消息，适合快速验证完整数据链路。

后续可替换为 Eclipse Paho 或 libmosquitto，实现长连接、QoS 回调和更完整的连接状态管理。

## 后续计划

- 将文件缓存升级为 SQLite
- 抽象统一 Publisher 接口
- 增加多串口和多设备并发
- 增加 systemd 服务
- 增加 ARM 交叉编译
