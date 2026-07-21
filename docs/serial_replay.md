# 串口 Replay 工具

## 1. 工具目的

`scripts/serial_replay.py` 从脱敏历史片段或合成测试文件读取十六进制字节，并按指定频率写入真实串口或 `socat` 虚拟串口。它用于复现协议解析问题、回归异常帧处理以及重放可公开的输入片段。

## 2. 输入格式

输入文件扩展名应为 `.hex` 或 `.txt`。每个非空数据行表示一次串口写入，字节使用两位十六进制形式并以空格分隔：

```text
# comment
AA 55 0B 01 10 00 FD 02 60 01 2C 0E 74 00 00 01 4D 58
```

空行和以 `#` 开头的注释行会被跳过。非法字节会报告源文件行号并终止回放。

仓库提供以下脱敏样例：

- `data/test_frames/valid_frames.hex`：合法传感器帧。
- `data/test_frames/fault_frames.hex`：CRC 错误、非法长度和前置噪声。
- `data/test_frames/stream_cases.hex`：半包和连续帧测试输入。

## 3. 参数

| 参数 | 说明 |
| --- | --- |
| `--input <path>` | 必填，输入 `.hex` 或 `.txt` 文件 |
| `--serial <device>` | 必填，目标串口设备 |
| `--baudrate <N>` | 波特率，默认 `115200` |
| `--rate-hz <N>` | 每秒回放帧数，默认 `1` |
| `--repeat <N>` | 重复次数，默认 `1`，`0` 表示持续回放 |
| `--split-bytes <N>` | 将每帧拆成最多 N 字节的写入块 |
| `--split-delay-ms <N>` | 分片写入间隔，默认 `50` 毫秒 |
| `--append-newline` | 在每条输入后附加换行，仅用于文本协议试验 |
| `--dry-run` | 只校验和显示输入，不打开串口 |
| `--summary <path>` | 摘要路径，默认 `artifacts/serial_replay_summary.md` |

摘要只记录串口 basename 或脱敏标记，不保存 `/dev/serial/by-id/` 中的设备序列号。

## 4. 虚拟串口 Replay

```bash
socat -d -d \
    pty,raw,echo=0,link=/tmp/tty_stm32 \
    pty,raw,echo=0,link=/tmp/tty_gateway

python3 scripts/serial_replay.py \
    --input data/test_frames/valid_frames.hex \
    --serial /tmp/tty_stm32 \
    --rate-hz 1
```

格式检查不需要创建串口：

```bash
python3 scripts/serial_replay.py \
    --input data/test_frames/valid_frames.hex \
    --serial /tmp/tty_dummy \
    --dry-run
```

## 5. 真实串口 Replay

确认串口未被其他进程占用并核对电气连接后执行：

```bash
python3 scripts/serial_replay.py \
    --input data/test_frames/valid_frames.hex \
    --serial /dev/ttyUSB0 \
    --baudrate 115200 \
    --rate-hz 1
```

真实设备验证前应将固件提交号、串口参数和测试时间登记到设备台账，公开摘要中不记录 by-id 序列号。

## 6. 半包模拟

以下命令将每帧按 6 字节分片，并在分片之间等待 50 毫秒：

```bash
python3 scripts/serial_replay.py \
    --input data/test_frames/stream_cases.hex \
    --serial /tmp/tty_stm32 \
    --split-bytes 6 \
    --split-delay-ms 50
```

提高 `--rate-hz` 并连续回放多行，可观察连续帧或接近粘包的读取行为。

## 7. 与其他测试的关系

- `run_fault_injection_test.sh` 负责一键编排 Broker、TCP 服务、虚拟串口和故障场景，并给出 PASS/FAIL 矩阵。
- `run_stability_test.sh` 负责持续运行、资源采样和消息计数。
- `serial_replay.py` 聚焦单个输入文件的确定性重放，可用于提取最小复现场景，也可作为上述流程的输入补充。

## 8. 工程边界

- Replay 是协议和链路复现工具，不替代真实硬件长期稳定性测试。
- Replay 不代表工业现场电气层、EMC、供电扰动或极端网络条件验证。
- 回放频率只描述本次输入调度配置，不构成固定吞吐量或时延指标。
