# 故障注入测试

## 1. 测试目标

`scripts/run_fault_injection_test.sh` 用于验证网关面对异常串口输入、网络通道中断和进程退出请求时的可恢复行为。脚本以软件故障注入为主，自动构建网关、创建隔离的虚拟串口和网络服务，并生成可归档结果。

## 2. 测试拓扑

```text
Python 帧注入器
    -> socat 虚拟串口
    -> edge_gateway
       -> Mosquitto Broker -> mosquitto_sub
       -> Python TCP Server
       -> SQLite WAL 缓存
```

Mosquitto 和 TCP Server 使用脚本分配的本地空闲端口，不依赖默认开发端口。所有后台进程均由脚本记录并在退出时清理。

## 3. 场景矩阵

| ID | 场景 | 注入方式 | 预期行为 |
| --- | --- | --- | --- |
| FI-01 | 正常帧基线 | 发送一帧合法传感器数据 | 解析并发布 JSON |
| FI-02 | CRC 错误帧 | 修改合法帧 CRC | 记录协议异常，不进入发布链路 |
| FI-03 | 非法长度帧 | 注入越界 Payload 长度后再发合法帧 | 记录长度异常，缓冲区不持续增长并恢复解析 |
| FI-04 | 前置噪声 | 垃圾字节后拼接合法帧 | 重新同步帧头并解析合法帧 |
| FI-05 | 半包 | 分两次写入同一帧 | 完整帧到达前等待，到达后发布一次 |
| FI-06 | 粘包 | 一次写入两帧 | 连续解析并发布两条消息 |
| FI-07 | MQTT Broker 离线 | 停止脚本启动的 Broker | 发布失败消息写入 SQLite 队列 |
| FI-08 | MQTT Broker 恢复 | 重启 Broker | 缓存按队列顺序补传并清空 |
| FI-09 | TCP Server 离线 | 停止 TCP 测试服务 | TCP 失败不阻塞 MQTT 发布 |
| FI-10 | 串口断开与恢复 | 终止并重建 socat PTY | 网关保持运行、重连并继续解析完整帧 |
| FI-11 | SIGTERM 优雅退出 | 向网关发送 SIGTERM | 正常退出并记录停止路径 |

## 4. 使用方法

```bash
./scripts/run_fault_injection_test.sh
```

如默认构建目录不存在，脚本会执行 CMake 配置与编译。缺少 `cmake`、`python3`、`pyserial`、`socat`、`mosquitto`、`mosquitto_pub`、`mosquitto_sub` 或 `sqlite3` 时，脚本会明确列出缺失项并停止。

## 5. 输出文件

- `artifacts/fault_injection_summary.md`：测试时间、Git commit、构建目录、场景结果表和失败摘要。
- `artifacts/fault_injection_raw.log`：网关、Broker、注入器和测试服务的合并原始输出。

`artifacts/` 默认不进入版本库。需要形成实验台账时，应先检查和脱敏，再将摘要复制到受控文档位置。

## 6. 工程边界

- 该测试不代表工业现场 EMC、电源扰动或极端网络环境验证。
- 该测试不声明固定吞吐量或 P99 时延。
- 该测试不替代真实硬件长期稳定性测试。
- 该测试结果不构成工业现场量产可靠性结论。
