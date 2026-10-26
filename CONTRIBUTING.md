# 贡献指南

欢迎通过 Fork 和 Pull Request（PR）提交修改。请先阅读 [维护约定](AGENTS.md)。

## 修改源码

- 修改对应的补丁脚本、头文件或 `build.py`，并重新生成发布源码 `dynamic-island-liquid-glass.wh.cpp`，将两者一起提交；不要只修改生成文件。
- 保留原作者和第三方许可证。
- 普通 PR 无需提高插件版本号；版本号、发布说明和正式发布由维护者统一处理。仅修改文档或 Actions 配置时无需重新生成源码。
- 提交前检查文件列表，排除个人配置、注册表、日程、通知、日志、迁移备份、字体文件和签名私钥。

## 检查修改

在仓库根目录使用 Python 3.13，运行与 Actions 相同的源码检查：

```shell
python -m pip install -r requirements.txt
python build.py --release --source-only
```

这项检查不需要安装 Windhawk，会验证补丁能否合成、发布版本格式和设置 YAML。推送代码或提交 PR 后，GitHub Actions 的 `Source check` 会执行相同命令。若首次运行需要维护者批准，以 PR 页面提示为准。

`Windows build` 还会自动使用固定 Windhawk 1.7.3 工具链编译 x64/x86，并运行回归、发布和隔离更新器检查。通过后可在该次运行中下载 `island-build-提交号` 试用包；它包含源码和两种架构 DLL，保留 30 天。

**自动检查通过不能代替实际体验。** 用户作为首位体验测试者；有必要在本机单独构建时，使用 Windhawk 1.7.3 工具链：

```shell
python build.py --release
```

界面、动画和交互仍需实际体验。请在 PR 中说明改动内容、已完成的检查及尚未验证的部分；已有有效检查结果时无需重复整套测试。Actions 失败时，查看对应运行日志并修复后重新推送。

## 发布分工

贡献者提交源码和 PR；Actions 负责自动编译、检查和试用包。用户先体验并明确要求发布后，维护者按 [发布流程](发布流程.md)在本机核对当前提交的 Actions 成功结果、签名并发布正式 Release，不重复本机构建。贡献者无需发布私钥，也无需运行 `publish.ps1 -InitializeKey`。Actions 只具有仓库内容读取权限，不执行签名或发布。
