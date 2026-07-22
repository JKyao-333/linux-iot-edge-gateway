# SocketCAN 接入

## 1. 架构

Linux SocketCAN 将 CAN 控制器作为网络接口暴露。网关通过 `PF_CAN`、`SOCK_RAW` 和 `CAN_RAW` 打开接口，读取内核 `can_frame`，再转换为统一设备数据。

```text
CAN controller / vcan0
-> PF_CAN raw socket
-> can_frame
-> CAN parser
-> DeviceData
-> SensorData
-> PublisherGroup
```

## 2. 配置

```yaml
can:
  enabled: true
  interface: can0
  heartbeat_timeout_seconds: 5
```

虚拟测试时将 `interface` 改为 `vcan0`。

SocketCAN 可以作为唯一输入源运行，也可以与 UART、Modbus RTU 同时启用。配置启用后，接口不存在或绑定失败会进入设备重连流程，不影响其他输入设备继续工作。

## 3. CAN 帧检查

当前传感器帧使用标准 11-bit CAN ID 和 8 字节 Payload：

| 字节 | 含义 | 换算 |
|---:|---|---|
| 0..1 | 温度 | big-endian 有符号值 / 10 |
| 2..3 | 湿度 | big-endian 值 / 10 |
| 4..5 | 气体浓度 | big-endian ppm |
| 6 | 电池电压 | 值 * 100 mV |
| 7 | 状态 | 原始状态位 |

设备 ID 当前取标准 CAN ID 的低 8 位。解析器拒绝扩展 ID、DLC 非 8、RTR 帧和错误帧。具体设备使用的 CAN ID 分配和 Payload 映射必须在设备协议中单独登记。

## 4. vcan 测试

创建虚拟 CAN 接口：

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
ip -details link show vcan0
```

安装 `can-utils` 后可观察和发送帧：

```bash
candump vcan0
cansend vcan0 123#00FD0260012C2500
```

仓库集成测试也可直接使用 Python 原始 CAN socket 发送同一帧，因此不强制依赖 `cansend`。

运行三协议集成测试：

```bash
./scripts/run_protocol_integration_test.sh
```

脚本需要 `vcan0` 或创建 vcan 所需的 root/CAP_NET_ADMIN 权限，并验证 UART、Modbus RTU、SocketCAN 最终均发布到 MQTT。

## 5. 异常与状态

- Socket 打开、`ioctl`、`bind` 或读取失败会进入传输错误和重连流程；
- RTR、错误帧、扩展 ID 或 DLC 错误会增加 CAN/协议错误计数；
- 合法帧更新设备最近数据时间；
- 超过心跳窗口没有新数据时，聚合状态显示为离线。

## 6. 工程边界

当前实现只接收经典 CAN 标准数据帧，不支持 CAN FD、扩展帧、RTR 业务数据、错误帧业务处理、ISO-TP 或 J1939。`vcan0` 只验证 Linux SocketCAN 调用、帧解析和上层发布链路，不覆盖 CAN 收发器、电气终端、总线仲裁错误、EMC 或实际总线负载。连续帧单元测试用于检查解析稳定性，不声明吞吐量或实时性能指标。
