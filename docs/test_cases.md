# 测试用例说明

## 1. 测试目标

项目测试覆盖 CRC16、自定义协议解析、半包与粘包、异常帧过滤、传感器数据清洗、JSON 封装、MQTT 发布、离线缓存、恢复补传、串口重连和日志记录。

## 2. 测试环境

- Ubuntu 22.04 / WSL2
- GCC 11 / C++17
- CMake 3.22
- Python 3.10 / pyserial
- socat
- Mosquitto 2.0

## 3. 自动化测试命令

启动 Broker：`sudo service mosquitto start`

运行全部测试：`./scripts/run_smoke_test.sh`

仅运行 CTest：`ctest --test-dir ~/linux-iot-edge-gateway-build --output-on-failure`

通过标准：24 个 CTest、串口到 MQTT 端到端测试、TCP smoke test 和缓存迁移测试全部通过。
## 4. C++ 测试矩阵

| 编号 | 测试目标 | 验证内容 |
|---|---|---|
| TC-001 | crc16_test | CRC16-Modbus 计算 |
| TC-002 | frame_test | 协议帧字段与 CRC |
| TC-003 | frame_parser_test | 单个完整帧解析 |
| TC-004 | frame_parser_stream_test | 粘包解析 |
| TC-005 | frame_parser_half_test | 半包缓存与拼接 |
| TC-006 | frame_parser_bad_crc_test | CRC 错误帧过滤 |
| TC-007 | frame_parser_crc_diagnostic_test | CRC 错误计数 |
| TC-008 | frame_parser_length_diagnostic_test | 非法长度检测与恢复 |
| TC-009 | sensor_data_struct_test | SensorData 字段 |
| TC-010 | sensor_data_parse_test | Payload 字段解析 |
| TC-011 | sensor_data_validate_test | 数据范围检查 |
| TC-012 | sensor_data_json_test | JSON 序列化 |
| TC-013 | file_cache_test | 缓存追加与读取 |
| TC-014 | file_cache_replace_test | 缓存原子替换 |
| TC-015 | reliable_publisher_test | 发布失败后缓存 |
| TC-016 | logger_test | 四级日志输出 |
| TC-017 | mqtt_client_test | MQTT 消息发布 |
| TC-018 | cache_replay_test | 缓存恢复补传 |
| TC-019 | gateway_config_test | 正常 YAML 配置加载与字段检查 |
| TC-020 | gateway_config_invalid_test | 非法端口、波特率和 YAML 格式拒绝 |
| TC-021 | shutdown_signal_test | SIGINT/SIGTERM 退出标志 |
| TC-022 | sqlite_cache_test | SQLite FIFO 追加、读取与队首删除 |
| TC-023 | sqlite_cache_persistence_test | 进程重启后的缓存持久化与顺序 |
| TC-024 | publisher_group_test | 多通道扇出、逐通道结果和失败隔离 |
## 5. 端到端与故障测试

| 编号 | 测试场景 | 预期结果 |
|---|---|---|
| ET-001 | Python 分两次发送一个帧 | 网关只解析并上报一次 |
| ET-002 | 两个完整帧一次写入 | 网关解析并上报两次 |
| FT-001 | CRC 错误帧后跟正常帧 | 错误帧丢弃，正常帧解析 |
| FT-002 | 非法长度帧后跟正常帧 | 记录长度错误并恢复解析 |
| FT-003 | 启动时串口不存在 | 每 2 秒重试且进程不退出 |
| FT-004 | 运行中串口断开再恢复 | 自动重连并继续接收数据 |
| FT-005 | MQTT Broker 离线 | 发布失败消息写入本地缓存 |
| FT-006 | MQTT Broker 恢复 | SQLite 队列按顺序补传并删除已确认消息 |

## 6. 串口端到端测试

运行命令：`./scripts/run_serial_smoke_test.sh`

测试链路：

Python 模拟器 -> 虚拟串口 -> termios -> FrameParser -> SensorData -> JSON -> MQTT

通过标准：

- 两段串口数据组成一个完整帧。
- 只生成一条传感器 JSON。
- MQTT 发布成功。
- 无 CRC 或长度错误。

## 7. 阶段验收标准

- 24 个 CTest 全部通过。
- 串口到 MQTT smoke test 通过。
- 半包、粘包和异常帧处理正确。
- 串口断开后能够自动重连。
- MQTT 离线数据能够缓存和补传。
- 旧文件缓存能够事务化迁移到 SQLite。
- 一条 Shell 命令能够完成构建与测试。
## 8. TCP 上报测试

### TCP-01：正常连接与 JSON 发送

前置条件：

- TCP 端口 `9000` 未被占用
- `tcp_client_test` 已成功编译

测试步骤：

1. 启动 `scripts/mock_tcp_server.py`
2. C++ 客户端连接 `127.0.0.1:9000`
3. 客户端发送一条 JSON
4. 服务端按照换行符提取完整消息

预期结果：

- 客户端输出 `TCP connected`
- 客户端输出 `TCP send ok`
- 客户端输出 `TCP test passed`
- 服务端输出完整的 `[RX]` JSON
- 脚本输出 `[PASS] TCP smoke test passed`

自动化命令：

`./scripts/run_tcp_smoke_test.sh`

### TCP-02：TCP 服务端离线

前置条件：

- `9000` 端口没有 TCP 服务端监听
- Mosquitto 正常运行

测试步骤：

1. 启动边缘网关
2. 通过虚拟串口发送合法传感器帧
3. 观察 MQTT 和 TCP 日志

预期结果：

- MQTT 数据仍能正常发布
- TCP 客户端报告连接失败
- 网关记录 `TCP publish failed`
- 网关进程不会因 TCP 失败退出

### TCP-03：MQTT 与 TCP 双通道上报

前置条件：

- Mosquitto 正在监听 `1883`
- Python TCP 服务端正在监听 `9000`
- 虚拟串口设备已经创建

测试步骤：

1. 启动 MQTT 订阅端
2. 启动 Python TCP 服务端
3. 启动边缘网关
4. Python 串口模拟器发送一帧合法数据

预期结果：

- 网关成功解析协议帧
- MQTT 订阅端收到传感器 JSON
- TCP 服务端收到相同的传感器 JSON
- 网关输出 `MQTT publish acknowledged`
- 网关输出 `TCP send ok`
## 9. YAML 配置测试

仅运行配置测试：

`ctest --test-dir ~/linux-iot-edge-gateway-build -R 'gateway_config.*test' --output-on-failure`

### CFG-01：正常配置加载

输入：

- `config/gateway.yaml`

验证内容：

- 串口设备和波特率正确
- MQTT 地址、端口和 Topic 前缀正确
- TCP 开关、地址和端口正确
- 缓存与日志路径正确
- 时间间隔配置正确

预期结果：

- 配置加载成功
- 所有字段与 YAML 内容一致

### CFG-02：异常配置拒绝

测试输入：

- MQTT 端口为 `70000`
- 串口波特率为 `12345`
- YAML 语法格式错误

预期结果：

- 配置加载失败
- 返回明确错误信息
- 网关拒绝使用无效配置启动

### CFG-03：运行时配置生效

测试配置：

- `mqtt.topic_prefix` 设置为 `lab`
- `tcp.enabled` 设置为 `false`
- `mqtt.cache_retry_interval_seconds` 设置为 `3`

预期结果：

- MQTT 数据发布到 `lab/16/data`
- 不建立 TCP 连接
- MQTT 发布与串口解析保持正常

## 10. 进程生命周期与 systemd 测试

### LIFE-01：信号处理单元测试

测试程序：`shutdown_signal_test`

验证内容：

- SIGINT 被捕获并设置退出标志
- SIGTERM 被捕获并设置退出标志
- 信号处理过程不会直接终止测试进程

### LIFE-02：SIGTERM 优雅退出

自动化命令：`./scripts/run_shutdown_smoke_test.sh`

测试步骤：

1. 创建临时 PTY 串口对和隔离配置。
2. 启动网关并等待进入串口读取循环。
3. 向网关进程发送 SIGTERM。
4. 等待进程结束并检查退出码和日志。

预期结果：

- 网关在 5 秒内停止
- 进程退出码为 `0`
- 日志包含 `shutdown requested`
- 日志包含 `edge gateway stopped`

### LIFE-03：systemd 服务管理

安装命令：`sudo ./scripts/install_systemd_service.sh /dev/ttyUSB0`

预期结果：

- 服务以 `iot-gateway` 用户运行
- `systemctl status linux-iot-edge-gateway` 显示 active
- `systemctl stop` 后日志记录正常停止
- 异常退出后 systemd 根据 `Restart=on-failure` 自动拉起
