// LiquidGlass2 BackgroundFill stroke, matching the user's installed Taskbar Styler theme.
std::array<D2D1_GRADIENT_STOP, 7> LiquidGlassRimStops() {
    return {{
        {0.00f, D2D1::ColorF(0xE0E0E0, 0x69 / 255.0f)},
        {0.10f, D2D1::ColorF(0x707070, 0x70 / 255.0f)},
        {0.25f, D2D1::ColorF(0x505050, 0x70 / 255.0f)},
        {0.50f, D2D1::ColorF(0x292929, 0x70 / 255.0f)},
        {0.75f, D2D1::ColorF(0x505050, 0x70 / 255.0f)},
        {0.90f, D2D1::ColorF(0x707070, 0x70 / 255.0f)},
        {1.00f, D2D1::ColorF(0xD0D0D0, 0x65 / 255.0f)},
    }};
}
