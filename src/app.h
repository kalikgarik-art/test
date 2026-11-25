#pragma once
// ---------------------------------------------------------------------------
// app.h — состояние приложения (App) и общие глобалы окна.
// Реализация отрисовки интерфейса — в ui.h (App::Render и Draw* функции).
// ---------------------------------------------------------------------------
#include <string>
#include <vector>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "imgui.h"
#include "console.h"
#include "editor.h"
#include "theme.h"
#include "workspace.h"

// Прямоугольник кастомного заголовка и его кнопок в координатах клиентской
// области окна. Не используем ImRect из imgui_internal.h, чтобы приложение
// зависело только от публичного API Dear ImGui.
struct UiRect {
  ImVec2 min;
  ImVec2 max;
  UiRect() = default;
  UiRect(const ImVec2& a, const ImVec2& b) : min(a), max(b) {}
  bool Contains(const ImVec2& p) const {
    return p.x >= min.x && p.y >= min.y && p.x < max.x && p.y < max.y;
  }
};
inline UiRect g_titlebarRect;
inline std::vector<UiRect> g_titlebarButtons;

inline void ToggleMaximize(HWND hwnd) {
  if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE);
  else ShowWindow(hwnd, SW_MAXIMIZE);
}

struct App {
  Workspace ws;
  Console console;
  CodeEditor editor;

  ImFont* uiFont = nullptr;   // Segoe UI
  ImFont* monoFont = nullptr; // Consolas

  ImVec4 accent = Theme::Accent;
  float rounding = 8.0f;

  bool attached = false;      // состояние "подключения" (Attach)
  bool autoscroll = true;     // автопрокрутка консоли
  bool showConsole = true;    // видимость нижней панели
  float consoleHeight = 170.0f;
  int page = 0;               // 0 — рабочая область, 1 — настройки
  int section = 0;            // 0 — Script Hub, 1 — Editor, 2 — Console
  std::string search;         // фильтр в сайдбаре

  void Init() {
    editor.font = monoFont;
    ws.Init(console);
    console.Add(LogTag::Sys, "Mind Workspace v1.0.0 initialized");
    console.Add(LogTag::Sys, "Lua 5.4 profile selected; script execution is disabled");
    console.Add(LogTag::Net, "Script Hub: synced & ready");
  }

  void SaveActive() { ws.SaveActive(console); }

  void Render(HWND hwnd); // реализация в ui.h
};
