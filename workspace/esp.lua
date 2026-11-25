-- Mind Workspace | ESP Module
-- Toggle with RightShift

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local LocalPlayer = Players.LocalPlayer

local ESP = {
    Enabled = true,
    TeamCheck = true,
    MaxDistance = 1000,
    Boxes = true,
    Names = true,
    Tracers = false,
}

local function createBox(target)
    local box = Instance.new("Frame")
    box.Name = "ESPBox"
    box.Size = UDim2.new(0, 40, 0, 60)
    box.BackgroundTransparency = 0.7
    box.BorderSizePixel = 1
    box.Parent = target
    return box
end

local function updateESP()
    for _, player in ipairs(Players:GetPlayers()) do
        if player ~= LocalPlayer then
            local character = player.Character
            if character and character:FindFirstChild("HumanoidRootPart") then
                local hrp = character.HumanoidRootPart
                local distance = (LocalPlayer.Character.HumanoidRootPart.Position - hrp.Position).Magnitude
                if distance <= ESP.MaxDistance then
                    print("[ESP]", player.Name, math.floor(distance))
                end
            end
        end
    end
end

RunService.RenderStepped:Connect(updateESP)
print("ESP loaded successfully")
