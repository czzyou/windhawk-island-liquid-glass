# 灵动岛 · 液态玻璃

基于 Himanshu / devcode90 的 Dynamic Island for Windows 修改，使用 Windhawk 运行，液态玻璃后端来自 hicccc77/electron-liquid-glass。保留原作者及第三方 MIT 许可证。

## 安装

支持 Windows x64、本机管理员账号和 Windhawk **1.7.3 安装版**。需要先安装并运行 Windhawk；本项目无需提交或通过 Windhawk 官方模组库安装。

1. 从 [Releases](https://github.com/czzyou/windhawk-island-liquid-glass/releases/latest) 下载 `island-setup.zip`。
2. 解压，关闭其他已启用的灵动岛模组，再双击 `install.cmd`，接受首次管理员授权。
3. 安装器从本仓库下载已签名源码并在本机编译，完成后启用。首次主队选择约在启动 45 秒后显示，也可从右键菜单设置。

每次开机首次登录后延迟 60 秒自动检查一次；不会每隔几分钟轮询，也不会在同次开机重新登录或休眠唤醒后重复检查。

开始菜单 **Windhawk Island → Check for updates** 可随时手动检测。检测到新版本就自动下载、校验签名、编译并切换，保留天气城市、NBA 主队、日程和外观设置。忙碌或断网时保持旧版，下次开机或手动检查再试。

**Roll back previous version** 回退上一版；**Uninstall updater** 只移除更新机制，保留插件与个人数据。

此首版已验证构建、签名、数据保留及模拟回退；尚未在第二台电脑完成首次安装、计划任务和真实重启的全流程验证。现有手动导入版和自用版不会自动被迁移或覆盖。

## 修改与发布

维护者修改 `release.json` 中的正式版本后运行：

```powershell
python -m pip install -r requirements.txt
# 首次生成私钥，存于当前用户的 LOCALAPPDATA，绝不上传
powershell -NoProfile -File updater/publish.ps1 -InitializeKey
# 生成并检查附件，不发布
powershell -NoProfile -File updater/publish.ps1 -PrepareOnly
# 上传草稿，附件齐全后发布为 Latest 正式版
powershell -NoProfile -File updater/publish.ps1 -NotesFile RELEASE_NOTES.md
```

已有公钥对应的私钥只在原维护者 Windows 用户配置中存在；参与开发的人可以修改源码并提交 PR，无法冒用原维护者签名。发布新版本必须严格提高三段数字版本号。签名密钥不可丢失或替换；备份说明见 [更新器文档](updater/README.md)。

```powershell
python build.py --release
python release-checks.py
powershell -NoProfile -File updater/test.ps1
# 额外验证更新器实际调用本机编译器，输出仍在隔离目录
powershell -NoProfile -File updater/test.ps1 -Compile
```

公开仓库仅分享给熟人不构成访问限制。客户端不需要 GitHub 账号、Token、Git、Python 或 GitHub CLI。日志和回退备份仅保存在本机。
