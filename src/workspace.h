#pragma once
// ---------------------------------------------------------------------------
// workspace.h — работа с файловой системой (<filesystem>, C++17).
// Папка ./workspace рядом с .exe создаётся автоматически при первом запуске,
// при необходимости в неё кладутся тестовые .lua файлы.
// Также здесь: вкладки открытых файлов, сохранение (Ctrl+S) и шаблоны POPULAR.
// ---------------------------------------------------------------------------
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "console.h"

namespace fs = std::filesystem;

struct WorkspaceDocument {
  std::string name;   // отображаемое имя вкладки
  fs::path path;      // путь на диске (пустой для несохранённых)
  std::string buffer; // содержимое редактора
  bool dirty = false; // есть несохранённые изменения
  bool isNew = false; // вкладка ещё не сохранена на диск
};

struct Template {
  const char* name;
  const char* count;
  const char* code;
};

class Workspace {
public:
  fs::path dir;                    // ./workspace рядом с .exe
  std::vector<WorkspaceDocument> tabs;      // открытые вкладки
  std::vector<std::string> files;  // .lua файлы из папки workspace
  int active = 0;                  // индекс активной вкладки

  void Init(Console& console) {
    wchar_t buf[32768];
    DWORD len = GetModuleFileNameW(nullptr, buf, 32768);
    fs::path exeDir = (len > 0 && len < 32768) ? fs::path(buf).parent_path() : fs::path(L".");
    dir = exeDir / "workspace";

    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) console.Add(LogTag::Err, "cannot create workspace: " + ec.message());
    RefreshFileList();
    if (files.empty()) {
      WriteDefaultFiles(console);
      RefreshFileList();
    }
    for (const auto& f : files) OpenFile(f, console);
    if (tabs.empty()) CreateNewTab(console);
    active = 0;
    console.Add(LogTag::Sys,
                "workspace ready: " + dir.u8string() + " (" +
                    std::to_string(files.size()) + " files)");
  }

  void RefreshFileList() {
    files.clear();
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
      if (e.is_regular_file(ec)) {
        std::string ext = e.path().extension().u8string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (ext == ".lua") files.push_back(e.path().filename().u8string());
      }
    }
    std::sort(files.begin(), files.end());
  }

  void OpenFile(const std::string& name, Console& console) {
    for (int i = 0; i < (int)tabs.size(); i++) {
      if (tabs[i].name == name) { active = i; return; }
    }
    WorkspaceDocument f;
    f.name = name;
    f.path = dir / fs::u8path(name);
    f.buffer = ReadAll(f.path);
    f.dirty = false;
    f.isNew = false;
    tabs.push_back(std::move(f));
    active = (int)tabs.size() - 1;
    console.Add(LogTag::Sys, "opened " + name);
  }

  void CreateNewTab(Console& console) {
    WorkspaceDocument f;
    std::error_code ec;
    do {
      f.name = "untitled_" + std::to_string(newCounter_++) + ".lua";
      ec.clear();
    } while (IsOpen(f.name) || fs::exists(dir / fs::u8path(f.name), ec));
    f.buffer = "-- New script\n-- Mind Workspace\n\n";
    f.dirty = true;
    f.isNew = true;
    tabs.push_back(std::move(f));
    active = (int)tabs.size() - 1;
    console.Add(LogTag::Sys, "new tab: " + tabs.back().name);
  }

  void CloseTab(int idx, Console& console) {
    if (idx < 0 || idx >= (int)tabs.size()) return;
    std::string name = tabs[idx].name;
    tabs.erase(tabs.begin() + idx);
    if (tabs.empty()) { CreateNewTab(console); return; }
    if (active >= (int)tabs.size()) active = (int)tabs.size() - 1;
    else if (active > idx) active--;
    console.Add(LogTag::Sys, "closed " + name);
  }

  void SaveActive(Console& console) {
    WorkspaceDocument* f = ActiveFile();
    if (!f) return;
    fs::path p = f->isNew ? (dir / fs::u8path(f->name)) : f->path;
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) {
      console.Add(LogTag::Err, "failed to save " + f->name);
      return;
    }
    out << f->buffer;
    f->path = p;
    f->isNew = false;
    f->dirty = false;
    RefreshFileList();
    console.Add(LogTag::Sys, "saved " + f->name + " -> ./workspace");
  }

  void OpenTemplate(const Template& t, Console& console) {
    const std::string baseName = std::string(t.name) + ".lua";
    const std::string stem = t.name;
    std::string name = baseName;
    for (int suffix = 0;; ++suffix) {
      for (int i = 0; i < (int)tabs.size(); i++) {
        if (tabs[i].name == name) {
          active = i;
          console.Add(LogTag::Mind, "template already open: " + name);
          return;
        }
      }
      std::error_code ec;
      bool existsOnDisk = fs::exists(dir / fs::u8path(name), ec);
      if (ec || !existsOnDisk) break;
      name = suffix == 0 ? stem + "_template.lua"
                         : stem + "_template_" + std::to_string(suffix) + ".lua";
    }
    WorkspaceDocument f;
    f.name = name;
    f.buffer = t.code;
    f.dirty = true;
    f.isNew = true;
    tabs.push_back(std::move(f));
    active = (int)tabs.size() - 1;
    console.Add(LogTag::Mind,
                "loaded template " + name + " (" + std::string(t.count) + " uses)");
  }

  bool IsOpen(const std::string& name) const {
    for (const auto& t : tabs) if (t.name == name) return true;
    return false;
  }

  WorkspaceDocument* ActiveFile() {
    if (tabs.empty() || active < 0 || active >= (int)tabs.size()) return nullptr;
    return &tabs[active];
  }

  static const std::vector<Template>& Templates() {
    static const std::vector<Template> kTemplates = {
        {"Universal Hub", "12.4K", R"Lua(-- Universal Hub | Mind Workspace
-- One hub for every game

local Hub = {}
Hub.Version = "3.2.1"
Hub.Games = {
    BloxFruits = true,
    Doors = true,
    PetSimulator = true,
}

local function notify(text)
    print("[Universal Hub]", text)
end

function Hub:Init()
    notify("loading modules...")
    for gameName, enabled in pairs(self.Games) do
        if enabled then
            notify("module ready: " .. gameName)
        end
    end
    notify("done in 0.42s")
end

Hub:Init()
return Hub
)Lua"},
        {"Infinite Yield", "8.1K", R"Lua(-- Infinite Yield | Admin Commands
-- Press ; to open the command bar

local Commands = {}
local Prefix = ";"

local function register(name, callback)
    Commands[name] = callback
    print("[Infinite Yield] registered:", name)
end

register("speed", function(value)
    game.Players.LocalPlayer.Character.Humanoid.WalkSpeed = tonumber(value) or 16
end)

register("jump", function(value)
    game.Players.LocalPlayer.Character.Humanoid.JumpPower = tonumber(value) or 50
end)

register("fly", function(enabled)
    print("fly:", enabled)
end)

print("Infinite Yield loaded - type ;help")
)Lua"},
        {"Owl Hub", "5.7K", R"Lua(-- Owl Hub | Visuals & Utility
local Owl = { Enabled = true, Theme = "Night" }

local function applyTheme()
    local gui = game:GetService("CoreGui")
    for _, v in ipairs(gui:GetChildren()) do
        if v:IsA("ScreenGui") then
            v.DisplayOrder = 10
        end
    end
    print("[Owl Hub] theme:", Owl.Theme)
end

local function toggle()
    Owl.Enabled = not Owl.Enabled
    print("[Owl Hub] enabled:", Owl.Enabled)
end

applyTheme()
return Owl
)Lua"},
        {"Speed Run X", "3.2K", R"Lua(-- Speed Run X | Movement Pack
local SpeedRun = {
    AutoJump = true,
    Slide = false,
    SpeedBoost = 1.5,
}

local Players = game:GetService("Players")
local LocalPlayer = Players.LocalPlayer

local function getHumanoid()
    local char = LocalPlayer.Character
    return char and char:FindFirstChildOfClass("Humanoid")
end

local function boost()
    local hum = getHumanoid()
    if hum then
        hum.WalkSpeed = 16 * SpeedRun.SpeedBoost
        print("[Speed Run X] boosted to", hum.WalkSpeed)
    end
end

boost()
return SpeedRun
)Lua"},
    };
    return kTemplates;
  }

private:
  int newCounter_ = 1;

  static std::string ReadAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return "";
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end()); // нормализуем CRLF -> LF
    return s;
  }

  void WriteDefaultFiles(Console& console) {
    const char* esp = R"Lua(-- Mind Workspace | ESP Module
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
)Lua";
    const char* aim = R"Lua(-- Mind Workspace | Aim Assist
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
)Lua";
    const char* theme = R"Lua(-- Mind Workspace | UI Theme
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
)Lua";
    auto write = [&](const char* name, const char* code) {
      std::ofstream out(dir / fs::u8path(name), std::ios::binary | std::ios::trunc);
      if (!out) {
        console.Add(LogTag::Err, std::string("cannot create default file ") + name);
        return;
      }
      out << code;
      if (out) console.Add(LogTag::Sys, std::string("created default file ") + name);
      else console.Add(LogTag::Err, std::string("failed writing default file ") + name);
    };
    write("esp.lua", esp);
    write("aimassist.lua", aim);
    write("ui_theme.lua", theme);
  }
};
