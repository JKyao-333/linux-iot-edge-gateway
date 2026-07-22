# HTTP 可观测性

## 1. 目的

可选 HTTP 服务用于查看 `edge_gateway` 用户态进程的存活、就绪状态和内部计数。默认关闭，只在配置显式启用时监听指定地址和端口。

## 2. 配置

```yaml
http:
  enabled: true
  host: 127.0.0.1
  port: 8080
```

生产或实验网部署时应根据网络边界选择监听地址，并由主机防火墙控制访问范围。默认配置使用回环地址，不对外网暴露。

## 3. Endpoint

`GET /health` 在进程可服务时返回 HTTP 200：

```json
{"status":"ok","version":"1.0.1","uptime_seconds":12,"serial_workers":1,"input_devices_total":3,"device_online":3,"device_offline":0,"mqtt_enabled":true,"tcp_enabled":true,"cache_backend":"sqlite","devices":[]}
```

`GET /ready` 返回配置、缓存和设备管理线程初始化状态。全部就绪时为 HTTP 200，否则为 HTTP 503：

```json
{"ready":true,"checks":{"config_loaded":true,"cache_ready":true,"serial_workers_started":true},"serial_worker_count":1,"input_devices_total":3,"mqtt_enabled":true,"tcp_enabled":true,"cache_backend":"sqlite"}
```

`GET /metrics` 返回 `text/plain; version=0.0.4` 格式的 Prometheus 文本。未知路径返回 HTTP 404。

```text
# HELP iot_gateway_uptime_seconds Seconds since the gateway process initialized runtime metrics.
# TYPE iot_gateway_uptime_seconds gauge
iot_gateway_uptime_seconds 12
# HELP iot_gateway_frames_parsed_total Protocol frames accepted by the frame parser.
# TYPE iot_gateway_frames_parsed_total counter
iot_gateway_frames_parsed_total 3
```

## 4. 指标

| 指标 | 类型 | 当前数据来源 |
| --- | --- | --- |
| `iot_gateway_uptime_seconds` | gauge | 当前进程单调时钟运行时间 |
| `iot_gateway_serial_worker_count` | gauge | 已配置的 UART 输入设备数，不包含 Modbus 与 SocketCAN |
| `iot_gateway_frames_parsed_total` | counter | 成功解析并进入业务处理的协议帧 |
| `iot_gateway_frames_invalid_total` | counter | CRC、长度或字段映射校验拒绝的协议帧 |
| `iot_gateway_crc_errors_total` | counter | 解析器 CRC16 校验失败计数 |
| `iot_gateway_length_errors_total` | counter | 解析器非法长度计数 |
| `iot_gateway_mqtt_publish_success_total` | counter | MQTT 发布成功结果 |
| `iot_gateway_mqtt_publish_failed_total` | counter | MQTT 发布失败结果，包括补传尝试 |
| `iot_gateway_tcp_publish_success_total` | counter | TCP JSON Lines 发布成功结果 |
| `iot_gateway_tcp_publish_failed_total` | counter | TCP 发布失败结果 |
| `iot_gateway_cache_depth` | gauge | 进程当前观测到的待补传消息数 |
| `iot_gateway_cache_enqueue_total` | counter | MQTT 发布失败后成功写入缓存的消息数 |
| `iot_gateway_cache_flush_attempt_total` | counter | 缓存补传发布尝试次数 |
| `gateway_device_online_total` | gauge | 当前聚合为在线的输入设备数 |
| `gateway_device_offline_total` | gauge | 当前聚合为离线的输入设备数 |
| `gateway_protocol_error_total` | counter | UART、Modbus 与 SocketCAN 输入协议错误总数 |
| `gateway_modbus_error_total` | counter | Modbus 超时、异常响应、校验或传输错误数 |
| `gateway_can_error_total` | counter | SocketCAN 帧校验或传输错误数 |

`serial_workers`、`serial_worker_count` 和 `iot_gateway_serial_worker_count` 均只表示 UART 输入数量；`input_devices_total` 表示 UART、Modbus 与 SocketCAN 的配置设备总数。`serial_workers_started` 是设备管理线程已经完成初始化的就绪检查字段，即使采用 Modbus-only 或 CAN-only 配置也可以为 `true`。

这些指标均来自当前 `edge_gateway` 进程实际执行路径，进程重启后计数重新开始，不代表跨进程累计值，也不包含吞吐量、分位延迟或系统级硬件指标。

## 5. 本地验证

```bash
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/ready
curl http://127.0.0.1:8080/metrics
./scripts/run_observability_test.sh
```

Raspberry Pi 4 或 systemd 部署时，可让服务仍监听 `127.0.0.1`，由本机巡检脚本访问；需要远程采集时再显式配置受控网卡地址。

## 6. 工程边界

该接口用于用户态状态观察，不是产品级健康管理系统。指标仅反映当前进程内可采集的计数和状态，不声明固定吞吐量、分位延迟、长期可用性或工业现场可靠性。
