# 多协议设备接入架构

## 1. 目标

设备输入层在不改变现有 UART 自定义协议解析器的前提下，统一接入 UART、Modbus RTU 和 SocketCAN。上层只接收 `SensorData`，不依赖输入协议的帧格式、文件描述符或轮询方式。

## 2. 数据流

```text
UART custom frame ----> UartDevice -----+
Modbus RTU registers -> ModbusDevice ----+--> DeviceManager
SocketCAN can_frame --> CanDevice -------+        |
                                                  v
                                             SensorData
                                                  |
                                                  v
                                           PublisherGroup
                                            /     |     \
                                         MQTT    TCP   HTTP observability
```

输入协议只负责传输、帧解析和字段映射。JSON 序列化、Topic 生成、MQTT 离线缓存和 TCP 发布仍位于现有业务与发布层。

## 3. 统一接口

`src/device/device_interface.h` 定义 `DeviceInterface`：

- `start()`：打开串口或 SocketCAN 接口；
- `stop()`：关闭底层资源；
- `read()`：返回数据、超时、协议错误或传输错误；
- `get_device_status()`：返回协议类型、在线状态、最近更新时间和错误计数。

三个输入适配器实现相同接口：

| 适配器 | 输入 | 解析实现 |
|---|---|---|
| `UartDevice` | termios 字节流 | 复用原有 `FrameParser` |
| `ModbusDevice` | RS485/串口 RTU 响应 | `modbus_rtu` |
| `CanDevice` | Linux `can_frame` | `can_parser` |

## 4. 统一数据模型

协议解析结果先转换为 `DeviceData`，再由公共转换函数生成并校验 `SensorData`。MQTT/TCP 发布层因此不需要判断数据来自 UART、Modbus RTU 还是 SocketCAN。

当前统一字段包括设备 ID、温度、湿度、气体浓度、电池电压、状态和序号。不同设备寄存器或 CAN Payload 的实际映射应通过设备台账确认后再调整。

## 5. 状态管理

`DeviceManager` 为每个输入适配器维护独立工作线程，并聚合：

- `device_id`；
- `protocol`；
- `online/offline`；
- `last_update_unix_seconds`；
- `error_count`。

传输错误会关闭当前输入并按串口重连周期重新打开；协议错误只丢弃当前数据，不中止其他输入。HTTP Health 和 Prometheus 指标读取聚合后的设备状态。`/health` 的 `devices` 数组按设备输出 `device_id`、`protocol`、`online`、`last_update` 和 `error_count`，便于定位单个输入通道的状态变化。

新增指标：

- `gateway_device_online_total`；
- `gateway_device_offline_total`；
- `gateway_protocol_error_total`；
- `gateway_modbus_error_total`；
- `gateway_can_error_total`。

## 6. 工程边界

当前 Modbus 寄存器表和 CAN Payload 是仓库定义的参考映射，用于验证多协议接入架构。部署到具体设备时必须按照设备协议手册更新映射并执行硬件联调。本实现和 vcan/socat 测试不构成工业现场、量产可靠性或固定性能结论。
