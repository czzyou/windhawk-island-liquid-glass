"""Alt-left-drag positioning shared by the window, painter and hidden hover target."""


def apply_island_drag(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:90], src.count(old))
        src = src.replace(old, new, 1)

    change('int YasbDockY(int screenTop) {', r'''std::atomic<bool> g_islandDragging{false};
std::atomic<bool> g_islandManuallyPlaced{false};
std::atomic<int> g_islandDragX{0}, g_islandDragY{0};
POINT g_islandDragStart{}, g_islandDragOrigin{};

constexpr POINT DraggedIslandOffset(POINT origin, POINT start, POINT cursor) {
    return {origin.x + cursor.x - start.x, origin.y + cursor.y - start.y};
}
static_assert(DraggedIslandOffset({10, -20}, {100, 100}, {90, 130}).x == 0);
static_assert(DraggedIslandOffset({10, -20}, {100, 100}, {90, 130}).y == 10);
static_assert(DraggedIslandOffset({-500, 40}, {-300, 200}, {-300, 200}).x == -500);

void FinishIslandDrag() {
    if (!g_islandDragging.exchange(false)) return;
    Wh_SetIntValue(L"IslandDragX", g_islandDragX.load());
    Wh_SetIntValue(L"IslandDragY", g_islandDragY.load());
    Wh_SetIntValue(L"IslandManuallyPlaced", 1);
    g_layoutDirty = true;
}

int YasbDockY(int screenTop) {''')
    change('    SetWindowPos(hwnd, zOrder, x, y, width, height,',
           '    x += g_islandDragX.load();\n    y += g_islandDragY.load();\n    SetWindowPos(hwnd, zOrder, x, y, width, height,')
    change('''    RECT r;
    r.left = x;
    r.right = x + dockWidth;''', '''    x += g_islandDragX.load();
    y += g_islandDragY.load();
    RECT r;
    r.left = x;
    r.right = x + dockWidth;''')
    change('''                    (work.left + work.right - width) * 0.5f + settings.offsetX + shift);
                left = screenLeft - window.left;
                top = monitor.rcMonitor.top + 8 + g_compactInsetY.load() - window.top + nudge;''', '''                    (work.left + work.right - width) * 0.5f + settings.offsetX + shift) + g_islandDragX.load();
                left = screenLeft - window.left;
                top = monitor.rcMonitor.top + 8 + g_compactInsetY.load() - window.top + nudge + g_islandDragY.load();''')
    change('            SetTimer(hwnd, 0x449, 45000, nullptr);', '''            g_islandManuallyPlaced = Wh_GetIntValue(L"IslandManuallyPlaced", 0) != 0;
            g_islandDragX = g_islandManuallyPlaced ? Wh_GetIntValue(L"IslandDragX", 0) : 0;
            g_islandDragY = g_islandManuallyPlaced ? Wh_GetIntValue(L"IslandDragY", 0) : 0;
            SetTimer(hwnd, 0x449, 45000, nullptr);''')
    change('''        case WM_LBUTTONDOWN:
            {''', '''        case WM_LBUTTONDOWN:
            {
                if ((GetAsyncKeyState(VK_MENU) & 0x8000) && !Wh_GetIntValue(L"UpdateRequested", 0)) {
                    if (!GetCursorPos(&g_islandDragStart)) return 0;
                    const int dockShift = static_cast<int>(std::lround(CaptionDockShift(
                        g_captionDockAnchor.load(), g_captionDockBlend.load())));
                    g_islandDragOrigin = {g_islandDragX.load() + dockShift, g_islandDragY.load()};
                    g_islandDragX = g_islandDragOrigin.x;
                    g_islandManuallyPlaced = true;
                    g_islandDragging = true;
                    SetCapture(hwnd);
                    g_layoutDirty = true;
                    return 0;
                }''')
    change('''        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT''', '''        case WM_MOUSEMOVE: {
            if (g_islandDragging.load()) {
                POINT cursor{};
                if (GetCursorPos(&cursor)) {
                    const auto offset = DraggedIslandOffset(g_islandDragOrigin, g_islandDragStart, cursor);
                    g_islandDragX = offset.x;
                    g_islandDragY = offset.y;
                    g_layoutDirty = true;
                }
                return 0;
            }
            TRACKMOUSEEVENT''')
    change('''        case WM_LBUTTONUP:
            {''', '''        case WM_LBUTTONUP:
            {
                if (g_islandDragging.load()) {
                    FinishIslandDrag();
                    ReleaseCapture();
                    return 0;
                }''')
    change('        case WM_CAPTURECHANGED:\n', '        case WM_CAPTURECHANGED:\n            FinishIslandDrag();\n')
    change('        case WM_DESTROY:\n', '''        case WM_CANCELMODE:
            if (g_islandDragging.load()) {
                FinishIslandDrag();
                ReleaseCapture();
            }
            return 0;
        case WM_DESTROY:
            FinishIslandDrag();
''')
    change('        const bool ctrlHoverCT = hover && ctrlHeld;', '''        const bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        const bool ctrlHoverCT = hover && ctrlHeld && !altHeld && !g_islandDragging.load();''')
    change('        SetClickThrough(hwnd, (primary.kind == IslandKind::Idle && !draggingOrHover && !pinned) || ctrlHoverCT);',
           '        SetClickThrough(hwnd, !g_islandDragging.load() && !altHeld && ((primary.kind == IslandKind::Idle && !draggingOrHover && !pinned) || ctrlHoverCT));')
    change('''        GUITHREADINFO dockGui{sizeof(GUITHREADINFO)};''', '''        if (g_islandManuallyPlaced.load()) {
            // Manual placement owns the position; automatic title-bar docking must not pull it back.
            dockAnchor = {};
            dockBlend = {};
        }
        GUITHREADINFO dockGui{sizeof(GUITHREADINFO)};''')
    change('        const bool dockPaused = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ||',
           '        const bool dockPaused = g_islandManuallyPlaced.load() || (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ||')
    change('            const bool busy = snapshot.timer.active || g_importantDaysOpen.load();',
           '            const bool busy = snapshot.timer.active || g_importantDaysOpen.load() || g_islandDragging.load();')
    return src
