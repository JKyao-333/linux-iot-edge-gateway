# 系统架构说明

## 1. 项目定位

本项目是 STM32 多组分气体监测系统的 Linux 边缘侧扩展模块。

STM32 或 Python 模拟器负责采集并发送传感器数据，Linux C++ 网关负责协议解析、数据清洗、JSON 封装、MQTT 上报、离线缓存、恢复补传和日志记录。

## 2. 数据链路

数据处理链路如下：

STM32 / Python 模拟器
-> Linux 虚拟串口或物理串口
-> termios 串口接收
-> FrameParser 流式协议解析
-> CRC16 校验
-> SensorData 字段解析
-> 数据范围检查
-> JSON 封装
-> ReliablePublisher
-> MQTT Broker

MQTT 发布失败时：

ReliablePublisher
-> MessageCache 统一接口
-> SqliteCache 持久化队列
-> 周期性重试
-> Broker 恢复后补传
-> 删除已成功发布的缓存

## 3. 模块结构

### 3.1 main.cpp

应用入口和主控制循环，负责：
- 加载 YAML 配置并支持命令行覆盖串口设备
- 为每个配置串口启动独立工作线程和独立 FrameParser
- 打开和配置串口
- 串口断开检测与自动重连
- 将字节流送入 FrameParser
- 输出协议诊断日志
- 调用传感器数据解析
- 调用 ReliablePublisher 上报
- 周期性触发缓存补传

### 3.2 protocol

文件：

- `src/protocol/frame.h`
- `src/protocol/crc16.h`
- `src/protocol/crc16.cpp`
- `src/protocol/byte_ring_buffer.h`
- `src/protocol/frame_parser.h`
- `src/protocol/frame_parser.cpp`

职责：

- 定义协议帧结构
- 实现 CRC16-Modbus
- 使用 256 字节固定容量环形缓冲区缓存未完成字节
- 通过移动读指针消费数据，避免线性缓冲区反复 `erase` 和内存搬移
- 处理半包和粘包
- 过滤 CRC 错误帧
- 检测非法 Payload 长度
- 从异常数据中恢复同步

### 3.3 app

文件：

- `src/app/sensor_data.h`
- `src/app/sensor_data.cpp`

职责：

- 定义 SensorData
- 将 Payload 转换为业务字段
- 处理大小端和缩放系数
- 检查温度、湿度、气体浓度和电池电压
- 解析设备状态位
- 生成 JSON 字符串

### 3.4 mqtt

文件：

- `src/mqtt/mqtt_client.h`
- `src/mqtt/mqtt_client.cpp`
- `src/mqtt/reliable_publisher.h`
- `src/mqtt/reliable_publisher.cpp`

MqttClient 基于 `libmosquitto` 维护原生 MQTT 长连接，使用独立网络循环处理连接、断开和发布回调。消息采用 QoS 1 发布，收到 Broker 的 PUBACK 后才返回成功，并在连接断开后自动重连。客户端支持用户名/密码认证和 TLS：使用 CA 文件验证 Broker 证书及主机名，也可同时配置客户端证书和私钥启用双向 TLS。

ReliablePublisher 负责：

- 优先尝试 MQTT 发布
- 发布失败时写入本地缓存
- Broker 恢复后按顺序补传
- 保留尚未发布成功的消息

### 3.4.1 publisher 抽象

文件：

- `src/publisher/publisher.h`
- `src/publisher/publisher_group.h`
- `src/publisher/publisher_group.cpp`
- `src/publisher/tcp_publisher.h`
- `src/publisher/tcp_publisher.cpp`

`Publisher` 定义统一发布接口，并使用 `Published`、`Deferred` 和 `Failed` 表示发布成功、延后处理和彻底失败。MQTT 的 `ReliablePublisher` 与 TCP 的 `TcpPublisher` 都实现该接口。

`PublisherGroup` 保存当前启用的发布通道，将同一条 Topic 和 JSON 依次交给每个通道，并汇总每个通道的独立结果。一个通道返回失败不会中断后续通道，因此 MQTT 与 TCP 可以独立退化。MQTT 的 `Deferred` 表示消息已经进入本地缓存，仍由 `ReliablePublisher` 周期补传。

### 3.5 cache

文件：

- `src/cache/message_cache.h`
- `src/cache/file_cache.h`
- `src/cache/file_cache.cpp`
- `src/cache/sqlite_cache.h`
- `src/cache/sqlite_cache.cpp`

`MessageCache` 定义追加、FIFO 读取和队首删除接口，使 ReliablePublisher 不依赖具体存储后端。

默认 `SqliteCache` 使用自增 ID 保持消息顺序，表字段包含 Topic、JSON Payload 和创建时间。数据库启用 WAL、`synchronous=FULL`、忙等待和预编译 SQL，既避免手工转义问题，也降低异常掉电造成数据库损坏的风险。

兼容 `FileCache` 仍支持每行一条消息的旧格式：

`topic<TAB>payload`

文件缓存更新采用临时文件加重命名的方式，避免直接覆盖原文件造成数据损坏。旧文件可通过 `scripts/migrate_file_cache.py` 一次性迁移到 SQLite。

### 3.6 log

文件：

- `src/log/logger.h`
- `src/log/logger.cpp`

日志级别：

- DEBUG
- INFO
- WARN
- ERROR

日志同时输出到终端和 `logs/gateway.log`，每条记录包含时间、级别、模块和消息。

### 3.7 config

文件：

- `src/config/gateway_config.h`
- `src/config/gateway_config.cpp`
- `config/gateway.yaml`

职责：

- 使用 yaml-cpp 解析 YAML 配置文件
- 将配置转换为强类型 `GatewayConfig` 结构体
- 为各配置项提供默认值
- 检查串口波特率、网络端口和时间间隔
- 检查串口设备列表非空、路径非空且不存在重复项
- 检查日志级别和必要字符串
- 配置无效时阻止网关启动并输出错误原因

`main.cpp` 只负责使用已经校验通过的配置，避免在业务流程中散落硬编码参数。
## 4. 运行模型

当前网关采用单进程、多线程运行模型。主线程创建并共享 MQTT、TCP 和缓存发布组件，负责周期性补传缓存及进程退出协调；每个串口由一个独立工作线程负责打开、读取、解析和断线重连。每个工作线程拥有自己的文件描述符、`FrameParser` 和环形缓冲区，因此一路串口断线不会阻塞其他设备的数据接入。共享发布通道使用互斥锁保护。

串口配置：

- 波特率：115200
- 数据位：8
- 校验位：无
- 停止位：1
- 流控：无
- 原始模式：raw

主要时间参数：
主要运行参数由 `config/gateway.yaml` 提供，默认值包括：

- 串口波特率 115200
- 串口重连间隔 2 秒
- MQTT 缓存补传检查间隔 5 秒
- MQTT Broker 端口 1883
- TCP Server 端口 9000
- TCP 上报默认启用
该模型结构简单，便于在嵌入式 Linux 设备上部署和调试。

## 5. 可靠性设计

### 串口可靠性

- 使用流式缓存处理半包
- 循环解析处理粘包
- CRC16 过滤传输错误
- 限制最大 Payload 长度
- 串口断开后关闭失效文件描述符
- 清空残留半帧并周期性重连

### MQTT 可靠性

- 发布失败时消息落盘
- 缓存保留 Topic 和 JSON Payload
- SQLite 按自增 ID 保持 FIFO 顺序
- Broker 恢复后自动补传，收到 PUBACK 后删除队首记录
- 未成功消息继续保留
- 异常退出时采用至少一次投递语义，可能重复但避免主动丢失

### MQTT 安全性

- 认证与 TLS 参数由 YAML 配置加载，不在业务代码中硬编码
- 日志只记录认证和 TLS 是否启用，不输出用户名、密码或私钥内容
- TLS 默认验证 CA 信任链和 Broker 主机名
- 客户端证书和私钥必须成对配置
- `mqtt.tls.insecure` 只用于本地证书调试，生产环境应保持 `false`

### 可观测性

- 记录启动、串口连接和恢复
- 记录 CRC 与长度错误
- 记录异常传感器数据
- 记录 MQTT 发布和缓存补传结果

### 进程生命周期可靠性

- `shutdown_signal` 使用 `sigaction()` 注册 SIGINT 和 SIGTERM
- 信号处理函数只修改 `sig_atomic_t` 标志，不执行日志或资源释放
- 串口首次打开、阻塞读取和断线重连循环都检查退出标志
- 退出时关闭串口和 TCP 连接，网络客户端通过析构函数释放
- systemd 使用 `Restart=on-failure` 拉起异常退出进程

## 6. 测试架构

测试分为五层：

1. C++ 单元测试：验证 CRC、解析器、业务数据、缓存和日志。
2. MQTT 集成测试：验证发布、缓存恢复、用户名认证和 TLS 握手。
3. 串口端到端测试：验证 Python、PTY、网关和 MQTT 完整链路，包括两个串口并发接入。
4. 进程生命周期测试：验证信号捕获和 SIGTERM 优雅退出。
5. 跨平台构建测试：验证 ARM64 依赖 sysroot、AArch64 ELF 和 QEMU 启动。

统一入口：

`./scripts/run_smoke_test.sh`

## 7. 当前边界与后续扩展

当前版本已经使用 `libmosquitto` 实现原生 MQTT 长连接、QoS 1 发布确认、自动重连、用户名认证和 TLS 加密。ReliablePublisher 通过 `MessageCache` 接口访问缓存，默认使用 SQLite 持久化队列，并在 Broker 恢复后按顺序补传。

ARM64 版本通过 GNU AArch64 交叉编译器构建。构建脚本从 Ubuntu Ports 下载 ARM64 版本的 yaml-cpp、SQLite 和 Mosquitto 依赖，解包为独立 sysroot，避免修改开发机原有的软件包架构配置。CMake 工具链文件限制头文件和库搜索范围，防止错误链接 x86_64 依赖。

构建完成后依次执行三项验证：

1. `file` 检查产物为 ARM aarch64 ELF。
2. `readelf` 检查 ELF Machine 和动态依赖。
3. `qemu-aarch64` 启动程序并验证配置错误退出路径。

验证通过后，将程序、生产配置和 systemd 服务单元打包为可部署的 `tar.gz`。

项目根目录的 `VERSION` 是唯一版本源。CMake 将版本写入程序，ARM64 构建脚本生成带版本号的部署包和 SHA-256 校验文件。推送与版本一致的 `vMAJOR.MINOR.PATCH` 标签后，GitHub Actions 会创建 Release，并为部署包生成构建来源证明。
## 8. 原生 TCP 上报

### 8.1 模块文件

- `src/tcp/tcp_client.h`
- `src/tcp/tcp_client.cpp`

`TcpClient` 基于 Linux POSIX Socket API 实现，负责：

- 使用 `getaddrinfo()`解析服务器地址
- 使用 `socket()` 创建 TCP Socket
- 使用 `connect()` 建立连接
- 使用 `send()` 循环发送完整数据
- 使用 `MSG_NOSIGNAL` 避免断线时触发 SIGPIPE
- 发送失败后关闭失效连接
- 后续发送时自动重新连接

### 8.2 消息边界

TCP 是字节流协议，不保留应用层消息边界。

本项目采用 JSON Lines 格式：

`一条 JSON + 一个换行符`

TCP 服务端持续缓存接收到的字节，并以换行符拆分完整 JSON，从而处理半包和粘包。

### 8.3 双通道数据链路

合法传感器数据经过清洗和 JSON 封装后，同时进入两条上报链路：

1. MQTT：发布到 `sensor/<device_id>/data`
2. TCP：发送到 `127.0.0.1:9000`

MQTT 和 TCP 通过统一 `Publisher` 接口注册到 `PublisherGroup`。发布组依次扇出同一条消息并返回逐通道结果，一条链路发布失败不会阻止另一条链路继续工作。

### 8.4 TCP 测试

- `tests/tcp_client_test.cpp`：C++ TCP 客户端测试
- `scripts/mock_tcp_server.py`：Python TCP 模拟服务端
- `scripts/run_tcp_smoke_test.sh`：TCP 自动化 smoke test

总测试入口 `scripts/run_smoke_test.sh` 会依次运行 CTest、优雅退出测试、串口到 MQTT 测试和 TCP 收发测试。

## 9. systemd 运行模型

生产部署使用 `deploy/systemd/linux-iot-edge-gateway.service`：

- 使用独立的 `iot-gateway` 低权限用户运行
- 通过 `dialout` 附加组访问串口设备
- 配置、缓存和日志分别位于 `/etc`、`/var/lib` 和 `/var/log`
- 使用 `ProtectSystem=strict`、`ProtectHome=true` 和 `NoNewPrivileges=true` 限制服务权限
- 进程异常退出后等待 3 秒自动重启
- `systemctl stop` 发送 SIGTERM，并等待网关完成清理

安装脚本会复制程序和生产配置、创建运行目录、注册并启动服务。卸载脚本默认保留配置和运行数据，只有显式传入 `--purge` 才删除这些数据。
