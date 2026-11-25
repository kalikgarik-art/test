#pragma once
// ---------------------------------------------------------------------------
// ui.h — весь интерфейс: кастомный заголовок (borderless), вкладки,
// левый сайдбар, футер с действиями, страница настроек и компоновка App::Render.
// ---------------------------------------------------------------------------
#include <algorithm>
#include <cctype>
#include <cstdio>
#include "app.h"
#include "imgui_stdlib.h"

namespace {

// --- helpers ---------------------------------------------------------------

bool MatchSearch(const std::string& hay, const std::string& needle) {
  if (needle.empty()) return true;
  std::string h = hay, n = needle;
  std::transform(h.begin(), h.end(), h.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  std::transform(n.begin(), n.end(), n.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return h.find(n) != std::string::npos;
}

void DoExecute(App& app) {
  WorkspaceDocument* f = app.ws.ActiveFile();
  if (!f) {
    app.console.Add(LogTag::Err, "no active file to preview");
    return;
  }
  int lines = 1;
  for (char c : f->buffer) if (c == '\n') lines++;
  app.console.Add(LogTag::Mind,
                  "execute requested: " + f->name + " (" + std::to_string(lines) + " lines)");
  app.console.Add(LogTag::Sys,
                  "safe mode: Lua is not evaluated; local preview status=ready");
}

void NavItem(App& app, const char* label, bool active, int action) {
  ImGui::PushStyleColor(ImGuiCol_Header,
                        active ? ImVec4(app.accent.x, app.accent.y, app.accent.z, 0.22f)
                               : ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                        ImVec4(app.accent.x, app.accent.y, app.accent.z, 0.12f));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                        ImVec4(app.accent.x, app.accent.y, app.accent.z, 0.30f));
  if (ImGui::Selectable(label, active, ImGuiSelectableFlags_None, ImVec2(-1, 30))) {
    if (action == 0) { app.page = 0; app.section = 0; app.showConsole = true; }  // Script Hub
    else if (action == 1) { app.page = 0; app.section = 1; app.showConsole = false; } // Editor
    else if (action == 2) { app.page = 0; app.section = 2; app.showConsole = true; }  // Console
    else if (action == 3) { app.page = 1; } // Settings
  }
  ImGui::PopStyleColor(3);
  if (active) {
    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRectFilled(
        mn, ImVec2(mn.x + 3, mx.y), ImGui::ColorConvertFloat4ToU32(app.accent), 1.5f);
  }
}

// Стандартная кнопка окна справа: 0 — свернуть, 1 — развернуть, 2 — закрыть.
void DrawWinButton(HWND hwnd, const char* id, int kind) {
  ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(id, ImVec2(36, 22));
  ImVec2 mn = p, mx = p + ImVec2(36, 22);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  bool hov = ImGui::IsItemHovered();
  if (kind == 2 && hov)
    dl->AddRectFilled(mn, mx, ImGui::ColorConvertFloat4ToU32(Theme::Red), 4.0f);
  else if (hov)
    dl->AddRectFilled(mn, mx, ImGui::ColorConvertFloat4ToU32(Theme::PanelLight), 4.0f);

  ImVec4 ic = (hov && kind == 2) ? ImVec4(1, 1, 1, 1) : Theme::TextMain;
  ImU32 col = ImGui::ColorConvertFloat4ToU32(ic);
  ImVec2 c = p + ImVec2(18, 11);
  if (kind == 0) {
    dl->AddLine(c + ImVec2(-5, 0), c + ImVec2(5, 0), col, 1.5f);
  } else if (kind == 1) {
    if (IsZoomed(hwnd)) {
      dl->AddRect(c + ImVec2(-4, -4), c + ImVec2(4, 4), col, 1.0f, 0, 1.5f);
      dl->AddRect(c + ImVec2(-6, -6), c + ImVec2(2, 2), col, 1.0f, 0, 1.5f);
    } else {
      dl->AddRect(c + ImVec2(-4.5f, -4.5f), c + ImVec2(4.5f, 4.5f), col, 1.0f, 0, 1.5f);
    }
  } else {
    dl->AddLine(c + ImVec2(-4.5f, -4.5f), c + ImVec2(4.5f, 4.5f), col, 1.5f);
    dl->AddLine(c + ImVec2(4.5f, -4.5f), c + ImVec2(-4.5f, 4.5f), col, 1.5f);
  }
  if (ImGui::IsItemClicked()) {
    if (kind == 0) ShowWindow(hwnd, SW_MINIMIZE);
    else if (kind == 1) ToggleMaximize(hwnd);
    else DestroyWindow(hwnd);
  }
  g_titlebarButtons.push_back(UiRect(mn, mx));
}

// --- title bar -------------------------------------------------------------

void DrawTitleBar(App& app, HWND hwnd) {
  ImGui::BeginChild("##TitleBar", ImVec2(0, 38), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  g_titlebarRect = UiRect(ImGui::GetWindowPos(),
                          ImGui::GetWindowPos() + ImGui::GetWindowSize());
  ImDrawList* dl = ImGui::GetWindowDrawList();

  // macOS-стиль: три цветных кружка слева.
  ImGui::SetCursorPos(ImVec2(12, 13));
  struct CircleBtn { const char* id; ImVec4 col; int act; const char* glyph; };
  CircleBtn cbs[3] = {
      {"##mcClose", ImVec4(1.000f, 0.373f, 0.341f, 1.0f), 0, "\xC3\x97"}, // ×
      {"##mcMin",   ImVec4(0.996f, 0.737f, 0.180f, 1.0f), 1, "\xE2\x80\x93"}, // –
      {"##mcMax",   ImVec4(0.157f, 0.784f, 0.251f, 1.0f), 2, "\xE2\x96\xA1"}, // □
  };
  for (auto& b : cbs) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(b.id, ImVec2(12, 12));
    ImVec2 c = p + ImVec2(6, 6);
    dl->AddCircleFilled(c, 6.0f, ImGui::ColorConvertFloat4ToU32(b.col));
    if (ImGui::IsItemHovered())
      dl->AddText(c + ImVec2(-2.5f, -5.0f),
                  ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.55f)), b.glyph);
    if (ImGui::IsItemClicked()) {
      if (b.act == 0) DestroyWindow(hwnd);
      else if (b.act == 1) ShowWindow(hwnd, SW_MINIMIZE);
      else ToggleMaximize(hwnd);
    }
    g_titlebarButtons.push_back(UiRect(p, p + ImVec2(12, 12)));
    ImGui::SameLine(0, 8);
  }

  ImGui::SetCursorPosY(10);
  ImGui::TextUnformatted("Mind \xE2\x80\x94 workspace"); // Mind — workspace
  ImGui::SameLine(0, 10);
  if (ImGui::SmallButton("Save")) app.SaveActive();
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save current file (Ctrl+S)");
  g_titlebarButtons.push_back(UiRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));

  // Справа: бейдж рантайма + стандартные кнопки окна.
  float rightW = 3 * 36 + 76 + 16;
  ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - rightW, 8));
  ImGui::PushStyleColor(ImGuiCol_Button, app.accent);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Lighten(app.accent, 0.15f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, Darken(app.accent, 0.15f));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
  ImGui::SmallButton("Lua 5.4");
  ImGui::PopStyleColor(4);
  g_titlebarButtons.push_back(UiRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
  ImGui::SameLine(0, 6);
  DrawWinButton(hwnd, "##wMin", 0);
  ImGui::SameLine(0, 0);
  DrawWinButton(hwnd, "##wMax", 1);
  ImGui::SameLine(0, 0);
  DrawWinButton(hwnd, "##wClose", 2);

  ImGui::EndChild();
}

// --- tabs ------------------------------------------------------------------

void DrawTabs(App& app) {
  ImGui::BeginChild("##Tabs", ImVec2(0, 38), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::SetCursorPosY(6);
  for (int i = 0; i < (int)app.ws.tabs.size(); i++) {
    WorkspaceDocument& f = app.ws.tabs[i];
    bool isActive = (i == app.ws.active);
    ImGui::PushStyleColor(ImGuiCol_Button, isActive ? Theme::PanelLight : Theme::Panel);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::PanelLight);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::PanelLight);
    std::string label = f.name + (f.dirty ? " *" : "");
    if (ImGui::Button(label.c_str(), ImVec2(0, 26))) app.ws.active = i;
    ImGui::PopStyleColor(3);
    if (isActive) {
      ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(mn.x, mx.y - 3), mx, ImGui::ColorConvertFloat4ToU32(app.accent), 2.0f);
    }
    ImGui::SameLine(0, 2);
    char closeId[64];
    std::snprintf(closeId, sizeof(closeId), "x##tab%d", i);
    if (ImGui::SmallButton(closeId)) app.ws.CloseTab(i, app.console);
    ImGui::SameLine(0, 6);
  }
  if (ImGui::SmallButton("+")) app.ws.CreateNewTab(app.console);
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("New tab (Ctrl+N)");
  ImGui::EndChild();
}

// --- sidebar ---------------------------------------------------------------

void DrawSidebar(App& app) {
  float topH = ImGui::GetContentRegionAvail().y - 40.0f;
  ImGui::BeginChild("##SideTop", ImVec2(0, topH), ImGuiChildFlags_None, ImGuiWindowFlags_None);

  ImGui::PushItemWidth(-1);
  ImGui::InputTextWithHint("##Search", "Search scripts", &app.search);
  ImGui::PopItemWidth();
  ImGui::Spacing();

  NavItem(app, "Script Hub", app.page == 0 && app.section == 0, 0);
  NavItem(app, "Editor", app.page == 0 && app.section == 1, 1);
  NavItem(app, "Console", app.page == 0 && app.section == 2, 2);
  NavItem(app, "Settings", app.page == 1, 3);
  ImGui::Spacing();

  ImGui::TextColored(Theme::TextDim, "POPULAR");
  ImGui::Separator();
  for (auto& t : Workspace::Templates()) {
    if (!MatchSearch(t.name, app.search)) continue;
    if (ImGui::Selectable(t.name, false, ImGuiSelectableFlags_None, ImVec2(-1, 28)))
      app.ws.OpenTemplate(t, app.console);
    ImGui::SameLine(ImGui::GetWindowWidth() - 52.0f);
    ImGui::TextColored(Theme::TextDim, t.count);
  }
  ImGui::Spacing();

  ImGui::TextColored(Theme::TextDim, "WORKSPACE");
  ImGui::Separator();
  for (auto& name : app.ws.files) {
    if (!MatchSearch(name, app.search)) continue;
    bool open = app.ws.IsOpen(name);
    if (ImGui::Selectable(name.c_str(), open, ImGuiSelectableFlags_None, ImVec2(-1, 28)))
      app.ws.OpenFile(name, app.console);
  }
  ImGui::EndChild();

  // Нижний статус сайдбара.
  ImGui::BeginChild("##SideStatus", ImVec2(0, 34), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::GetWindowDrawList()->AddCircleFilled(
      p + ImVec2(10, 13), 4.0f, ImGui::ColorConvertFloat4ToU32(Theme::Green));
  ImGui::SetCursorPos(ImVec2(22, 8));
  ImGui::TextUnformatted("Script Hub: synced & ready");
  ImGui::EndChild();
}

// --- footer (action bar) ---------------------------------------------------

void DrawFooter(App& app) {
  ImGui::BeginChild("##Footer", ImVec2(0, 38), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

  ImVec2 p = ImGui::GetCursorScreenPos();
  ImVec4 dot = app.attached ? Theme::Green : Theme::TextDim;
  ImGui::GetWindowDrawList()->AddCircleFilled(
      p + ImVec2(10, 13), 4.0f, ImGui::ColorConvertFloat4ToU32(dot));
  ImGui::SetCursorPos(ImVec2(22, 9));
  ImGui::TextUnformatted(app.attached ? "Attached" : "idle");

  const float bw = 90.0f;
  ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - bw * 3 - 24, 7));
  if (ImGui::Button("Clear", ImVec2(bw, 24))) {
    app.console.Clear();
    app.console.Add(LogTag::Sys, "console cleared");
  }
  ImGui::SameLine(0, 6);
  if (app.attached)
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImVec4(app.accent.x, app.accent.y, app.accent.z, 0.35f));
  if (ImGui::Button(app.attached ? "Detach" : "Attach", ImVec2(bw, 24))) {
    app.attached = !app.attached;
    if (app.attached) app.console.Add(LogTag::Net, "attached to workspace");
    else app.console.Add(LogTag::Net, "detached from workspace");
  }
  if (app.attached) ImGui::PopStyleColor();
  ImGui::SameLine(0, 6);
  ImGui::PushStyleColor(ImGuiCol_Button, app.accent);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Lighten(app.accent, 0.15f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, Darken(app.accent, 0.15f));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
  if (ImGui::Button("Execute", ImVec2(bw, 24))) DoExecute(app);
  ImGui::PopStyleColor(4);

  ImGui::EndChild();
}

// --- settings --------------------------------------------------------------

void DrawSettings(App& app) {
  ImGui::BeginChild("##Settings", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_None);
  ImGui::Text("Settings");
  ImGui::Separator();
  ImGui::ColorEdit3("Accent color", (float*)&app.accent);
  ImGui::SliderFloat("Window rounding", &app.rounding, 0.0f, 16.0f, "%.1f");
  ImGui::Checkbox("Console autoscroll", &app.autoscroll);
  ImGui::Checkbox("Lua syntax highlighting", &app.editor.highlightEnabled);
  ImGui::Separator();
  ImGui::TextWrapped(
      "Mind Workspace 1.0.0 - desktop script editor. Lua 5.4 profile badge; "
      "scripts are not evaluated in this safe-mode build. Files are stored in ./workspace next to the executable.");
  ImGui::EndChild();
  ApplyTheme(app.accent, app.rounding);
}

} // namespace

// --- компоновка всего окна -------------------------------------------------

inline void App::Render(HWND hwnd) {
  g_titlebarButtons.clear();

  ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->Pos);
  ImGui::SetNextWindowSize(vp->Size);
  ImGui::Begin("##Main", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);

  DrawTitleBar(*this, hwnd);
  DrawTabs(*this);

  float midH = ImGui::GetContentRegionAvail().y - 44.0f; // минус футер и отступы
  ImGui::BeginChild("##Sidebar", ImVec2(264, midH), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  DrawSidebar(*this);
  ImGui::EndChild();
  ImGui::SameLine(0, 6);

  ImGui::BeginChild("##Center", ImVec2(0, midH), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  if (page == 0) {
    float ch = showConsole ? consoleHeight : 0.0f;
    float eh = ImGui::GetContentRegionAvail().y - ch - (showConsole ? 18.0f : 0.0f);
    WorkspaceDocument* f = ws.ActiveFile();
    if (f) {
      ImGui::PushFont(monoFont ? monoFont : uiFont);
      bool edited = editor.Render("##Code", f->buffer, ImVec2(0, eh));
      ImGui::PopFont();
      if (edited) f->dirty = true;
    }
    if (showConsole) {
      // Разделитель редактора и консоли (перетаскивается мышью).
      ImGui::InvisibleButton("##Splitter", ImVec2(-1, 6));
      if (ImGui::IsItemActive()) {
        consoleHeight -= ImGui::GetIO().MouseDelta.y;
        consoleHeight = std::clamp(consoleHeight, 80.0f, 700.0f);
      }
      ImVec2 smn = ImGui::GetItemRectMin(), smx = ImGui::GetItemRectMax();
      bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
      ImGui::GetWindowDrawList()->AddLine(
          ImVec2(smn.x, (smn.y + smx.y) * 0.5f), ImVec2(smx.x, (smn.y + smx.y) * 0.5f),
          ImGui::ColorConvertFloat4ToU32(hot ? accent : Theme::Border), 1.0f);
      console.Render(autoscroll, consoleHeight);
    }
  } else {
    DrawSettings(*this);
  }
  ImGui::EndChild();

  DrawFooter(*this);

  // Проверяем сочетания напрямую: Ctrl+S должен срабатывать и тогда, когда
  // фокус находится внутри многострочного поля редактора.
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) SaveActive();
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) ws.CreateNewTab(console);
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_W) && ws.active >= 0)
    ws.CloseTab(ws.active, console);

  ImGui::End();
}
