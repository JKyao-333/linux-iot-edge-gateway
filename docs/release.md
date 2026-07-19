# 版本发布指南

## 1. 版本来源

项目根目录的 `VERSION` 是唯一版本来源，格式必须为 `MAJOR.MINOR.PATCH`，例如 `1.0.0`。

CMake 配置阶段会读取该文件并生成版本头文件。运行以下命令可查看编译进程序的版本：

`~/linux-iot-edge-gateway-build/edge_gateway --version`

## 2. 本地发布检查

验证版本格式：

`./scripts/verify_release_version.sh`

验证标签是否与版本一致：

`./scripts/verify_release_version.sh v1.0.0`

构建 ARM64 部署包：

`./scripts/build_aarch64.sh`

构建脚本会生成带版本号的部署包及 SHA-256 校验文件：

- `linux-iot-edge-gateway-1.0.0-aarch64.tar.gz`
- `linux-iot-edge-gateway-1.0.0-aarch64.tar.gz.sha256`

## 3. 发布步骤

1. 修改 `VERSION`，遵循语义化版本规则。
2. 完成代码审查并运行 `./scripts/run_smoke_test.sh`。
3. 提交版本变更并推送 `main`。
4. 创建与版本一致的标签，例如 `git tag -a v1.0.0 -m "release v1.0.0"`。
5. 推送标签，例如 `git push origin v1.0.0`。

只有 `vMAJOR.MINOR.PATCH` 标签会触发 `.github/workflows/release.yml`。工作流首先检查标签与 `VERSION` 一致，然后交叉编译 ARM64 程序、执行 QEMU 启动检查、生成部署包和 SHA-256 文件，最后创建 GitHub Release。

## 4. 产物验证

下载部署包与校验文件后执行：

`sha256sum -c linux-iot-edge-gateway-1.0.0-aarch64.tar.gz.sha256`

GitHub Release 工作流还会通过官方 `actions/attest-build-provenance` 生成构建来源证明。该证明绑定仓库、工作流、提交和产物摘要，可使用 GitHub CLI 验证：

`gh attestation verify linux-iot-edge-gateway-1.0.0-aarch64.tar.gz --repo JKyao-333/linux-iot-edge-gateway`

## 5. 版本规则

- `MAJOR`：不兼容的协议或配置变更。
- `MINOR`：向后兼容的新功能。
- `PATCH`：向后兼容的问题修复。

不要重复使用已经发布的标签，也不要在发布后修改对应标签指向。
