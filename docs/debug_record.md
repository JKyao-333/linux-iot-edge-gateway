# 项目调试记录

## 1. WSL Ubuntu 安装失败

### 现象

安装 Ubuntu 22.04 时出现：

`Wsl/InstallDistro/WININET_E_CANNOT_CONNECT`

### 原因

Windows 网络服务暂时无法连接发行版下载服务器。

### 处理

执行 `wsl --update`，重新尝试安装 Ubuntu 22.04。

### 结果

Ubuntu 22.04.5 LTS 成功安装并运行于 WSL2。

---

## 2. Mosquitto 端口被占用

### 现象

手动执行 `mosquitto -v -p 1883` 时出现：

`Error: Address already in use`

### 原因

Mosquitto 系统服务已经监听 1883 端口，重复启动产生端口冲突。

### 排查命令

`ss -lnt | grep ':1883'`

### 处理

直接使用已经运行的系统服务，不再重复启动 Broker。

---

## 3. Windows 挂载目录编译异常

### 现象

在 `/mnt/e/Github` 下构建时出现：

- `Clock skew detected`
- `modification time is in the future`
- `ld: cannot open output file edge_gateway: No such file or directory`

### 原因

源码位于 Windows DrvFs 挂载目录，Windows 与 WSL 文件时间和文件操作语义存在差异，导致增量构建状态不稳定。

### 处理

源码继续保存在 E 盘，但构建目录改为 WSL Linux 文件系统：

`~/linux-iot-edge-gateway-build`

配置和构建命令：

`cmake -S . -B ~/linux-iot-edge-gateway-build`

`cmake --build ~/linux-iot-edge-gateway-build`

### 结果

链接错误消失，增量构建恢复稳定。

---

## 4. 模拟器 CRC 与协议不一致

### 现象

Python 模拟器发送的帧能够被串口读取，但 FrameParser 不输出有效帧。

### 原因

模拟器最初使用占位 CRC `00 00`，与 C++ CRC16-Modbus 计算结果不一致。

### 处理

明确 CRC 计算范围为：

长度 + 命令字 + 设备 ID + Payload

CRC 不包含帧头，并按低字节、高字节顺序发送。

当前传感器帧 CRC 为：

- 计算值：`0x584D`
- 发送字节：`4D 58`

### 结果

C++ 网关成功解析 Python 发送的协议帧。

---

## 5. C++ 函数粘贴位置错误

### 现象

编译出现：

- `qualified-id in declaration before '(' token`
- `FileCache has not been declared`
- `CachedMessage was not declared`

### 原因

新增函数被粘贴到了另一个函数内部，或者放到了命名空间外部，导致作用域和大括号不匹配。

### 排查方法

检查报错行之前的函数是否已经正确闭合，重点核对：

- 函数右大括号
- namespace 右大括号
- 类成员函数的命名空间
- 函数签名是否与头文件一致

### 处理

将 `replace_all()`、`flush_cache()` 等成员函数移动到正确的命名空间和函数边界。

### 结果

相关静态库和测试程序成功编译。

---

## 6. ReliablePublisher 参数类型不一致

### 现象

修改主程序接入可靠发布器后出现：

- `publisher was not declared`
- 无法将 `ReliablePublisher` 绑定到 `MqttClient&`

### 原因

`handle_frame()` 的参数仍然是 `MqttClient&`，函数内部却开始使用 `ReliablePublisher`。

### 处理

统一函数签名、调用参数和变量名称，使 `handle_frame()` 接收 `ReliablePublisher&`。

### 结果

主程序能够在 MQTT 失败时调用缓存逻辑。

---

## 7. 串口断开后网关退出

### 现象

停止 socat 后，网关输出：

`read failed: Input/output error`

随后进程退出。

### 原因

主循环在 `read()` 返回错误时直接执行 `break`。

### 处理

改为：

1. 记录串口读取错误。
2. 关闭失效文件描述符。
3. 将 fd 设为无效状态。
4. 清空解析器残留半帧。
5. 每 2 秒尝试重新打开串口。
6. 连接恢复后继续主循环。

### 结果

网关能够在不重启进程的情况下恢复串口通信。

---

## 8. FrameParser 缺少 reset()

### 现象

加入串口重连代码后编译出现：

`FrameParser has no member named reset`

### 原因

主程序需要在断线后清除残留半帧，但解析器尚未提供重置接口。

### 处理

为 FrameParser 增加 `reset()`：

- 清空内部字节缓冲区
- 清零 CRC 错误计数
- 清零长度错误计数

### 结果

重连后新数据不会与断线前残留数据错误拼接。

---

## 9. MQTT 离线消息重复测试

### 现象

多次停止 Broker 并发送数据后，缓存文件中出现多条内容相同的消息。

### 原因

Python 模拟器持续发送相同序号的数据，每次发布失败都会按设计写入缓存。

### 处理

测试前清理专用缓存文件，并使用独立测试路径，避免历史数据影响测试判断。

### 结果

自动化测试之间相互隔离，测试结果可重复。

---

## 10. 调试经验总结

本项目形成了以下排障方法：

1. 先区分编译错误、运行错误和协议错误。
2. 根据第一条编译错误检查作用域和函数边界。
3. 使用十六进制日志确认实际收到的串口字节。
4. 使用独立程序交叉验证 CRC 结果。
5. 使用 `ss` 检查网络端口状态。
6. 使用日志和缓存文件验证离线行为。
7. 将源码目录和构建目录分离。
8. 为每个故障建立可重复的测试用例。
