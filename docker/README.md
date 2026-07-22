# Docker Compose 本地复现

该环境用于在一台开发机上复现 MQTT、TCP、HTTP 可观测性与网关数据链路。Compose 启动以下服务：

- `mosquitto`：MQTT Broker，映射宿主机端口 `1883`；
- `tcp-server`：Python JSON Lines 接收端，映射端口 `9000`；
- `edge-gateway`：从当前仓库构建，映射 HTTP 端口 `8080`；
- 网关容器内部使用 `socat` 和现有 Python 模拟器生成串口输入。

## 启动与检查

```bash
mkdir -p artifacts/docker
docker compose up --build
```

另开终端检查：

```bash
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/ready
curl http://127.0.0.1:8080/metrics
mosquitto_sub -h 127.0.0.1 -p 1883 -t 'sensor/+/data' -v
```

停止并清理容器：

```bash
docker compose down
```

运行日志与 SQLite 缓存默认写入 `artifacts/docker/`，该目录中的运行产物不会提交到仓库。

## 串口边界

Docker 模式中的 PTY 和 Python 数据源只用于协议、发布链路和可观测性复现，不等同于真实 UART 电气层验证。真实 STM32/OpenM3 串口验证继续使用宿主机部署与现有硬件验证流程。
