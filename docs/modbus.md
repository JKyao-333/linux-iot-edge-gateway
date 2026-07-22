# Modbus RTU 接入

## 1. 链路

```text
RS485 device
-> Linux serial port
-> ModbusDevice (master)
-> Modbus response parser
-> DeviceData
-> SensorData
-> PublisherGroup
```

网关作为 Modbus RTU Master 周期发送读请求。Modbus 数据不会绕过设备数据模型直接发布到 MQTT。

## 2. 支持范围

- 功能码 `0x03`：Read Holding Registers；
- 功能码 `0x04`：Read Input Registers；
- Slave 地址：1 至 247；
- 单次读取：1 至 125 个寄存器；
- CRC16-Modbus，CRC 低字节先发送；
- 可配置轮询周期和响应超时。

## 3. RTU 帧格式

读请求固定为 8 字节：

| 字段 | 长度 |
|---|---:|
| Slave ID | 1 B |
| Function | 1 B |
| Start address | 2 B, big-endian |
| Register count | 2 B, big-endian |
| CRC16 | 2 B, little-endian |

正常响应：`Slave ID | Function | Byte count | Register data | CRC16`。

异常响应：`Slave ID | Function + 0x80 | Exception code | CRC16`。

## 4. 配置

```yaml
modbus:
  enabled: true
  port: /dev/ttyUSB0
  baud_rate: 115200
  slave_id: 1
  function_code: 3
  start_address: 0
  register_count: 6
  poll_interval_ms: 1000
  response_timeout_ms: 500
```

`baudrate` 作为兼容别名也可读取；仓库配置统一使用 `baud_rate`。

## 5. 参考寄存器映射

| 偏移 | 含义 | 换算 |
|---:|---|---|
| 0 | 温度 | 有符号值 / 10 摄氏度 |
| 1 | 湿度 | 值 / 10 % |
| 2 | 气体浓度 | ppm |
| 3 | 电池电压 | mV |
| 4 | 设备状态 | 低 8 位 |
| 5 | 数据序号 | 原始值 |

该映射是网关参考设备模型，不应替代具体从站的寄存器手册。

## 6. 异常处理

| 场景 | 行为 |
|---|---|
| 无响应 | 到达 `response_timeout_ms` 后记录超时和 Modbus 错误 |
| CRC 错误 | 丢弃响应，增加协议与 Modbus 错误计数 |
| Slave 异常响应 | 记录异常码，不生成 `SensorData` |
| 功能码或地址不匹配 | 丢弃响应并记录协议错误 |
| Byte count/帧长度错误 | 丢弃响应并记录协议错误 |
| 串口读写错误 | 关闭串口，由 `DeviceManager` 重新打开 |

## 7. 测试

```bash
ctest --test-dir ~/linux-iot-edge-gateway-build -R 'modbus' --output-on-failure
```

`tests/modbus/modbus_rtu_test.cpp` 使用固定已知字节验证 FC03、FC04、CRC、异常响应和长度错误。`tests/modbus/modbus_device_timeout_test.cpp` 使用 PTY 验证无响应超时。`scripts/mock_modbus_rtu_slave.py` 为三协议集成测试提供最小 RTU 从站。

## 8. 工程边界

当前实现是单请求串行轮询模型，未实现写寄存器、多从站调度或设备厂商寄存器模板。socat/PTTY 测试验证软件行为，不等同于 RS485 总线电气特性、终端电阻、EMC 或现场布线验证。
