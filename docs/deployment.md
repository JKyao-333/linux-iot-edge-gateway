# systemd 部署指南

## 1. 部署目标

本项目提供 systemd 服务单元和安装脚本，用于将边缘网关作为 Linux 后台服务运行。部署后具备：

- 开机自动启动
- 低权限用户运行
- 串口访问权限
- 异常退出自动拉起
- SIGTERM 优雅停止
- journal 与文件双通道日志
- 配置、缓存和日志目录隔离

## 2. 前置条件

完成依赖安装和项目编译：

`cmake -S . -B ~/linux-iot-edge-gateway-build`

`cmake --build ~/linux-iot-edge-gateway-build --parallel`

确认网关程序存在：

`test -x ~/linux-iot-edge-gateway-build/edge_gateway`

## 3. 一键安装

将参数替换为设备实际串口：

`sudo ./scripts/install_systemd_service.sh /dev/ttyUSB0`

安装脚本会：

1. 创建 `iot-gateway` 系统用户和用户组。
2. 将该用户加入 `dialout` 附加组。
3. 安装程序到 `/usr/local/bin/edge_gateway`。
4. 安装配置到 `/etc/linux-iot-edge-gateway/gateway.yaml`。
5. 创建缓存目录 `/var/lib/linux-iot-edge-gateway`。
6. 创建日志目录 `/var/log/linux-iot-edge-gateway`。
7. 注册、启用并启动 systemd 服务。

如果构建目录不是默认路径，可显式指定程序：

`sudo GATEWAY_BIN=/path/to/edge_gateway ./scripts/install_systemd_service.sh /dev/ttyUSB0`

## 4. 服务管理

查看状态：

`systemctl status linux-iot-edge-gateway`

查看实时 journal：

`journalctl -u linux-iot-edge-gateway -f`

启动、停止和重启：

`sudo systemctl start linux-iot-edge-gateway`

`sudo systemctl stop linux-iot-edge-gateway`

`sudo systemctl restart linux-iot-edge-gateway`

## 5. 修改配置

生产配置路径：

`/etc/linux-iot-edge-gateway/gateway.yaml`

修改后重启服务：

`sudo systemctl restart linux-iot-edge-gateway`

默认生产配置启用 MQTT，关闭 TCP。需要 TCP 双通道上报时，将 `tcp.enabled` 改为 `true`，并配置服务端地址和端口。

## 6. 优雅退出

执行 `systemctl stop` 后，systemd 向网关发送 SIGTERM。网关只在信号处理函数中设置退出标志，随后由主循环完成：

1. 退出串口读取或重连循环。
2. 关闭串口文件描述符。
3. 关闭 TCP 连接。
4. 停止 MQTT 网络循环并释放客户端。
5. 刷新并关闭日志文件。
6. 以状态码 `0` 退出。

服务单元设置 `TimeoutStopSec=10s`。自动化 smoke test 要求网关在 5 秒内退出。

## 7. 卸载

卸载服务和程序，保留配置、缓存及日志：

`sudo ./scripts/uninstall_systemd_service.sh`

同时删除配置和运行数据：

`sudo ./scripts/uninstall_systemd_service.sh --purge`

`--purge` 会永久删除 `/etc`、`/var/lib` 和 `/var/log` 下的网关目录，应确认数据无需保留后再执行。
