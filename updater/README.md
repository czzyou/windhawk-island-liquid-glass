# 灵动岛自动更新

本包用于 Windhawk 1.7.3 安装版、x64 Windows，使用者需要本机管理员账号。Windhawk 必须已安装并运行。

首次解压后双击 `install.cmd`，接受一次 Windows 管理员授权。若 Windows 阻止来自网络的文件，先确认安装包由维护者提供，再在 ZIP 的属性中解除锁定并重新解压。

如果已经启用了其他灵动岛，请先在 Windhawk 中关闭它；安装器不会替你停用或覆盖其他模组。自动更新版使用独立的 `local@dynamic-island-liquid-glass` 标识，旧模组数据不会自动迁移。

安装后，每次开机首次登录延迟 60 秒检查一次；再次登录和休眠唤醒不重复自动检查，也没有每隔几分钟的后台轮询。

开始菜单 `Windhawk Island` 中提供：

- `Check for updates`：随时手动检查，检查到新版后自动下载、校验、编译并启用；结果会弹窗显示。
- `Roll back previous version`：回到上一版本，阻止马上重装该版本。
- `Uninstall updater`：移除自动检测任务和更新器，保留插件、个人设置和回退备份。

编译期间旧版继续使用；切换时灵动岛会短暂重启。正在计时或编辑日程时推迟更新，结束后可手动再检查。断网或检查失败后不在后台反复尝试，等下次开机或手动触发。

更新会验证维护者的 RSA 签名和源码 SHA256。保留天气城市、主队、日程和外观设置。手动禁用的插件不会被更新器启用。手动修改过受管理源码时停止覆盖并提示。

日志：`%ProgramData%\Windhawk-Island-Updater\update.log`；编译失败详情在同目录的对应版本临时子目录中。上线前须经过其他电脑验证；当前不声明支持 ARM64、便携版或其他 Windhawk 版本。

## 维护者

发布配置在根目录 `release.json`。只提高正式版本号，通过 `updater/publish.ps1` 构建、检查并发布。普通代码提交、草稿和预发布不会推送给客户端。

首次执行 `powershell -NoProfile -File updater/publish.ps1 -InitializeKey` 创建签名密钥；私钥保存在当前用户 LOCALAPPDATA 的 `Windhawk-Island-Publisher/signing-key.dpapi`，使用 Windows DPAPI 绑定当前账号，不能提交到 GitHub。此文件单独拷贝到其他账号不能直接恢复，请另外做好原 Windows 用户配置/恢复凭据的离线备份；不要删除或覆盖密钥。

执行 `powershell -NoProfile -File updater/publish.ps1 -PrepareOnly` 只生成可检查的发布附件。实际发布使用 `-NotesFile <版本说明路径>`；GitHub CLI 只在维护者电脑使用，客户端不需要登录 GitHub。

公开仓库任何人都能访问，只向熟人分享链接不等于访问控制。不要上传个人注册表、日程、通知、编译日志或私钥。原作者 Himanshu/devcode90 及第三方 MIT 许可证保留在源码中。
