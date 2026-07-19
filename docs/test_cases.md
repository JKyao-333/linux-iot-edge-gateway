# 测试用例说明

## 1. 测试目标

项目测试覆盖 CRC16、自定义协议解析、半包与粘包、异常帧过滤、传感器数据清洗、JSON 封装、MQTT 发布、认证与 TLS、离线缓存、恢复补传、串口重连、多串口并发和日志记录。

## 2. 测试环境

- Ubuntu 22.04 / WSL2
- GCC 11 / C++17
- CMake 3.22
- Python 3.10 / pyserial
- socat
- Mosquitto 2.0
- OpenSSL 3
- GNU AArch64 交叉编译器 / QEMU user mode

## 3. 自动化测试命令

启动 Broker：`sudo service mosquitto start`

运行全部测试：`./scripts/run_smoke_test.sh`

仅运行 CTest：`ctest --test-dir ~/linux-iot-edge-gateway-build --output-on-failure`

通过标准：28 个 CTest、单串口与双串口 MQTT 端到端测试、MQTT 认证与 TLS smoke test、TCP smoke test 和缓存迁移测试全部通过。

ARM64 构建验证：`./scripts/build_aarch64.sh`
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
| TC-025 | byte_ring_buffer_test | 固定容量、回绕顺序、满缓冲拒绝与消费 |
| TC-026 | frame_parser_ring_buffer_test | 噪声过滤、半包保留、粘包解析与无溢出验证 |
| TC-027 | release_version_test | `VERSION` 语义化格式及标签一致性 |
| TC-028 | version_cli_test | 网关 `--version` 输出与构建版本一致 |
## 5. 端到端与故障测试

| 编号 | 测试场景 | 预期结果 |
|---|---|---|
| ET-001 | Python 分两次发送一个帧 | 网关只解析并上报一次 |
| ET-002 | 两个完整帧一次写入 | 网关解析并上报两次 |
| ET-003 | 两个虚拟串口并发发送不同设备帧 | 两个工作线程分别解析，MQTT 收到两个设备 Topic |
| FT-001 | CRC 错误帧后跟正常帧 | 错误帧丢弃，正常帧解析 |
| FT-002 | 非法长度帧后跟正常帧 | 记录长度错误并恢复解析 |
| FT-003 | 启动时串口不存在 | 每 2 秒重试且进程不退出 |
| FT-004 | 运行中串口断开再恢复 | 自动重连并继续接收数据 |
| FT-005 | MQTT Broker 离线 | 发布失败消息写入本地缓存 |
| FT-006 | MQTT Broker 恢复 | SQLite 队列按顺序补传并删除已确认消息 |
| SEC-001 | 使用临时 CA、服务端证书和密码文件启动隔离 Broker | C++ 客户端认证成功、TLS 校验成功并完成 QoS 1 发布 |
| SEC-002 | 密码无用户名或 TLS 缺少必要证书字段 | 配置加载失败并返回明确原因 |

## 6. 串口端到端测试

运行命令：`./scripts/run_serial_smoke_test.sh`

测试链路：

Python 模拟器 -> 虚拟串口 -> termios -> FrameParser -> SensorData -> JSON -> MQTT

通过标准：

- 两段串口数据组成一个完整帧。
- 只生成一条传感器 JSON。
- MQTT 发布成功。
- 无 CRC 或长度错误。

双串口并发测试命令：`./scripts/run_multi_serial_smoke_test.sh`

该测试创建两对独立 PTY，将设备 `0x10` 和 `0x11` 的合法帧并发写入两个串口，并验证 MQTT 同时收到对应的 `/16/data` 与 `/17/data` Topic。

## 7. 阶段验收标准

- 28 个 CTest 全部通过。
- 串口到 MQTT smoke test 通过。
- 双串口并发接入 smoke test 通过，且两路数据均能独立上报。
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

## 11. ARM64 交叉构建测试

### ARM-01：依赖 sysroot 隔离

执行 `./scripts/setup_aarch64_sysroot.sh`，确认 ARM64 依赖下载到独立目录，并生成 `gateway-sysroot.manifest`。脚本不得修改宿主机已安装的软件包架构。

### ARM-02：ELF 架构检查

执行 `./scripts/build_aarch64.sh`，预期 `file` 输出包含 `ARM aarch64`，`readelf -h` 的 Machine 为 `AArch64`，且动态依赖来自 ARM64 sysroot。

### ARM-03：QEMU 启动检查

构建脚本使用 `qemu-aarch64` 启动交叉编译产物，并传入不存在的配置文件。预期程序成功进入配置加载逻辑，输出 `load config failed:` 并以状态码 `1` 退出。

### ARM-04：部署包检查

预期生成 `linux-iot-edge-gateway-<version>-aarch64.tar.gz` 和对应 `.sha256` 文件，包内包含网关程序、版本文件、生产 YAML 配置和 systemd 服务单元。

### ARM-05：版本与校验文件

执行 `./scripts/build_aarch64.sh` 后，部署包文件名必须包含 `VERSION` 中的版本号。执行 `sha256sum -c` 应返回成功，包内的 `/usr/local/share/linux-iot-edge-gateway/VERSION` 应与程序 `--version` 输出一致。
