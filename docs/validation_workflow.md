# 工程验证工作流

## 1. 验证层级

当前项目按以下层级组织工程验证：

1. 单元测试与 CTest；
2. smoke test；
3. 故障注入测试；
4. 环境采集；
5. 稳定性测试；
6. 串口 Replay；
7. Health Check；
8. 日志脱敏归档；
9. OpenM3 + Raspberry Pi 4 公开复现；
10. 公开复现证据摘要与台账。

这些层级分别覆盖代码级回归、用户态端到端链路、异常恢复、运行趋势、问题回放和硬件复现。各层结果互为补充，公开字段与边界统一见[公开复现证据台账](reproducible_evidence_ledger.md)。

## 2. 推荐执行顺序

```bash
./scripts/run_smoke_test.sh
./scripts/run_fault_injection_test.sh
./scripts/run_observability_test.sh
./scripts/collect_env.sh --include-git --include-packages --output artifacts/env_report.md
./scripts/collect_evidence_summary.sh
./scripts/run_stability_test.sh --duration-seconds 60 --rate-hz 1 --collect-env
python3 scripts/serial_replay.py --input data/test_frames/valid_frames.hex --serial /tmp/tty_dummy --dry-run
./scripts/check_gateway_health.sh --service linux-iot-edge-gateway --process-name edge_gateway --output artifacts/health_check.md || true
python3 scripts/sanitize_logs.py --input logs/sample_gateway.log --dry-run
```

也可以先运行轻量工具链检查：

```bash
./scripts/run_validation_workflow_check.sh
```

该脚本检查 replay dry-run、日志脱敏 dry-run、Health Check 和环境采集，不启动长时间测试，也不依赖真实硬件。Health Check 返回 `1` 或 `2` 可能仅表示目标服务或进程当前未运行，不会直接导致轻量检查失败。Replay dry-run 不打开串口，只验证测试帧格式。60 秒 stability 测试属于短跑验证，不代表长期稳定性结论。

## 3. 虚拟环境验证路径

无硬件环境可使用 CTest、smoke test、socat 虚拟串口、Python 模拟器、故障注入、稳定性测试和 replay dry-run：

```bash
./scripts/run_smoke_test.sh
./scripts/run_fault_injection_test.sh
./scripts/run_observability_test.sh
./scripts/run_stability_test.sh --duration-seconds 60 --rate-hz 1 --collect-env
python3 scripts/serial_replay.py --input data/test_frames/valid_frames.hex --serial /tmp/tty_dummy --dry-run
```

虚拟环境验证主要覆盖用户态协议解析、缓存、网络发布、异常恢复和进程生命周期，不覆盖真实串口电气层、供电和 EMC 条件。

## 4. 公开硬件复现路径

OpenM3 + Raspberry Pi 4 公开复现建议按以下步骤执行：

1. 在 Raspberry Pi 4 64-bit Linux 环境中部署 `edge_gateway`；
2. 连接 OpenM3 / STM32-compatible UART node；
3. 采集脱敏环境报告；
4. 使用真实串口运行短时稳定性测试；
5. 运行 Health Check；
6. 对日志进行脱敏归档。

```bash
./scripts/collect_env.sh \
    --include-git \
    --include-packages \
    --include-serial \
    --include-hash ~/linux-iot-edge-gateway-build/edge_gateway \
    --output artifacts/env_report.md

./scripts/run_stability_test.sh \
    --duration-seconds 1800 \
    --rate-hz 1 \
    --serial /dev/ttyUSB0 \
    --hardware-label openm3-rpi4 \
    --collect-env \
    --with-tcp

./scripts/check_gateway_health.sh \
    --service linux-iot-edge-gateway \
    --process-name edge_gateway \
    --serial /dev/ttyUSB0 \
    --mqtt-host localhost \
    --mqtt-port 1883 \
    --output artifacts/health_check.md

python3 scripts/sanitize_logs.py \
    --input artifacts/stability_<timestamp>/gateway.log \
    --output artifacts/stability_<timestamp>/gateway_sanitized.log
```

只记录实际运行得到的测试时长和结果。OpenM3 与 Raspberry Pi 4 是当前公开复现平台，其可公开字段与不声明字段由公开复现证据台账统一说明。

## 5. 结果归档建议

- `artifacts/` 默认不提交；
- 原始日志和本机环境报告仅在本地受控保存，默认不进入版本库；
- 脱敏后的摘要经复核后可以转写到公开工程文档或公开复现证据台账；
- 固件 commit、系统镜像摘要和长时间运行参数仅在存在可公开证据时记录，否则当前公开复现路径不声明；
- 不提交真实串口抓包、数据库、证书、账号、设备序列号、本机绝对路径或未脱敏日志；
- `data/test_frames/` 只保存合成测试帧，不保存现场真实采集数据。

## 6. 工程边界

- 验证工作流用于工程复现、异常趋势观察和用户态链路验证；
- 不代表工业现场 EMC、电源扰动或极端网络条件；
- 不声明固定吞吐量、P99 时延或 7x24 小时可用性；
- 不替代正式产品级可靠性认证；
- 不替代真实硬件长期测试的原始数据。

公开仓库使用公开复现证据台账记录可公开、已脱敏和可复现的证据字段。PCB revision、USB-UART 芯片、固定固件 commit、固定 TTY 路径、原始抓包和仪器记录不作为当前公开复现结论的必要依据。
