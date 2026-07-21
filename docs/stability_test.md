# 稳定性测试流程

## 1. 测试目的

`scripts/run_stability_test.sh` 将持续运行观察转换为可执行流程。它周期采集网关进程、资源、日志、缓存和上报计数，并生成 Markdown 摘要与 CSV 样本，便于比较不同硬件和软件版本的异常趋势。

## 2. 虚拟串口模式

未指定 `--serial` 时，脚本使用 socat 创建 PTY 对，通过 Python 模拟器按配置频率持续发送合法帧。

```bash
./scripts/run_stability_test.sh \
    --duration-seconds 300 \
    --rate-hz 1 \
    --collect-env
```

默认硬件标签为 `virtual-socat`。该模式适用于 Ubuntu、WSL2、CI 前置检查和无硬件回归。

## 3. 真实硬件模式

使用 `--serial` 指定 OpenM3、STM32 下位机或其他兼容串口设备。此时脚本不启动 Python 串口模拟器，数据频率由下位机固件控制；`--rate-hz` 仅作为测试配置记录，不能替代实测发送计数。

```bash
./scripts/run_stability_test.sh \
    --duration-seconds 1800 \
    --rate-hz 1 \
    --serial /dev/ttyUSB0 \
    --hardware-label stm32-arm64-board \
    --collect-env
```

## 4. OpenM3 与 Raspberry Pi 4 复现模式

在 Raspberry Pi 4 Model B 64-bit Linux 上连接 OpenM3 兼容 STM32 节点后，可运行：

```bash
./scripts/run_stability_test.sh \
    --duration-seconds 1800 \
    --rate-hz 1 \
    --serial /dev/ttyUSB0 \
    --hardware-label openm3-rpi4 \
    --collect-env \
    --with-tcp
```

执行前应确认配置文件中的 MQTT Broker 可访问，且 TCP 端口未被其他进程占用。

## 5. 参数说明

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `--duration-seconds <N>` | `300` | 持续观察秒数 |
| `--rate-hz <N>` | `1` | 虚拟模拟器发送频率 |
| `--serial <device>` | 未指定 | 指定真实串口；未指定时创建虚拟串口 |
| `--config <path>` | `config/gateway.yaml` | 网关基础配置 |
| `--build-dir <path>` | `~/linux-iot-edge-gateway-build` | CMake 构建目录 |
| `--mqtt-topic <topic>` | `sensor/+/data` | MQTT 订阅过滤器 |
| `--with-tcp` | 关闭 | 启动并验证 Python TCP Server |
| `--output-dir <path>` | `artifacts/stability_<timestamp>` | 本次输出目录 |
| `--collect-env` | 关闭 | 调用 `collect_env.sh` |
| `--no-build` | 关闭 | 跳过 CMake 配置和编译 |
| `--hardware-label <label>` | 按串口模式推导 | 记录硬件或数据源标签 |

## 6. 输出文件

- `stability_summary.md`：测试配置、计数、异常情况、判定和边界。
- `stability_samples.csv`：周期 CPU、RSS、文件描述符、线程、日志、缓存和消息计数。
- `gateway.log`：网关结构化运行日志。
- `gateway_console.log`：进程标准输出和标准错误。
- `mqtt_received.log`：订阅端收到的消息。
- `tcp_received.log`：启用 TCP 时服务端收到的 JSON Lines。
- `simulator.log`：虚拟串口模式下的发送记录。
- `env_report.md`：使用 `--collect-env` 时生成的脱敏环境报告。

## 7. 判定规则

PASS 要求网关在观察周期内未异常退出、至少收到一条 MQTT 消息、无未处理崩溃；启用 TCP 时还要求至少收到一条 TCP 消息。未注入 Broker 故障时，SQLite 队列不应在结束时仍有积压。

FAIL 表示上述连续性条件至少一项不满足。指标因平台权限或命令缺失而无法采集，但核心连续性条件未失败时，结果标记为 INCONCLUSIVE。脚本返回码分别为 PASS `0`、FAIL `1`、INCONCLUSIVE `2`。

## 8. 工程边界

- 该脚本用于持续运行和异常趋势观察，不是性能基准测试。
- 不声明固定吞吐量。
- 不声明 P99 时延。
- 不声明 7x24 小时生产可用性。
- 正式性能评估需要独立设计工作负载、计时方法、资源采样和统计口径。
- 虚拟串口结果不替代真实硬件长期稳定性测试。
