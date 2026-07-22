# 变更记录

本项目遵循语义化版本规则。

## Unreleased

### 多协议设备接入

- 新增统一 `DeviceInterface` 与 `DeviceManager`，将 UART、Modbus RTU 和 SocketCAN 输入转换为公共 `SensorData`。
- 新增 Modbus RTU Master，支持功能码 03/04、CRC、响应超时、异常响应和长度校验。
- 新增 Linux SocketCAN 输入、CAN ID/DLC/Payload 解析和 `vcan0` 集成测试路径。
- HTTP Health 与 Prometheus Metrics 增加设备在线、离线及协议错误状态。
- 新增三协议集成测试脚本和 Modbus、SocketCAN 工程文档。
- 移除主程序中已由 `UartDevice` 接管的旧串口工作循环，保持单一 UART 接入路径。
- 修复 UART 同批次错误帧与合法帧并存时协议错误统计丢失的问题。
- 配置校验支持 UART-only、Modbus-only、CAN-only 和混合输入，并拒绝未启用任何输入源的配置。
- HTTP 状态增加输入设备总数，并明确串口工作线程字段只统计 UART 输入。

### 可观测性与本地复现

- 新增可选 HTTP Health、Ready 与 Metrics endpoint。
- 新增 Prometheus 文本格式运行指标导出，并接入协议、发布和缓存实际执行路径。
- 新增 Docker Compose 本地复现环境。
- 新增 HTTP observability 自动化测试脚本。
- Health Check 支持可选 HTTP 与 Metrics URL 检查。

### 工程验证

- 新增 GitHub Actions CI workflow，覆盖 CMake 构建、CTest、smoke test、HTTP observability test、轻量验证工作流、证据摘要生成和 Docker Compose 配置检查。
- README 增加 CI 状态 badge。
- 文档补充 GitHub Actions 与本地 `vcan0` 协议集成测试的验证边界。
- 新增公开复现证据台账，统一记录公开复现平台、运行环境、协议统计和原始记录归档边界。
- 统一公开文档措辞，明确当前公开复现证据范围与本地材料归档边界。
- 新增本地复现证据摘要采集脚本，用于归档 Linux kernel、TTY、测试日期和 artifacts 索引等运行信息。
- 新增故障注入测试套件，覆盖异常协议帧、MQTT/TCP 离线、SQLite 补传、串口重连和 SIGTERM 退出。
- 新增脱敏环境采集脚本，用于记录软件工具链、运行时依赖、设备摘要和文件哈希。
- 新增可执行稳定性测试流程，周期采集进程资源、日志、缓存和消息计数并生成归档摘要。
- 新增故障注入、环境采集和稳定性测试文档。
- 新增串口 Replay 工具与脱敏协议测试帧。
- 新增 Health Check 状态检查脚本。
- 新增日志脱敏脚本。
- 新增串口 replay、health check 和 logging 工程文档。
- 优化日志脱敏脚本 dry-run 参数体验。
- 新增工程验证工作流文档与轻量自检脚本，串联 smoke test、故障注入、环境采集、稳定性测试、串口 replay、health check 和日志脱敏归档流程。

## 1.0.1 - 2026-07-21

### 文档

- 修正验证边界，记录已完成的 STM32 串口接入和 ARM64 Linux 板级稳定性验证。
- 新增 STM32 硬件接入验证、ARM64 稳定性测试、模块实现说明和技术参考资料。
- 删除与项目定位无关的说明，统一为课题组预研工程文档。
- 增加运行数据与日志目录的脱敏和提交规则。
- 记录 OpenM3 兼容节点和 Raspberry Pi 4 Model B 的公开核心链路复现结果。

### 构建

- ARM64 工具根目录改由 `AARCH64_TOOLS_ROOT` 控制，默认使用 `~/Tools/linux-iot-edge-gateway`。

### 仓库

- 新增 MIT License。
- 扩展 `.gitignore`，排除本地日志、缓存数据库、临时文件、凭据和证书材料。

## 1.0.0 - 2026-07-20

- 首个稳定版本，包含串口协议解析、MQTT/TCP 发布、SQLite 离线缓存、自动重连、日志、自动化测试和 ARM64 构建流程。
