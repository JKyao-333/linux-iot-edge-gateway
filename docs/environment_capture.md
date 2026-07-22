# 环境采集

## 1. 采集目的

构建、串口、网络和运行时依赖差异会直接影响复现结果。`scripts/collect_env.sh` 将常用环境信息写成 Markdown，用于测试附件和公开复现记录，并为公开复现证据台账提供按次环境摘要。

## 2. 使用方法

默认采集：

```bash
./scripts/collect_env.sh
```

完整示例：

```bash
./scripts/collect_env.sh \
    --include-git \
    --include-packages \
    --include-serial \
    --include-network \
    --include-hash ~/linux-iot-edge-gateway-build/edge_gateway \
    --output artifacts/env_report.md
```

参数说明：

| 参数 | 作用 |
| --- | --- |
| `--output <path>` | 指定 Markdown 输出路径 |
| `--include-serial` | 统计串口设备类型和数量 |
| `--include-network` | 采集脱敏后的本地地址摘要 |
| `--include-git` | 增加分支和脱敏远端信息 |
| `--include-packages` | 记录关键 Debian/Ubuntu 软件包版本 |
| `--include-hash <file>` | 计算文件 SHA-256，可重复指定 |

单个工具不存在时报告中记录 `not found`，采集流程继续。只有输出目录无法创建或文件不可写时返回非零状态。

## 3. 输出字段

报告包括 Summary、Git、OS、CPU / Memory、Toolchain、Runtime Dependencies、Serial Devices、Network、File Hashes 和 Notes / Redaction Policy。

默认字段覆盖内核与系统版本、CPU 摘要、内存、磁盘、编译器、CMake、Python、SQLite、Mosquitto 工具、`pkg-config`、`libmosquitto`、systemd、QEMU 和 AArch64 交叉编译器状态。

## 4. 脱敏规则

- 仓库路径和主目录分别替换为 `<repo-root>` 与 `<home>`。
- 当前账户名替换为 `<redacted-user>`。
- 串口设备默认只记录类型和数量，不记录 `serial/by-id` 序列号。
- IPv4 地址保留前两段并将主机部分替换为 `x.x`；IPv6 统一标记为 `<redacted-ipv6>`。
- 不采集 Wi-Fi 名称、MAC 地址、公网 IP、口令、Token 或证书私钥。

## 5. 与硬件验证配套使用

- OpenM3 与 Raspberry Pi 4 复现：建议开启 Git、软件包、串口和网关二进制哈希采集。
- 其他 ARM64 开发板：通过稳定性测试的 `--hardware-label` 和环境摘要记录板卡类型、系统版本与测试条件；只有经过复核和脱敏的字段进入公开证据台账。
- STM32 / ARM64 公开复现：环境报告记录软件侧运行条件，硬件平台字段与验证结论以公开复现证据台账列明的范围为准。

## 6. 工程边界

`env_report.md` 是当前公开复现证据链的按次环境附件，不单独支撑长期稳定性或硬件电气层结论。报告默认保存在被 Git 忽略的 `artifacts/` 目录中，复核并脱敏后的字段可转写到公开复现证据台账。
