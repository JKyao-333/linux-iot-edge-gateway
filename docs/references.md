# 技术参考资料

本页记录项目实现、部署和验证所依据的规范、官方文档与相关开源工程。访问日期为 2026-07-21。

## 1. 系统与构建

- POSIX `termios` / `tcsetattr`：[The Open Group - tcsetattr](https://pubs.opengroup.org/onlinepubs/9699919799/functions/tcsetattr.html)
- CMake 交叉编译工具链：[cmake-toolchains(7)](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)
- GNU GCC AArch64 目标选项：[AArch64 Options](https://gcc.gnu.org/onlinedocs/gcc/AArch64-Options.html)
- QEMU 用户态模拟：[QEMU User space emulator](https://www.qemu.org/docs/master/user/main.html)
- socat PTY 与字节流转发：[socat documentation](http://www.dest-unreach.org/socat/doc/socat.html)
- systemd 服务单元：[systemd.service](https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html)

## 2. MQTT 与持久化

- Eclipse Mosquitto 文档：[Mosquitto Documentation](https://mosquitto.org/documentation/)
- libmosquitto C API：[mosquitto.h API reference](https://mosquitto.org/api/files/mosquitto-h.html)
- SQLite WAL：[Write-Ahead Logging](https://www.sqlite.org/wal.html)
- SQLite PRAGMA：[PRAGMA synchronous](https://www.sqlite.org/pragma.html#pragma_synchronous)

## 3. 配置与协议

- yaml-cpp：[jbeder/yaml-cpp](https://github.com/jbeder/yaml-cpp)
- Modbus 串行链路规范：[MODBUS over Serial Line Specification and Implementation Guide V1.02](https://www.modbus.org/docs/Modbus_over_serial_line_V1_02.pdf)
- CRC16-Modbus 的多项式、初值和字节顺序以 Modbus 串行链路规范为准；项目测试向量见 `tests/crc16_test.cpp`。

## 4. 相关开源工程

- [emqx/neuron](https://github.com/emqx/neuron)：工业协议南向接入、MQTT 北向连接和多架构边缘部署的架构参考。
- [je-s/Serial2MqttGateway](https://github.com/je-s/Serial2MqttGateway)：串口设备识别、I/O 异常处理和设备 Topic 层级的实现参考。
- [svrooij/smartmeter2mqtt](https://github.com/svrooij/smartmeter2mqtt)：Raspberry Pi ARM64、稳定串口设备路径以及 MQTT/TCP 多输出的复现环境参考。
- [FIT IoT-LAB OpenM3 schematic](https://github.com/iot-lab/iot-lab/wiki/Docs/openm3-schematics.pdf)：STM32F103RxY Cortex-M3 参考硬件。

这些项目用于技术调研和复现平台选择，不构成本项目硬件测试结果或性能数据来源。
