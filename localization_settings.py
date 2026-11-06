import yaml


GROUPS = {
    'Appearance': '外观与位置', 'Behavior': '行为与显示', 'Animations': '动画与性能',
    'Themes': '颜色与主题', 'Indicators': '隐私指示', 'Modules': '模块与功能', 'Shortcuts': '快捷键',
}

# Chinese setting labels and descriptions; English remains the default metadata.
SETTINGS = {
    'Position': ('位置', '选择灵动岛在屏幕上的位置。'),
    'TargetMonitor': ('目标显示器', '选择显示器；未找到时回退到主显示器。'),
    'OffsetX': ('水平偏移', '水平位置偏移，单位为像素；正数向右，负数向左。'),
    'OffsetY': ('垂直偏移', '垂直位置偏移，单位为像素；正数向下，负数向上。启用单独的展开偏移后，此值用于收起状态。'),
    'SeparateExpandedOffsetY': ('展开时使用单独的垂直偏移', '为展开和收起状态分别设置高度，切换时平滑过渡。'),
    'OffsetYExpanded': ('展开时的垂直偏移', '仅在启用单独的展开偏移时生效，单位为像素。'),
    'BorderMergedMode': ('贴合屏幕边缘', '将灵动岛贴合屏幕顶部，不留悬浮间隙。'),
    'ShapeStyle': ('灵动岛形状', '选择胶囊、Windows 11 圆角矩形或 macOS 刘海形状。'),
    'SizeScale': ('整体大小', '缩放灵动岛及其内容；小屏幕会自动缩小基础尺寸，最多缩小 25%。'),
    'AutoDpiScale': ('自动 DPI 缩放', '根据显示器 DPI 自动缩放，适合高分辨率屏幕。'),
    'AlwaysOnTop': ('窗口置顶', '让灵动岛显示在其他窗口上方。'),
    'ExpandOnHover': ('悬停展开', '鼠标悬停时自动展开；关闭后点击展开。'),
    'AutoHideIdleSeconds': ('自动隐藏时间', '所有状态在指定秒数无操作后隐藏；0 表示不隐藏。'),
    'AutoHideFullscreen': ('全屏时隐藏', '应用或视频全屏时自动隐藏。'),
    'UnhideOnHover': ('悬停时重新显示', '将鼠标移到隐藏位置时重新显示灵动岛。'),
    'TargetFPS': ('刷新率 / FPS', '自动匹配当前显示器刷新率，或选择固定帧率。'),
    'AnimationStyle': ('动画风格', '控制展开与收起动画的弹性。'),
    'AnimationSpeed': ('动画速度', '控制灵动岛展开和收起的速度。'),
    'ThemePreset': ('主题预设', '选择预设主题，或使用下方的自定义十六进制颜色。'),
    'PillOpacity': ('灵动岛不透明度', '范围 35～100，数值越低越透明。'),
    'LiquidGlass': ('折射液态玻璃', '自定义主题且背景材质关闭时，使用 GPU 实时折射桌面；普通截图仍可使用。'),
    'TintIntensity': ('背景着色强度', '范围 0～100，控制灵动岛背景着色深浅。'),
    'BackdropMaterial': ('背景材质', '使用 Windows 模糊或亚克力背景；不支持时使用不透明背景。'),
    'BackdropFillOpacity': ('背景填充不透明度', '范围 0～100；启用背景材质时，数值越低越能透出模糊背景。'),
    'BackdropTint': ('亚克力着色强度', '范围 0～100，仅用于亚克力背景。'),
    'MaterialDepth': ('深度阴影', '添加柔和的深度阴影与强调色；关闭后使用平面效果。'),
    'DropShadow': ('柔和投影', '在灵动岛下方绘制投影。'),
    'AmbientAuroraGlow': ('专辑氛围背光', '根据专辑主要颜色和占比，在灵动岛周围绘制柔和的呼吸背光。'),
    'AmbientGlowIntensity': ('氛围背光强度', '范围 0～200，控制专辑氛围背光亮度。'),
    'AccentBloom': ('强调色光晕强度', '范围 0～200；自动模式使用专辑颜色，设为 0 可关闭。'),
    'TextScale': ('文字大小', '范围 70～160，独立于整体尺寸缩放文字。'),
    'AccentColorMode': ('强调色模式', '选择强调色来源；自动模式从专辑封面提取。'),
    'CustomAccentHex': ('自定义强调色', '自定义模式使用的十六进制颜色。'),
    'CalendarAccent': ('日历强调色', '设置日历月份、周末和当天标记的强调色。'),
    'ClockAccentGlow': ('显示时钟背景光晕', '在展开的时钟后显示柔和光晕。'),
    'FontFamily': ('字体', '指定灵动岛字体；留空使用系统默认字体。'),
    'ContourBorderMode': ('外轮廓边框', '选择主题默认边框、专辑自动取色或无边框。'),
    'ContourBorderHex': ('轮廓颜色', '十六进制轮廓颜色；修改默认值会覆盖主题预设。'),
    'PillBgColor': ('背景颜色', '支持 3、4、6 或 8 位十六进制颜色；8 位 RRGGBBAA 可设置透明度。修改默认值会覆盖主题预设。'),
    'TextPrimaryColor': ('主要文字颜色', '标题和主要文字的十六进制颜色；修改默认值会覆盖主题预设。'),
    'TextSecondaryColor': ('次要文字颜色', '艺术家名称和次要文字的十六进制颜色；修改默认值会覆盖主题预设。'),
    'PrivacyDots': ('显示隐私指示', '摄像头或麦克风使用时显示隐私圆点。'),
    'PrivacyDotsMic': ('显示麦克风指示', '麦克风使用时显示橙色圆点。'),
    'PrivacyDotsMicHex': ('麦克风指示颜色', '麦克风隐私圆点的十六进制颜色。'),
    'PrivacyDotsCam': ('显示摄像头指示', '摄像头使用时显示绿色圆点。'),
    'PrivacyDotsCamHex': ('摄像头指示颜色', '摄像头隐私圆点的十六进制颜色。'),
    'Media': ('媒体模块', '播放媒体时显示封面、歌曲信息和播放控制。'),
    'MediaAutoExpand': ('切歌时自动展开', '歌曲或视频切换时自动展开；关闭后在收起状态平滑更新。'),
    'Volume': ('音量滑条', '调整系统音量时显示灵动岛音量面板。'),
    'CapsLock': ('大小写锁定提示', '显示 Caps Lock 和 Num Lock 的状态变化。'),
    'Battery': ('电池模块', '笔记本电量较低时显示提醒。'),
    'BluetoothIndicator': ('蓝牙连接提示', '蓝牙设备连接或断开时显示设备名称、图标和可用电量。'),
    'BluetoothShowBattery': ('显示蓝牙电量', '通过 BLE GATT 读取支持的设备电量；不支持时只显示连接状态。'),
    'Progress': ('进度模块', '为下载或文件复制显示进度环。'),
    'TimerModule': ('专注计时器', '启用专注 / 休息计时器，可通过右键菜单启动。'),
    'NBAScores': ('NBA 实时比分', '默认关闭；启用后从右键菜单设置主队，无直播时显示普通时钟。'),
    'Clipboard': ('剪贴板模块', '快速预览刚复制的文字或图片。'),
    'StatusCountdownProgress': ('状态提示倒计时', '在临时状态提示底部显示倒计时进度条。'),
    'DoNotDisturbIndicator': ('勿扰状态提示', 'Windows 勿扰模式切换时显示提示。'),
    'HardwareMonitorModule': ('硬件监视页面', '在滚动页面中显示 CPU、GPU、内存、磁盘及网络信息。'),
    'GameOverlay': ('游戏悬浮窗', '用实时 FPS、CPU 和内存等信息替代时钟。'),
    'ShowMetricText': ('显示指标标签', '在游戏悬浮窗指标中显示 CPU 等文字。'),
    'Weather': ('天气模块', '在灵动岛中显示天气；关闭后只显示时钟。'),
    'WeatherCity': ('天气城市（留空按公网 IP 定位）', '留空自动定位；代理或 VPN 可能影响结果。也可填写九位中国天气网城市编号，例如北京 101010100。'),
    'MealReminders': ('吃饭提醒', '按本机时间提醒午饭和晚饭。'),
    'LunchTime': ('午饭时间', '使用 24 小时制 HH:mm，留空或格式错误则不提醒。'),
    'DinnerTime': ('晚饭时间', '使用 24 小时制 HH:mm，留空或格式错误则不提醒。'),
    'WeatherFahrenheit': ('使用华氏度', '温度使用华氏度，风速使用英制单位。'),
    'ClockFormat': ('时钟格式', '选择 12 小时制、24 小时制或跟随 Windows 区域格式。'),
    'ShowSeconds': ('显示秒数', '在时钟中显示秒数，可能略微增加 CPU 使用。'),
    'DateFormat': ('自定义日期格式', '留空跟随系统；支持 yyyy、MM、M、dd、d、MMM、MMMM、ddd、dddd。'),
    'DateFirst': ('日期显示在时间前', '交换日期与时间的显示顺序。'),
    'Lyrics': ('实时歌词', '支持网易云、QQ 音乐、酷狗、酷我、Spotify 和 Apple Music 客户端；浏览器不显示歌词。'),
    'CollapsedLyrics': ('收起时显示歌词', '播放音乐且胶囊收起时显示当前歌词，也可在右键菜单切换。'),
    'MediaTabLyrics': ('媒体页内显示歌词', None), 'LyricsCache': ('缓存歌词到本机', None),
    'LyricsCacheMaxMB': ('歌词缓存容量', None),
    'LyricsOffsetMs': ('歌词偏移（扬声器 / 有线，毫秒）', '范围 -3000～3000；正数提前，负数延后。'),
    'LyricsOffsetBluetoothMs': ('歌词偏移（蓝牙，毫秒）', None),
    'QuickLookup': ('快捷查词', '支持英中、中英词典；输入暂停后向有道请求候选，Enter 查询。最近记录仅存内存。'),
    'FileTrayModule': ('文件托盘', '拖入文件暂存路径，单击打开、按住拖出。点击 × 后，本机普通文件将在 7 天后永久删除原文件（不进入回收站）；七天内重新拖入可取消，文件修改或替换后也会取消。退出期间不执行，下次启动补查。文件夹和右键清空只移除入口。'),
    'FileTrayMaxItems': ('文件托盘容量', '范围 1～25，超过容量时移除最旧的入口。'),
    'MediaExpandBlocklist': ('这些应用切歌时不自动展开', '用逗号分隔应用或网站名称，例如 chrome、tiktok、youtube；收起状态仍会更新。'),
    'GameOverlayShowFps': ('游戏悬浮窗：显示 FPS', '显示帧率。'),
    'GameOverlayShowCpu': ('游戏悬浮窗：显示 CPU', '显示 CPU 使用率。'),
    'GameOverlayShowGpu': ('游戏悬浮窗：显示 GPU', '显示 GPU 使用率。'),
    'GameOverlayShowRam': ('游戏悬浮窗：显示内存', '显示内存使用率。'),
    'GameOverlayShowDisk': ('游戏悬浮窗：显示磁盘', '显示磁盘使用率。'),
    'GameOverlayCompact': ('游戏悬浮窗：紧凑尺寸', '缩小为适合任务栏的窄条。'),
    'HideShowHotkeyEnabled': ('启用显示 / 隐藏快捷键', '隐藏后只能使用快捷键重新显示；关闭前请确认能在设置中重新启用。'),
    'HideShowModifiers': ('显示 / 隐藏快捷键修饰键', '与下方字母或数字组合使用。'),
    'LookupModifiers': ('查词快捷键修饰键', None), 'LookupKey': ('查词快捷键（Space 或 A-Z、0-9）', None),
    'HideShowKey': ('显示 / 隐藏快捷键', '使用 A-Z 或 0-9 单个字符；无效时回退到 D。'),
}

ENGLISH = {
    'AmbientAuroraGlow': ('Album ambient glow', None), 'AmbientGlowIntensity': ('Ambient glow intensity', None),
    'NBAScores': (None, 'Off by default. Enable this and choose favorites from the island menu; the clock is shown when no game is live.'),
    'WeatherCity': ('Weather city (blank: locate by public IP)', 'Leave blank for IP location; proxies and VPNs can affect it. Or enter a nine-digit China Weather city code, such as Beijing 101010100.'),
    'MealReminders': ('Meal reminders', 'Remind for lunch and dinner using local time.'),
    'LunchTime': ('Lunch time', 'Use 24-hour HH:mm. Blank or invalid values disable the reminder.'),
    'DinnerTime': ('Dinner time', 'Use 24-hour HH:mm. Blank or invalid values disable the reminder.'),
    'Lyrics': ('Live lyrics', 'Supports NetEase, QQ Music, Kugou, Kuwo, Spotify and Apple Music desktop apps, not browsers. Lyrics are matched using NetEase, QQ, Kugou and LRCLIB.'),
    'CollapsedLyrics': ('Show lyrics when collapsed', 'Show the current lyric while the island is collapsed; also available in the right-click menu.'),
    'MediaTabLyrics': ('Show lyrics on media page', None), 'LyricsCache': ('Cache lyrics locally', None),
    'LyricsCacheMaxMB': ('Lyrics cache limit', None),
    'LyricsOffsetMs': ('Lyrics offset (speaker / wired, ms)', '-3000 to 3000; positive values advance and negative values delay lyrics.'),
    'LyricsOffsetBluetoothMs': ('Lyrics offset (Bluetooth, ms)', None),
    'QuickLookup': ('Quick lookup', 'English–Chinese dictionary. Suggestions are requested from Youdao after typing; press Enter to look up. Recent queries stay in memory.'),
    'FileTrayModule': ('File tray (drag and drop)', 'Dragging files stores their paths; click to open or drag them out. Clicking × schedules permanent deletion of a local original file after 7 days, without the Recycle Bin. Re-add within 7 days to cancel; modified or replaced files are protected. Deletion resumes after restart. Folders and clearing from the menu only remove entries.'),
    'LookupModifiers': ('Lookup hotkey modifiers', None), 'LookupKey': ('Lookup hotkey (Space or A-Z, 0-9)', None),
}

OPTIONS = {
    'Top Center': '顶部居中', 'Top Left': '左上', 'Top Right': '右上', 'Bottom Center': '底部居中',
    'Bottom Left': '左下', 'Bottom Right': '右下', 'Primary Monitor': '主显示器',
    'Display 1': '显示器 1', 'Display 2': '显示器 2', 'Display 3': '显示器 3', 'Display 4': '显示器 4',
    'Display 5': '显示器 5', 'Follow Mouse (Active Monitor)': '跟随鼠标所在显示器',
    'Default (Apple Pill)': '默认胶囊', 'Windows 11 (Rounded Box)': 'Windows 11 圆角矩形', 'macOS Notch (Top Edge Flush)': 'macOS 刘海（贴合顶部）',
    'Hide instantly': '立即隐藏', 'Never hide (default)': '从不隐藏（默认）', 'Hide after 5 seconds': '5 秒后隐藏',
    'Hide after 10 seconds': '10 秒后隐藏', 'Hide after 30 seconds': '30 秒后隐藏', 'Hide after 60 seconds': '60 秒后隐藏',
    'Auto (Match Monitor Refresh Rate)': '自动匹配显示器刷新率', '60 FPS (Eco / Standard)': '60 FPS（节能 / 标准）',
    '360 FPS (Ultra Smooth)': '360 FPS（极致流畅）', '500 FPS (Maximum / Uncapped)': '500 FPS（最高）',
    'Smooth (No bounciness / Critically damped)': '平滑（无弹性）', 'Default (Balanced Apple-like spring)': '默认（均衡弹性）',
    'Bouncy (Dynamic elastic spring)': '弹性', 'Snappy (High stiffness, quick settle)': '迅捷',
    'Very Slow (0.5x)': '非常慢（0.5x）', 'Slow (0.75x)': '慢（0.75x）', 'Normal (1.0x)': '正常（1.0x）',
    'Fast (1.35x)': '快（1.35x）', 'Very Fast (1.65x)': '非常快（1.65x）', 'Ultra Fast (2.0x)': '极快（2.0x）',
    'Obsidian - true black (Default)': '黑曜石（纯黑）', 'Graphite - neutral Windows 11 dark': '石墨（深灰）',
    'Slate - cool blue-grey': '石板（蓝灰）', 'Nord - the Nord palette': 'Nord 配色', 'Evergreen - deep green': '常青（深绿）',
    'Espresso - warm brown': '浓咖啡（暖棕）', 'Plum - muted mauve': '梅紫', 'Porcelain - light theme': '瓷白（浅色）',
    'Custom Colors (Use Hex Below)': '自定义颜色', 'Off (solid background)': '关闭（实色背景）', 'Blur': '模糊',
    'Acrylic (frosted)': '亚克力（磨砂）', 'Auto, from album art': '自动提取专辑颜色', 'System (Device Accent)': '系统强调色',
    'Custom hex': '自定义颜色', 'Default Red': '默认红色', 'Default': '默认', 'Auto (From album art)': '自动提取专辑颜色',
    'Borderless': '无边框', 'Follow Windows locale': '跟随 Windows 区域格式', '12-hour (3:07 PM)': '12 小时制（下午 3:07）',
    '24-hour (15:07)': '24 小时制（15:07）', 'Disable disk cache': '关闭磁盘缓存',
}


def localize_settings(block):
    data = yaml.safe_load(block)
    for group in data:
        group_key = next(key for key in group if not key.startswith('$'))
        group['$name:zh-CN'] = GROUPS[group_key]
        group['$name:zh-TW'] = GROUPS[group_key]
        entries = group[group_key]
        entries[:] = [entry for entry in entries if 'Language' not in entry]
        for entry in entries:
            key = next(key for key in entry if not key.startswith('$'))
            chinese_name, chinese_description = SETTINGS[key]
            english_name, english_description = ENGLISH.get(key, (None, None))
            if english_name:
                entry['$name'] = english_name
            if english_description:
                entry['$description'] = english_description
            for locale in ('zh-CN', 'zh-TW'):
                entry['$name:' + locale] = chinese_name
                if chinese_description is not None:
                    entry['$description:' + locale] = chinese_description
            if '$options' in entry:
                for option in entry['$options']:
                    for value, label in option.items():
                        if label == '关闭磁盘缓存':
                            option[value] = 'Disable disk cache'
                translated = [{value: OPTIONS.get(label, label)} for option in entry['$options'] for value, label in option.items()]
                entry['$options:zh-CN'] = translated
                entry['$options:zh-TW'] = [dict(option) for option in translated]
    return '\n' + yaml.safe_dump(data, allow_unicode=True, sort_keys=False, width=1000) + '\n'
