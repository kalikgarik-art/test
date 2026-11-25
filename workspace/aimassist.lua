-- Mind Workspace | Aim Assist
local Players = game:GetService("Players")
local UserInputService = game:GetService("UserInputService")
local RunService = game:GetService("RunService")

local LocalPlayer = Players.LocalPlayer
local Camera = workspace.CurrentCamera

local Settings = {
    Enabled = true,
    FOV = 120,
    Smoothness = 0.15,
    TargetPart = "Head",
    TeamCheck = true,
}

local function getClosestTarget()
    local closest, closestDist = nil, Settings.FOV
    for _, player in ipairs(Players:GetPlayers()) do
        if player ~= LocalPlayer and player.Team ~= LocalPlayer.Team then
            local character = player.Character
            local part = character and character:FindFirstChild(Settings.TargetPart)
            if part then
                local screenPos, visible = Camera:WorldToViewportPoint(part.Position)
                if visible then
                    local dist = (Vector2.new(screenPos.X, screenPos.Y) - Vector2.new(Camera.ViewportSize.X / 2, Camera.ViewportSize.Y / 2)).Magnitude
                    if dist < closestDist then
                        closest, closestDist = player, dist
                    end
                end
            end
        end
    end
    return closest
end

RunService.RenderStepped:Connect(function()
    if not Settings.Enabled then return end
    local target = getClosestTarget()
    if target and target.Character then
        local part = target.Character:FindFirstChild(Settings.TargetPart)
        if part then
            local goal = Camera:WorldToViewportPoint(part.Position)
            local current = Vector2.new(Camera.ViewportSize.X / 2, Camera.ViewportSize.Y / 2)
            local delta = Vector2.new(goal.X, goal.Y) - current
            UserInputService:SendMouseMotion(delta.X * Settings.Smoothness, delta.Y * Settings.Smoothness)
        end
    end
end)

print("Aim Assist ready")
