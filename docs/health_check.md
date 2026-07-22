# Health Check 状态检查

## 1. 工具目的

`scripts/check_gateway_health.sh` 为本地开发、Raspberry Pi 4、ARM64 开发板和 systemd 部署提供只读状态检查。脚本检查当前服务、进程、日志、缓存、串口和通信端点，不启动或修改被检查对象。

## 2. 本地开发模式

本地前台启动网关时，systemd 服务可能未启用。可以检查进程、Broker、日志和缓存：

```bash
./scripts/check_gateway_health.sh \
    --process-name edge_gateway \
    --log artifacts/local_run/gateway.log \
    --cache-db artifacts/local_run/pending_messages.db \
    --mqtt-host localhost \
    --mqtt-port 1883 \
    --output artifacts/health_check.md
```

未提供的日志、缓存、串口和 TCP 项目会显示 `SKIP`。

## 3. systemd 部署模式

```bash
./scripts/check_gateway_health.sh \
    --service linux-iot-edge-gateway \
    --process-name edge_gateway \
    --log /var/log/linux-iot-edge-gateway/gateway.log \
    --cache-db /var/lib/linux-iot-edge-gateway/pending_messages.db \
    --serial /dev/ttyUSB0
```

脚本不会输出完整本机路径；仓库和 home 路径会替换为占位符，其他文件只显示 basename。

## 4. OpenM3 与 Raspberry Pi 4 复现模式

在 Raspberry Pi 4 上启动网关和 Mosquitto 后，可将 OpenM3 对应串口作为检查项：

```bash
./scripts/check_gateway_health.sh \
    --service linux-iot-edge-gateway \
    --serial /dev/ttyUSB0 \
    --mqtt-host localhost \
    --mqtt-port 1883 \
    --tcp-host localhost \
    --tcp-port 9000
```

如果使用 `/dev/serial/by-id/`，报告只保留 `/dev/serial/by-id/<redacted-serial>`。

## 5. Markdown 输出示例

```text
| Check | Status | Detail |
| --- | --- | --- |
| systemd available | PASS | systemd is active |
| Gateway service | PASS | linux-iot-edge-gateway is active |
| MQTT endpoint | PASS | localhost:1883 is reachable |
| TCP endpoint | SKIP | TCP endpoint not provided |
```

## 6. JSON 输出示例

```bash
./scripts/check_gateway_health.sh \
    --json \
    --output artifacts/health_check.json
```

```json
{
  "collected_at": "2026-01-01T00:00:00+08:00",
  "git_commit": "<commit>",
  "overall": "WARN",
  "checks": [
    {
      "id": "log",
      "label": "Recent log levels",
      "status": "SKIP",
      "detail": "log path not provided"
    }
  ]
}
```

## 7. 状态与退出码

| 状态 | 含义 |
| --- | --- |
| `PASS` | 检查成功且当前状态正常 |
| `WARN` | 信息不完整或存在可恢复、需关注的状态 |
| `FAIL` | 明确指定的服务、进程、设备、数据库或端点异常 |
| `SKIP` | 未提供可选检查对象，或其前置环境不可用 |

| 退出码 | 含义 |
| ---: | --- |
| `0` | 全部为 PASS 或 SKIP |
| `1` | 存在 WARN，但没有 FAIL |
| `2` | 至少存在一项 FAIL |
| `3` | 参数错误、报告无法写入或脚本运行错误 |

## 8. 工程边界

Health Check 只描述采集时刻的用户态状态，不代表长期稳定性，也不替代公开复现证据台账中的故障注入、持续运行测试或现场电气检查。
