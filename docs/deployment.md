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

`sudo apt install -y libsqlite3-dev sqlite3`

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

生产 Broker 启用认证或 TLS 时，设置 `mqtt.username`、`mqtt.password` 和 `mqtt.tls`。`mqtt.tls.ca_file` 应指向可信 CA 文件；双向 TLS 还需同时配置 `certificate_file` 与 `private_key_file`。生产环境必须保持 `mqtt.tls.insecure: false`。

不要把真实密码和私钥提交到 Git。生产配置由安装脚本以 `root:iot-gateway`、`0640` 权限安装到 `/etc/linux-iot-edge-gateway/gateway.yaml`，证书和私钥也应限制为仅服务账户可读。

默认缓存配置为 `cache.type: sqlite`，数据库位于 `/var/lib/linux-iot-edge-gateway/pending_messages.db`。查看待补传记录：

`sudo sqlite3 /var/lib/linux-iot-edge-gateway/pending_messages.db 'SELECT id, topic, created_at FROM pending_messages ORDER BY id;'`

从旧文件缓存升级时，应先停止服务再执行迁移：

`sudo systemctl stop linux-iot-edge-gateway`

`sudo python3 scripts/migrate_file_cache.py /var/lib/linux-iot-edge-gateway/pending_messages.cache /var/lib/linux-iot-edge-gateway/pending_messages.db`

迁移成功后原文件会保留为 `.migrated` 备份，确认数据库内容后再启动服务。

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

## 8. ARM64 设备部署

在 x86_64 Ubuntu / WSL2 开发机执行：

ARM64 工具、sysroot 和部署产物默认保存到 `~/Tools/linux-iot-edge-gateway`。可通过环境变量覆盖：

`export AARCH64_TOOLS_ROOT=/path/to/linux-iot-edge-gateway-tools`

WSL 用户如需使用 D 盘，可显式设置为 `/mnt/d/Tools/linux-iot-edge-gateway`。

`./scripts/setup_aarch64_sysroot.sh`

`./scripts/build_aarch64.sh`

默认部署包位于：

`${AARCH64_TOOLS_ROOT:-$HOME/Tools/linux-iot-edge-gateway}/artifacts/linux-iot-edge-gateway-<version>-aarch64.tar.gz`

对应的 SHA-256 校验文件位于：

`${AARCH64_TOOLS_ROOT:-$HOME/Tools/linux-iot-edge-gateway}/artifacts/linux-iot-edge-gateway-<version>-aarch64.tar.gz.sha256`

将部署包传到 ARM64 Ubuntu 22.04 设备后，先安装运行时依赖：

`sudo apt install -y libyaml-cpp0.7 libmosquitto1 libsqlite3-0`

检查包内容后解压到根目录：

`sha256sum -c linux-iot-edge-gateway-<version>-aarch64.tar.gz.sha256`

`tar -tzf linux-iot-edge-gateway-<version>-aarch64.tar.gz`

`sudo tar -xzf linux-iot-edge-gateway-<version>-aarch64.tar.gz -C /`

随后按照本指南创建运行用户、数据目录和日志目录，再启用服务。首次部署应先执行 `file /usr/local/bin/edge_gateway`，确认输出包含 `ARM aarch64`。
