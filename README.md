# 灵动岛 · 液态玻璃

基于 Himanshu / devcode90 的 Dynamic Island for Windows 修改，使用 Windhawk 运行，液态玻璃后端来自 hicccc77/electron-liquid-glass。保留原作者及第三方 MIT 许可证。

## 安装

支持 Windows x64、本机管理员账号和 Windhawk **1.7.3 安装版**。需要先安装并运行 Windhawk；本项目无需提交或通过 Windhawk 官方模组库安装。

1. 从 [Releases](https://github.com/czzyou/windhawk-island-liquid-glass/releases/latest) 下载 `island-setup.zip`。
2. 解压，关闭其他已启用的灵动岛模组，再双击 `install.cmd`，接受首次管理员授权。
3. 安装器从本仓库下载已签名源码并在本机编译，完成后启用。首次主队选择约在启动 45 秒后显示，也可从右键菜单设置。

默认英文、数字使用 **SF Pro Text**，岛内中文优先使用 **苹方**。字体需用户自行安装，公开仓库和安装包不包含字体文件；缺少字体时使用系统回退，见[字体安装说明](fonts/README.md)。

安装更新器约 1 分钟后触发首次检查，之后每 **3 小时**静默检测。无更新不提示；有更新只在灵动岛外侧边缘显示静态黄色叹号，不弹窗、不抢焦点、不自动安装。休眠时不主动唤醒电脑，失败后不立即重试。

**已有用户需重新运行新版 `install.cmd` 一次以升级更新器到 1.0.2、替换计划任务。** 普通插件更新无法替换旧更新器脚本或修改其检测频率。

按住 **Alt＋鼠标左键** 可拖动灵动岛，松开左键后记住位置。手动放置后暂停自动标题栏避让，避免被拉回。

右键灵动岛 → **检查更新…** 可随时手动检测；黄色叹号出现时菜单改为 **安装可用更新…**。只有手动操作才下载、校验签名、编译并切换，保留天气城市、NBA 主队、日程和外观设置。忙碌时推迟安装，完成后再手动更新。

**Roll back previous version** 回退上一版；**Uninstall updater** 只移除更新机制，保留插件与个人数据。

此首版已验证构建、签名、数据保留及模拟回退；尚未在第二台电脑完成首次安装、计划任务和真实重启的全流程验证。现有手动导入版和自用版不会自动被迁移或覆盖。

## 修改与发布

固定操作见 [发布流程](发布流程.md)：修改与本机试用之后，用户确认发布，再提交源码并发布正式 Release。只 push 代码不会让使用者收到更新。

日常发布使用现有签名密钥，不需要重新初始化。更新版本和说明后，先准备附件并提交、推送源码，再运行发布命令：

```powershell
# 生成并检查附件，不发布
powershell -NoProfile -File updater/publish.ps1 -PrepareOnly
# 按发布流程提交并推送源码
# 上传草稿，附件齐全后发布为 Latest 正式版
powershell -NoProfile -File updater/publish.ps1 -PublishPrepared -NotesFile RELEASE_NOTES.md
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
