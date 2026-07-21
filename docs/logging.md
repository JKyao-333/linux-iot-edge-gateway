# 日志规范与脱敏

## 1. 日志等级

| 等级 | 用途 |
| --- | --- |
| `DEBUG` | 开发调试、协议状态和细粒度诊断 |
| `INFO` | 启停、连接建立、恢复和发布成功等关键状态变化 |
| `WARN` | CRC 错误、非法长度、连接中断等可恢复异常 |
| `ERROR` | 无法恢复或需要人工介入的错误 |

## 2. 建议字段

结构化日志或后续日志接口应优先包含：

- `timestamp`
- `level`
- `module`
- `event`
- `device_id`
- `sequence`
- `error_code` 或 `reason`

并非每条日志都需要全部字段。连接级事件通常不包含设备数据，协议和发布事件则应保留足够的关联字段。

## 3. 当前典型事件

当前用户态网关已经记录以下事件或等价信息：

- `serial opened`
- `connection lost`
- `serial connection restored`
- `CRC validation failed`
- `invalid payload length`
- `parsed data`
- `publish result`
- `MQTT connected` / `MQTT connection lost`
- `cache enqueue` / `cache replay` / `cache delete`
- `shutdown requested`
- `edge gateway stopped`

## 4. 使用建议

- DEBUG 只在开发和短期诊断时启用，避免长期产生大量原始输出。
- INFO 用于可以描述生命周期和链路状态的关键事件。
- WARN 表示网关能够继续运行，但应保留原因和恢复结果。
- ERROR 用于操作失败、状态损坏或需要人工处理的问题。
- 日志分析应结合配置、测试时间和设备台账，不根据单条日志推导长期可靠性结论。

## 5. 脱敏策略

原始运行日志、故障注入 raw log、稳定性日志和本机环境报告默认位于被 Git 忽略的 `artifacts/` 或 `logs/` 运行目录中，不直接提交。

公开摘要前使用：

```bash
python3 scripts/sanitize_logs.py \
    --input artifacts/fault_injection_raw.log \
    --output artifacts/fault_injection_sanitized.log
```

默认处理以下内容：

- IPv4 和 IPv6 地址
- 仓库、home、Windows 用户目录和 WSL 用户目录
- `/dev/serial/by-id/` 设备序列号
- 当前账户名
- password、token、private_key、certificate 和 username 等字段值

对完整 JSON Lines，脚本先解析 JSON 并递归脱敏，输出仍保持逐行合法 JSON。`--append-note` 会增加普通文本注释，因此严格 JSON Lines 归档不应开启该参数。

如需检查而不写文件：

```bash
python3 scripts/sanitize_logs.py \
    --input logs/sample_gateway.log \
    --output artifacts/sample_sanitized.log \
    --dry-run
```

## 6. 工程边界

当前日志覆盖 Linux 用户态网关的协议、缓存、网络发布和进程生命周期，不覆盖内核驱动、电源、电气层、工业现场完整观测或独立性能测量。
