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
-> FileCache 本地缓存
-> 周期性重试
-> Broker 恢复后补传
-> 删除已成功发布的缓存

## 3. 模块结构

### 3.1 main.cpp

应用入口和主控制循环，负责：

- 读取串口设备参数
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
- `src/protocol/frame_parser.h`
- `src/protocol/frame_parser.cpp`

职责：

- 定义协议帧结构
- 实现 CRC16-Modbus
- 缓存未完成字节
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

MqttClient 负责调用本机 `mosquitto_pub` 发布消息。

ReliablePublisher 负责：

- 优先尝试 MQTT 发布
- 发布失败时写入本地缓存
- Broker 恢复后按顺序补传
- 保留尚未发布成功的消息

### 3.5 cache

文件：

- `src/cache/file_cache.h`
- `src/cache/file_cache.cpp`

缓存格式为每行一条消息：

`topic<TAB>payload`

缓存更新采用临时文件加重命名的方式，避免直接覆盖原文件造成数据损坏。

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

## 4. 运行模型

当前网关采用单进程、单线程同步循环。

串口配置：

- 波特率：115200
- 数据位：8
- 校验位：无
- 停止位：1
- 流控：无
- 原始模式：raw

主要时间参数：

- 串口读取超时约 1 秒
- 串口重连间隔 2 秒
- MQTT 缓存补传检查间隔 5 秒

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
- Broker 恢复后自动补传
- 成功消息从缓存中删除
- 未成功消息继续保留

### 可观测性

- 记录启动、串口连接和恢复
- 记录 CRC 与长度错误
- 记录异常传感器数据
- 记录 MQTT 发布和缓存补传结果

## 6. 测试架构

测试分为三层：

1. C++ 单元测试：验证 CRC、解析器、业务数据、缓存和日志。
2. MQTT 集成测试：验证发布和缓存恢复。
3. 串口端到端测试：验证 Python、PTY、网关和 MQTT 完整链路。

统一入口：

`./scripts/run_smoke_test.sh`

## 7. 当前边界与后续扩展

当前版本使用 `mosquitto_pub` 子进程完成 MQTT 发布，便于快速验证链路。

后续可扩展：

- 接入 Eclipse Paho 或 libmosquitto
- 抽象 Publisher 接口
- 增加原生 TCP 上报实现
- 增加 YAML 配置加载
- 将文件缓存升级为 SQLite
- 增加多串口和多设备并发
- 增加 systemd 服务和交叉编译支持
