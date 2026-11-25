-- Mind Workspace | UI Theme
local Theme = {}

Theme.Colors = {
    Background = Color3.fromRGB(13, 14, 20),
    Panel = Color3.fromRGB(19, 21, 31),
    Accent = Color3.fromRGB(99, 91, 255),
    AccentAlt = Color3.fromRGB(139, 92, 246),
    Text = Color3.fromRGB(232, 234, 241),
    TextDim = Color3.fromRGB(144, 148, 169),
    Success = Color3.fromRGB(59, 210, 122),
    Danger = Color3.fromRGB(239, 68, 68),
}

Theme.Rounding = {
    Window = 8,
    Button = 6,
    Input = 6,
}

Theme.Fonts = {
    Main = Enum.Font.Gotham,
    Mono = Enum.Font.Code,
}

function Theme.Apply(gui)
    gui.BackgroundColor3 = Theme.Colors.Background
    for _, frame in ipairs(gui:GetDescendants()) do
        if frame:IsA("Frame") or frame:IsA("TextButton") then
            frame.BackgroundColor3 = Theme.Colors.Panel
            frame.BorderSizePixel = 0
        elseif frame:IsA("TextLabel") or frame:IsA("TextButton") then
            frame.TextColor3 = Theme.Colors.Text
            frame.Font = Theme.Fonts.Main
        end
    end
    print("Theme applied:", #gui:GetDescendants(), "elements")
end

return Theme
