#pragma once
// ---------------------------------------------------------------------------
// ui.h — кастомный UI Mind Workspace: title bar, вкладки, сайдбар,
// редактор/консоль и анимированный action footer.
// Все интерактивные поверхности рисуются через ImGui draw lists; стандартные
// кнопочные заливки используются только на странице настроек.
// ---------------------------------------------------------------------------
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "app.h"
#include "imgui_internal.h" // ImLerp — плавные переходы состояния интерфейса
#include "imgui_stdlib.h"

namespace MindUi {

// ---------- palette/animation helpers --------------------------------------

inline ImU32 Pack(const ImVec4& c) {
  return ImGui::ColorConvertFloat4ToU32(c);
}

inline bool MatchSearch(const std::string& text, const std::string& query) {
  if (query.empty()) return true;
  std::string haystack = text;
  std::string needle = query;
  std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  std::transform(needle.begin(), needle.end(), needle.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return haystack.find(needle) != std::string::npos;
}

inline ImFont* UiFont(const App& app) {
  return app.uiFont ? app.uiFont : ImGui::GetFont();
}

inline ImFont* UiBoldFont(const App& app) {
  return app.uiBoldFont ? app.uiBoldFont : UiFont(app);
}

inline float UiFontSize(const App& app) {
  return UiFont(app)->FontSize;
}

inline float TextWidth(ImFont* font, const char* text) {
  if (!font || !text) return 0.0f;
  return font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text).x;
}

inline std::string FitUtf8Text(ImFont* font, const char* text, float maxWidth) {
  if (!text || maxWidth <= 0.0f) return "";
  std::string result(text);
  if (TextWidth(font, result.c_str()) <= maxWidth) return result;
  constexpr const char* ellipsis = "...";
  while (!result.empty() && TextWidth(font, (result + ellipsis).c_str()) > maxWidth) {
    size_t cut = result.size() - 1;
    while (cut > 0 && (static_cast<unsigned char>(result[cut]) & 0xC0u) == 0x80u) --cut;
    result.erase(cut);
  }
  return result.empty() ? std::string(ellipsis) : result + ellipsis;
}

inline void AddText(ImDrawList* draw, ImFont* font, float size,
                    const ImVec2& pos, ImU32 color, const char* text) {
  if (draw && font && text)
    draw->AddText(font, size, pos, color, text);
}

inline float Animate(App& app, ImGuiID id, float target) {
  float& current = app.animState[id];
  const float factor = ImClamp(ImGui::GetIO().DeltaTime * 12.0f, 0.0f, 1.0f);
  current = ImLerp(current, target, factor);
  if (std::fabs(current - target) < 0.001f) current = target;
  return current;
}

inline float ItemAnimation(App& app, ImGuiID id, bool hovered, bool selected,
                           bool pressed = false) {
  const float target = pressed ? 0.82f : (selected ? 1.0f : (hovered ? 0.50f : 0.0f));
  return Animate(app, id, target);
}

inline void AddSectionLabel(const char* label) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float width = ImGui::GetContentRegionAvail().x;
  const float lineH = ImGui::GetTextLineHeight();
  ImGui::TextColored(Theme::TextDim, "%s", label);
  const ImVec2 textEnd = ImGui::GetItemRectMax();
  const float y = p.y + lineH * 0.60f;
  const float x1 = textEnd.x + 10.0f;
  const float x2 = p.x + width;
  if (x2 > x1)
    draw->AddLine(ImVec2(x1, y), ImVec2(x2, y), Pack(Theme::Border), 1.0f);
}

inline void AddSearchIcon(ImDrawList* draw, const ImVec2& center, ImU32 color) {
  const ImVec2 lens(center.x - 2.0f, center.y - 2.0f);
  draw->AddCircle(lens, 5.0f, color, 20, 1.7f);
  draw->AddLine(ImVec2(lens.x + 3.6f, lens.y + 3.6f),
                ImVec2(lens.x + 7.3f, lens.y + 7.3f), color, 1.7f);
}

inline void AddFolderIcon(ImDrawList* draw, const ImVec2& p, ImU32 color) {
  const ImVec2 a(p.x + 1.0f, p.y + 6.0f);
  const ImVec2 b(p.x + 7.0f, p.y + 6.0f);
  const ImVec2 c(p.x + 9.0f, p.y + 3.5f);
  const ImVec2 d(p.x + 17.0f, p.y + 3.5f);
  const ImVec2 e(p.x + 19.0f, p.y + 6.0f);
  const ImVec2 f(p.x + 21.0f, p.y + 6.0f);
  const ImVec2 g(p.x + 20.0f, p.y + 17.5f);
  const ImVec2 h(p.x + 1.0f, p.y + 17.5f);
  draw->AddLine(a, b, color, 1.6f);
  draw->AddLine(b, c, color, 1.6f);
  draw->AddLine(c, d, color, 1.6f);
  draw->AddLine(d, e, color, 1.6f);
  draw->AddLine(e, f, color, 1.6f);
  draw->AddLine(f, g, color, 1.6f);
  draw->AddLine(g, h, color, 1.6f);
  draw->AddLine(h, a, color, 1.6f);
}

inline void AddSaveIcon(ImDrawList* draw, const ImVec2& p, ImU32 color) {
  draw->AddRect(ImVec2(p.x + 3, p.y + 2), ImVec2(p.x + 19, p.y + 20), color, 2.0f, 0, 1.5f);
  draw->AddRect(ImVec2(p.x + 6, p.y + 3), ImVec2(p.x + 16, p.y + 9), color, 1.0f, 0, 1.3f);
  draw->AddRect(ImVec2(p.x + 7, p.y + 13), ImVec2(p.x + 15, p.y + 19), color, 1.0f, 0, 1.3f);
}

inline void AddPlayIcon(ImDrawList* draw, const ImVec2& p, ImU32 color, float scale = 1.0f) {
  draw->AddTriangleFilled(
      ImVec2(p.x, p.y - 7.0f * scale),
      ImVec2(p.x + 11.0f * scale, p.y),
      ImVec2(p.x, p.y + 7.0f * scale), color);
}

inline void AddLinkIcon(ImDrawList* draw, const ImVec2& p, ImU32 color) {
  // Две лаконичные петли с короткими соединительными штрихами.
  draw->AddRect(ImVec2(p.x + 1.0f, p.y + 5.0f), ImVec2(p.x + 12.0f, p.y + 13.0f),
                color, 4.0f, 0, 1.7f);
  draw->AddRect(ImVec2(p.x + 9.0f, p.y + 5.0f), ImVec2(p.x + 20.0f, p.y + 13.0f),
                color, 4.0f, 0, 1.7f);
  draw->AddLine(ImVec2(p.x + 7.0f, p.y + 9.0f), ImVec2(p.x + 14.0f, p.y + 9.0f),
                color, 1.7f);
}

inline void AddClearIcon(ImDrawList* draw, const ImVec2& p, ImU32 color) {
  draw->AddLine(ImVec2(p.x + 4, p.y + 6), ImVec2(p.x + 18, p.y + 6), color, 1.6f);
  draw->AddLine(ImVec2(p.x + 7, p.y + 4), ImVec2(p.x + 15, p.y + 4), color, 1.6f);
  draw->AddLine(ImVec2(p.x + 6, p.y + 8), ImVec2(p.x + 7, p.y + 18), color, 1.6f);
  draw->AddLine(ImVec2(p.x + 16, p.y + 8), ImVec2(p.x + 15, p.y + 18), color, 1.6f);
  draw->AddLine(ImVec2(p.x + 7, p.y + 18), ImVec2(p.x + 15, p.y + 18), color, 1.6f);
  draw->AddLine(ImVec2(p.x + 10, p.y + 9), ImVec2(p.x + 10, p.y + 16), color, 1.2f);
  draw->AddLine(ImVec2(p.x + 12, p.y + 9), ImVec2(p.x + 12, p.y + 16), color, 1.2f);
}

inline void AddRoundedGradient(ImDrawList* draw, const ImVec2& min,
                               const ImVec2& max, const ImVec4& left,
                               const ImVec4& right, float rounding,
                               const ImVec4& maskColor = Theme::Panel) {
  // Dear ImGui's multi-colour rectangle is the gradient base. Four small
  // concave masks carve rounded corners matching the footer's panel color.
  draw->AddRectFilledMultiColor(min, max, Pack(left), Pack(right), Pack(right), Pack(left));

  const float r = std::min(rounding, std::min((max.x - min.x) * 0.5f,
                                               (max.y - min.y) * 0.5f));
  if (r > 0.5f) {
    constexpr int kArcSegments = 6;
    auto maskCorner = [&](const ImVec2& center, float start, float finish,
                          const ImVec2& outer, const ImVec2& edgeStart,
                          const ImVec2& edgeEnd) {
      ImVec2 points[10];
      int count = 0;
      points[count++] = outer;
      points[count++] = edgeStart;
      for (int i = 1; i < kArcSegments; ++i) {
        const float t = (float)i / (float)kArcSegments;
        const float angle = start + (finish - start) * t;
        points[count++] = ImVec2(center.x + std::cos(angle) * r,
                                 center.y + std::sin(angle) * r);
      }
      points[count++] = edgeEnd;
      draw->AddConcavePolyFilled(points, count, Pack(maskColor));
    };

    const float pi = 3.14159265358979323846f;
    maskCorner(ImVec2(min.x + r, min.y + r), -pi * 0.5f, -pi,
               min, ImVec2(min.x + r, min.y), ImVec2(min.x, min.y + r));
    maskCorner(ImVec2(max.x - r, min.y + r), 0.0f, -pi * 0.5f,
               ImVec2(max.x, min.y), ImVec2(max.x, min.y + r), ImVec2(max.x - r, min.y));
    maskCorner(ImVec2(max.x - r, max.y - r), pi * 0.5f, 0.0f,
               max, ImVec2(max.x - r, max.y), ImVec2(max.x, max.y - r));
    maskCorner(ImVec2(min.x + r, max.y - r), pi, pi * 0.5f,
               ImVec2(min.x, max.y), ImVec2(min.x, max.y - r), ImVec2(min.x + r, max.y));
  }
}

// ---------- title bar -------------------------------------------------------

inline void DrawMacDot(HWND hwnd, const char* id,
                       const ImVec2& min, const ImVec4& color, int action) {
  ImGui::SetCursorScreenPos(min);
  ImGui::InvisibleButton(id, ImVec2(18.0f, 18.0f));
  const bool hovered = ImGui::IsItemHovered();
  const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  const ImVec2 center(min.x + 9.0f, min.y + 9.0f);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddCircleFilled(center, 5.6f, Pack(color), 24);
  if (hovered)
    draw->AddCircle(center, 6.3f, Pack(ImVec4(1.0f, 1.0f, 1.0f, 0.34f)), 24, 1.0f);
  g_titlebarButtons.emplace_back(min, ImVec2(min.x + 18.0f, min.y + 18.0f));
  if (clicked) {
    if (action == 0) DestroyWindow(hwnd);
    else if (action == 1) ShowWindow(hwnd, SW_MINIMIZE);
    else ToggleMaximize(hwnd);
  }
}

inline void DrawWindowButton(HWND hwnd, const char* id, const ImVec2& min,
                             int kind) {
  constexpr ImVec2 size(42.0f, 36.0f);
  ImGui::SetCursorScreenPos(min);
  ImGui::InvisibleButton(id, size);
  const bool hovered = ImGui::IsItemHovered();
  const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  const ImVec2 max(min.x + size.x, min.y + size.y);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  if (hovered) {
    const ImVec4 bg = (kind == 2) ? WithAlpha(Theme::Red, 0.85f)
                                  : WithAlpha(Theme::PanelLight, 0.95f);
    draw->AddRectFilled(min, max, Pack(bg), 7.0f);
  }
  const ImU32 icon = Pack((kind == 2 && hovered) ? Theme::TextMain : Theme::TextDim);
  const ImVec2 c(min.x + size.x * 0.5f, min.y + size.y * 0.5f);
  if (kind == 0) {
    draw->AddLine(ImVec2(c.x - 5.0f, c.y + 2.0f), ImVec2(c.x + 5.0f, c.y + 2.0f), icon, 1.6f);
  } else if (kind == 1) {
    if (IsZoomed(hwnd)) {
      draw->AddRect(ImVec2(c.x - 3.0f, c.y - 5.0f), ImVec2(c.x + 5.0f, c.y + 3.0f), icon, 1.2f, 0, 1.5f);
      draw->AddRect(ImVec2(c.x - 5.0f, c.y - 3.0f), ImVec2(c.x + 3.0f, c.y + 5.0f), icon, 1.2f, 0, 1.5f);
    } else {
      draw->AddRect(ImVec2(c.x - 5.0f, c.y - 5.0f), ImVec2(c.x + 5.0f, c.y + 5.0f), icon, 1.4f, 0, 1.5f);
    }
  } else {
    draw->AddLine(ImVec2(c.x - 4.5f, c.y - 4.5f), ImVec2(c.x + 4.5f, c.y + 4.5f), icon, 1.6f);
    draw->AddLine(ImVec2(c.x + 4.5f, c.y - 4.5f), ImVec2(c.x - 4.5f, c.y + 4.5f), icon, 1.6f);
  }
  g_titlebarButtons.emplace_back(min, max);
  if (clicked) {
    if (kind == 0) ShowWindow(hwnd, SW_MINIMIZE);
    else if (kind == 1) ToggleMaximize(hwnd);
    else DestroyWindow(hwnd);
  }
}

inline void DrawTitleBar(App& app, HWND hwnd) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Panel);
  ImGui::BeginChild("##TitleBar", ImVec2(0.0f, 48.0f), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleColor();

  const ImVec2 pos = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  const ImVec2 end(pos.x + size.x, pos.y + size.y);
  g_titlebarRect = UiRect(pos, end);
  ImDrawList* draw = ImGui::GetWindowDrawList();

  const ImVec4 dots[3] = {
      ImVec4(1.0f, 0.37f, 0.34f, 1.0f),
      ImVec4(1.0f, 0.74f, 0.18f, 1.0f),
      ImVec4(0.16f, 0.78f, 0.25f, 1.0f)};
  const char* dotIds[3] = {"##macClose", "##macMinimize", "##macMaximize"};
  for (int i = 0; i < 3; ++i) {
    const ImVec2 p(pos.x + 13.0f + i * 16.0f, pos.y + 15.0f);
    DrawMacDot(hwnd, dotIds[i], p, dots[i], i);
  }

  const char* title = "Mind \xE2\x80\x94 workspace"; // Mind — workspace
  const float titleX = pos.x + 72.0f;
  const float titleY = pos.y + (size.y - UiFontSize(app)) * 0.5f - 1.0f;
  AddText(draw, UiBoldFont(app), UiFontSize(app), ImVec2(titleX, titleY),
          Pack(Theme::TextMain), title);

  const float folderX = pos.x + size.x * 0.65f;
  const ImVec2 folderMin(folderX, pos.y + 10.0f);
  ImGui::SetCursorScreenPos(folderMin);
  ImGui::InvisibleButton("##refreshWorkspace", ImVec2(28.0f, 28.0f));
  const bool folderHover = ImGui::IsItemHovered();
  if (folderHover)
    draw->AddRectFilled(folderMin, ImVec2(folderMin.x + 28.0f, folderMin.y + 28.0f),
                        Pack(WithAlpha(Theme::PanelLight, 0.95f)), 7.0f);
  AddFolderIcon(draw, ImVec2(folderMin.x + 3.0f, folderMin.y + 4.0f),
                Pack(folderHover ? Theme::Accent2 : Theme::TextDim));
  g_titlebarButtons.emplace_back(folderMin, ImVec2(folderMin.x + 28.0f, folderMin.y + 28.0f));
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
    app.ws.RefreshFileList();
    app.console.Add(LogTag::Sys, "workspace file list refreshed");
  }
  if (folderHover) ImGui::SetTooltip("Refresh local workspace files");

  const ImVec2 saveMin(folderMin.x + 34.0f, folderMin.y);
  ImGui::SetCursorScreenPos(saveMin);
  ImGui::InvisibleButton("##saveFile", ImVec2(28.0f, 28.0f));
  const bool saveHover = ImGui::IsItemHovered();
  if (saveHover)
    draw->AddRectFilled(saveMin, ImVec2(saveMin.x + 28.0f, saveMin.y + 28.0f),
                        Pack(WithAlpha(Theme::PanelLight, 0.95f)), 7.0f);
  AddSaveIcon(draw, ImVec2(saveMin.x + 3.0f, saveMin.y + 3.0f),
              Pack(saveHover ? Theme::Accent2 : Theme::TextDim));
  g_titlebarButtons.emplace_back(saveMin, ImVec2(saveMin.x + 28.0f, saveMin.y + 28.0f));
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) app.SaveActive();
  if (saveHover) ImGui::SetTooltip("Save current file (Ctrl+S)");

  // Правый блок: runtime badge и минималистичные системные кнопки окна.
  const float controlsWidth = 3.0f * 42.0f;
  const float controlsX = end.x - 14.0f - controlsWidth;
  const ImVec2 badgeMin(controlsX - 92.0f, pos.y + 8.0f);
  const ImVec2 badgeMax(badgeMin.x + 80.0f, badgeMin.y + 32.0f);
  draw->AddRectFilled(badgeMin, badgeMax, Pack(Theme::FooterButton), 7.0f);
  draw->AddRect(badgeMin, badgeMax, Pack(Theme::Border), 7.0f, 0, 1.0f);
  draw->AddCircleFilled(ImVec2(badgeMin.x + 12.0f, (badgeMin.y + badgeMax.y) * 0.5f),
                        3.2f, Pack(Theme::Cyan), 20);
  const char* runtime = "Lua 5.4";
  const ImVec2 runtimeSize = UiFont(app)->CalcTextSizeA(UiFontSize(app), FLT_MAX, 0.0f, runtime);
  AddText(draw, UiFont(app), UiFontSize(app),
          ImVec2(badgeMin.x + 22.0f,
                 badgeMin.y + (badgeMax.y - badgeMin.y - runtimeSize.y) * 0.5f),
          Pack(Theme::Cyan), runtime);

  DrawWindowButton(hwnd, "##windowMinimize", ImVec2(controlsX, pos.y + 6.0f), 0);
  DrawWindowButton(hwnd, "##windowMaximize", ImVec2(controlsX + 42.0f, pos.y + 6.0f), 1);
  DrawWindowButton(hwnd, "##windowClose", ImVec2(controlsX + 84.0f, pos.y + 6.0f), 2);

  draw->AddLine(ImVec2(pos.x, end.y - 1.0f), ImVec2(end.x, end.y - 1.0f),
                Pack(Theme::Border), 1.0f);
  ImGui::EndChild();
}

// ---------- file tabs -------------------------------------------------------

inline void DrawTabs(App& app) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BgDark);
  ImGui::BeginChild("##Tabs", ImVec2(0.0f, 52.0f), ImGuiChildFlags_None,
                    ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleColor();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 childPos = ImGui::GetWindowPos();
  const ImVec2 childSize = ImGui::GetWindowSize();
  draw->AddLine(ImVec2(childPos.x, childPos.y + childSize.y - 1.0f),
                ImVec2(childPos.x + childSize.x, childPos.y + childSize.y - 1.0f),
                Pack(Theme::Border), 1.0f);

  ImGui::SetCursorScreenPos(ImVec2(childPos.x + 12.0f, childPos.y + 4.0f));
  for (int i = 0; i < static_cast<int>(app.ws.tabs.size()); ++i) {
    const WorkspaceDocument& file = app.ws.tabs[i];
    ImGui::PushID(i);
    const std::string displayName = file.name + (file.dirty ? " *" : "");
    const float tabWidth = std::max(154.0f, TextWidth(UiFont(app), displayName.c_str()) + 66.0f);
    const ImVec2 tabMin = ImGui::GetCursorScreenPos();
    const ImVec2 tabSize(tabWidth, 42.0f);
    const ImGuiID animId = ImGui::GetID("##tabAnimation");
    ImGui::InvisibleButton("##tabHit", tabSize);
    const bool hovered = ImGui::IsItemHovered();
    const bool pressed = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const ImVec2 tabMax(tabMin.x + tabSize.x, tabMin.y + tabSize.y);
    const bool closeHit = hovered && ImGui::GetIO().MousePos.x >= tabMax.x - 28.0f;
    const float amount = ItemAnimation(app, animId, hovered, i == app.ws.active, pressed);

    if (i == app.ws.active || amount > 0.001f) {
      const ImVec4 tabBg = (i == app.ws.active)
          ? ImLerp(Theme::BgDark, Theme::TabActive, amount)
          : WithAlpha(Theme::PanelLight, amount * 0.55f);
      draw->AddRectFilled(tabMin, tabMax, Pack(tabBg), 8.0f,
                          ImDrawFlags_RoundCornersTop);
    }

    if (i == app.ws.active || amount > 0.15f) {
      const float glowAlpha = (i == app.ws.active) ? 0.22f * amount : amount * 0.12f;
      draw->AddRectFilled(ImVec2(tabMin.x + 13.0f, tabMax.y - 3.0f),
                          ImVec2(tabMax.x - 13.0f, tabMax.y + 1.0f),
                          Pack(WithAlpha(Theme::Accent2, glowAlpha)), 2.0f);
      if (i == app.ws.active)
        draw->AddRectFilled(ImVec2(tabMin.x + 20.0f, tabMax.y - 2.0f),
                            ImVec2(tabMax.x - 20.0f, tabMax.y),
                            Pack(WithAlpha(Theme::Accent2, amount)), 1.0f);
    }

    // Активная вкладка получает мягкую, постепенно появляющуюся точку.
    if (i == app.ws.active && amount > 0.001f) {
      draw->AddCircleFilled(ImVec2(tabMin.x + 18.0f, tabMin.y + 21.0f), 6.0f,
                            Pack(WithAlpha(Theme::Accent2, 0.20f * amount)), 24);
      draw->AddCircleFilled(ImVec2(tabMin.x + 18.0f, tabMin.y + 21.0f), 3.0f,
                            Pack(WithAlpha(Theme::Accent2, amount)), 24);
    }
    const ImVec4 textColor = ImLerp(Theme::TextDim, Theme::TextMain,
                                    (i == app.ws.active) ? amount : amount * 0.55f);
    AddText(draw, UiFont(app), UiFontSize(app),
            ImVec2(tabMin.x + 31.0f, tabMin.y + 13.0f), Pack(textColor), displayName.c_str());

    // Крестик — векторный, его hitbox входит в саму вкладку.
    const ImVec2 closeCenter(tabMax.x - 16.0f, tabMin.y + 21.0f);
    const ImU32 closeColor = Pack(WithAlpha(Theme::TextDim, closeHit ? 1.0f : 0.55f));
    draw->AddLine(ImVec2(closeCenter.x - 3.2f, closeCenter.y - 3.2f),
                  ImVec2(closeCenter.x + 3.2f, closeCenter.y + 3.2f), closeColor, 1.3f);
    draw->AddLine(ImVec2(closeCenter.x + 3.2f, closeCenter.y - 3.2f),
                  ImVec2(closeCenter.x - 3.2f, closeCenter.y + 3.2f), closeColor, 1.3f);

    if (clicked) {
      if (closeHit) app.ws.CloseTab(i, app.console);
      else app.ws.active = i;
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, 6.0f);
  }

  ImGui::PushID("new_tab");
  const ImVec2 plusMin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##newTab", ImVec2(38.0f, 38.0f));
  const bool plusHovered = ImGui::IsItemHovered();
  const bool plusClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  const ImGuiID plusAnimId = ImGui::GetID("##newTabAnimation");
  const float plusAmount = Animate(app, plusAnimId, plusHovered ? 1.0f : 0.0f);
  if (plusAmount > 0.001f)
    draw->AddRectFilled(plusMin, ImVec2(plusMin.x + 38.0f, plusMin.y + 38.0f),
                        Pack(WithAlpha(Theme::PanelLight, plusAmount * 0.85f)), 8.0f);
  const ImVec2 plusCenter(plusMin.x + 19.0f, plusMin.y + 19.0f);
  const ImU32 plusColor = Pack(ImLerp(Theme::TextDim, Theme::TextMain, plusAmount));
  draw->AddLine(ImVec2(plusCenter.x - 5.0f, plusCenter.y),
                ImVec2(plusCenter.x + 5.0f, plusCenter.y), plusColor, 1.6f);
  draw->AddLine(ImVec2(plusCenter.x, plusCenter.y - 5.0f),
                ImVec2(plusCenter.x, plusCenter.y + 5.0f), plusColor, 1.6f);
  if (plusClicked) app.ws.CreateNewTab(app.console);
  if (plusHovered) ImGui::SetTooltip("New tab (Ctrl+N)");
  ImGui::PopID();
  ImGui::EndChild();
}

inline void DrawRuntimeToast(App& app, ImGuiViewport* viewport) {
  const double elapsed = ImGui::GetTime();
  if (elapsed > 8.0) return;
  const float alpha = (elapsed < 6.0) ? 1.0f : 1.0f - (float)(elapsed - 6.0) / 2.0f;
  const ImVec2 min(viewport->Pos.x + viewport->Size.x - 158.0f,
                   viewport->Pos.y + 50.0f);
  const ImVec2 max(min.x + 144.0f, min.y + 50.0f);
  ImDrawList* draw = ImGui::GetForegroundDrawList(viewport);
  draw->AddRectFilled(min, max, Pack(WithAlpha(Theme::PanelLight, 0.96f * alpha)), 11.0f);
  draw->AddRect(min, max, Pack(WithAlpha(Theme::Border, alpha)), 11.0f, 0, 1.0f);
  AddText(draw, UiBoldFont(app), 8.0f,
          ImVec2(min.x + 17.0f, min.y + 7.0f), Pack(WithAlpha(Theme::TextDim, alpha)), "R U N T I M E");
  draw->AddCircleFilled(ImVec2(min.x + 20.0f, min.y + 33.0f), 6.0f,
                        Pack(WithAlpha(Theme::Cyan, 0.16f * alpha)), 20);
  draw->AddCircleFilled(ImVec2(min.x + 20.0f, min.y + 33.0f), 3.2f,
                        Pack(WithAlpha(Theme::Cyan, alpha)), 20);
  AddText(draw, UiBoldFont(app), UiFontSize(app),
          ImVec2(min.x + 34.0f, min.y + 25.0f),
          Pack(WithAlpha(Theme::TextMain, alpha)), "v1.0.0 loaded");
}

// ---------- sidebar ---------------------------------------------------------

inline void DrawNavIcon(ImDrawList* draw, int section, const ImVec2& min, ImU32 color) {
  const ImVec2 c(min.x + 10.0f, min.y + 10.0f);
  if (section == 0) {
    draw->AddLine(ImVec2(c.x - 5.0f, c.y + 5.0f), ImVec2(c.x - 5.0f, c.y - 4.0f), color, 1.6f);
    draw->AddLine(ImVec2(c.x - 1.0f, c.y + 5.0f), ImVec2(c.x - 1.0f, c.y - 1.0f), color, 1.6f);
    draw->AddLine(ImVec2(c.x + 3.0f, c.y + 5.0f), ImVec2(c.x + 3.0f, c.y - 6.0f), color, 1.6f);
    draw->AddLine(ImVec2(c.x + 6.0f, c.y + 5.0f), ImVec2(c.x + 6.0f, c.y - 2.0f), color, 1.6f);
  } else if (section == 1) {
    draw->AddLine(ImVec2(c.x - 4.0f, c.y - 4.0f), ImVec2(c.x - 8.0f, c.y), color, 1.5f);
    draw->AddLine(ImVec2(c.x - 8.0f, c.y), ImVec2(c.x - 4.0f, c.y + 4.0f), color, 1.5f);
    draw->AddLine(ImVec2(c.x + 4.0f, c.y - 4.0f), ImVec2(c.x + 8.0f, c.y), color, 1.5f);
    draw->AddLine(ImVec2(c.x + 8.0f, c.y), ImVec2(c.x + 4.0f, c.y + 4.0f), color, 1.5f);
  } else if (section == 2) {
    draw->AddLine(ImVec2(c.x - 5.0f, c.y - 4.0f), ImVec2(c.x, c.y), color, 1.5f);
    draw->AddLine(ImVec2(c.x, c.y), ImVec2(c.x - 5.0f, c.y + 4.0f), color, 1.5f);
    draw->AddLine(ImVec2(c.x + 2.0f, c.y + 5.0f), ImVec2(c.x + 8.0f, c.y + 5.0f), color, 1.5f);
  } else {
    draw->AddCircle(c, 4.0f, color, 16, 1.4f);
    draw->AddCircleFilled(c, 1.2f, color, 12);
    for (int i = 0; i < 8; ++i) {
      const float a = (float)i * 0.78539816339f;
      const ImVec2 from(c.x + std::cos(a) * 5.0f, c.y + std::sin(a) * 5.0f);
      const ImVec2 to(c.x + std::cos(a) * 7.0f, c.y + std::sin(a) * 7.0f);
      draw->AddLine(from, to, color, 1.4f);
    }
  }
}

inline void DrawNavItem(App& app, const char* label, int section) {
  const ImGuiID animId = ImGui::GetID(label);
  const ImVec2 min = ImGui::GetCursorScreenPos();
  const ImVec2 size(ImGui::GetContentRegionAvail().x, 38.0f);
  ImGui::InvisibleButton("##navItem", size);
  const bool hovered = ImGui::IsItemHovered();
  const bool pressed = ImGui::IsItemActive();
  const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  const bool active = app.page == 0 ? app.section == section : section == 3;
  const float amount = ItemAnimation(app, animId, hovered, active, pressed);
  const ImVec2 max(min.x + size.x, min.y + size.y);
  ImDrawList* draw = ImGui::GetWindowDrawList();

  if (active || amount > 0.001f) {
    const ImVec4 bg = active ? ImLerp(Theme::Panel, Theme::NavActive, amount)
                             : WithAlpha(Theme::PanelLight, amount * 0.70f);
    draw->AddRectFilled(min, max, Pack(bg), 8.0f);
  }
  if (active && amount > 0.001f) {
    draw->AddRectFilled(ImVec2(min.x, min.y + 7.0f),
                        ImVec2(min.x + 3.0f, max.y - 7.0f),
                        Pack(WithAlpha(Theme::Accent2, amount)), 2.0f);
  }
  const ImVec4 textColor = ImLerp(Theme::TextDim, Theme::TextMain,
                                  active ? amount : amount * 0.75f);
  const ImVec4 iconColor = active ? ImLerp(Theme::Accent2, Theme::TextMain, amount * 0.28f)
                                  : ImLerp(Theme::TextDim, Theme::TextMain, amount * 0.80f);
  DrawNavIcon(draw, section, ImVec2(min.x + 7.0f, min.y + 9.0f), Pack(iconColor));
  const std::string fittedLabel = FitUtf8Text(UiFont(app), label, size.x - 52.0f);
  AddText(draw, UiFont(app), UiFontSize(app),
          ImVec2(min.x + 37.0f, min.y + (size.y - UiFontSize(app)) * 0.5f),
          Pack(textColor), fittedLabel.c_str());

  if (clicked) {
    if (section == 0) { app.page = 0; app.section = 0; app.showConsole = true; }
    else if (section == 1) { app.page = 0; app.section = 1; app.showConsole = false; }
    else if (section == 2) { app.page = 0; app.section = 2; app.showConsole = true; }
    else { app.page = 1; }
  }
}

template <typename Callback>
inline void DrawListRow(App& app, const std::string& idText,
                        const char* label, const char* trailing,
                        bool selected, Callback&& onClick) {
  ImGui::PushID(idText.c_str());
  const ImGuiID animId = ImGui::GetID("##rowAnimation");
  const ImVec2 min = ImGui::GetCursorScreenPos();
  const ImVec2 size(ImGui::GetContentRegionAvail().x, 33.0f);
  ImGui::InvisibleButton("##row", size);
  const bool hovered = ImGui::IsItemHovered();
  const bool pressed = ImGui::IsItemActive();
  const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  const float amount = ItemAnimation(app, animId, hovered, selected, pressed);
  const ImVec2 max(min.x + size.x, min.y + size.y);
  ImDrawList* draw = ImGui::GetWindowDrawList();

  if (selected) {
    draw->AddRectFilled(min, max, Pack(WithAlpha(Theme::NavActive, 0.62f)), 7.0f);
  } else if (amount > 0.001f) {
    draw->AddRectFilled(min, max, Pack(WithAlpha(Theme::PanelLight, amount * 0.72f)), 7.0f);
  }
  const ImVec4 textColor = selected ? Theme::TextMain
      : ImLerp(Theme::TextDim, Theme::TextMain, amount * 0.82f);
  const bool popularItem = idText.rfind("popular/", 0) == 0;
  const float textOffset = popularItem ? 20.0f : 12.0f;
  if (popularItem) {
    draw->AddCircleFilled(ImVec2(min.x + 8.0f, min.y + size.y * 0.5f), 2.0f,
                          Pack(WithAlpha(Theme::Cyan, 0.80f + amount * 0.20f)), 16);
  }
  const float trailWidth = (trailing && trailing[0]) ? TextWidth(UiFont(app), trailing) : 0.0f;
  const float labelMaxWidth = size.x - textOffset - 12.0f - trailWidth - (trailWidth > 0.0f ? 8.0f : 0.0f);
  const std::string fittedLabel = FitUtf8Text(UiFont(app), label, labelMaxWidth);
  AddText(draw, UiFont(app), UiFontSize(app),
          ImVec2(min.x + textOffset, min.y + (size.y - UiFontSize(app)) * 0.5f),
          Pack(textColor), fittedLabel.c_str());
  if (trailing && trailing[0]) {
    AddText(draw, UiFont(app), UiFontSize(app),
            ImVec2(max.x - trailWidth - 12.0f,
                   min.y + (size.y - UiFontSize(app)) * 0.5f),
            Pack(Theme::TextDim), trailing);
  }
  if (clicked) onClick();
  ImGui::PopID();
}

inline void DrawSidebar(App& app, const ImVec2& size) {
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Panel);
  ImGui::BeginChild("##Sidebar", size, ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(2);

  ImGui::PushItemWidth(-1.0f);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::FooterButton);
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Theme::PanelLight);
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Theme::FooterButton);
  ImGui::PushStyleColor(ImGuiCol_Border, Theme::Border);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(38.0f, 9.0f));
  ImGui::InputTextWithHint("##SearchScripts", "Search scripts", &app.search);
  const ImVec2 searchMin = ImGui::GetItemRectMin();
  const ImVec2 searchMax = ImGui::GetItemRectMax();
  AddSearchIcon(ImGui::GetWindowDrawList(),
                ImVec2(searchMin.x + 17.0f, (searchMin.y + searchMax.y) * 0.5f),
                Pack(Theme::TextDim));
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(4);
  ImGui::PopItemWidth();

  ImGui::Dummy(ImVec2(1.0f, 8.0f));
  DrawNavItem(app, "Script Hub", 0);
  ImGui::Dummy(ImVec2(1.0f, 3.0f));
  DrawNavItem(app, "Editor", 1);
  ImGui::Dummy(ImVec2(1.0f, 3.0f));
  DrawNavItem(app, "Console", 2);
  ImGui::Dummy(ImVec2(1.0f, 3.0f));
  DrawNavItem(app, "Settings", 3);
  ImGui::Dummy(ImVec2(1.0f, 8.0f));

  ImGui::BeginChild("##SidebarLists", ImVec2(0.0f, 0.0f),
                    ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
  AddSectionLabel("POPULAR");
  ImGui::Dummy(ImVec2(1.0f, 4.0f));
  for (const Template& item : Workspace::Templates()) {
    if (!MatchSearch(item.name, app.search)) continue;
    std::string key = std::string("popular/") + item.name;
    DrawListRow(app, key, item.name, item.count, false,
                [&app, &item]() { app.ws.OpenTemplate(item, app.console); });
    ImGui::Dummy(ImVec2(1.0f, 2.0f));
  }

  ImGui::Dummy(ImVec2(1.0f, 7.0f));
  AddSectionLabel("WORKSPACE");
  ImGui::Dummy(ImVec2(1.0f, 4.0f));
  for (const std::string& name : app.ws.files) {
    if (!MatchSearch(name, app.search)) continue;
    DrawListRow(app, std::string("workspace/") + name,
                name.c_str(), nullptr, app.ws.IsOpen(name),
                [&app, name]() { app.ws.OpenFile(name, app.console); });
    ImGui::Dummy(ImVec2(1.0f, 2.0f));
  }
  ImGui::EndChild();
  ImGui::EndChild();
}

// ---------- footer action bar ----------------------------------------------

struct ButtonInput {
  ImVec2 min;
  ImVec2 max;
  bool hovered = false;
  bool active = false;
  bool clicked = false;
  float amount = 0.0f;
};

inline ButtonInput SubmitActionButton(App& app, const char* id,
                                      const ImVec2& min, const ImVec2& size) {
  ImGui::PushID(id);
  ImGui::SetCursorScreenPos(min);
  const ImGuiID animId = ImGui::GetID("##hoverAnim");
  ImGui::InvisibleButton("##buttonHit", size);
  ButtonInput result;
  result.min = min;
  result.max = ImVec2(min.x + size.x, min.y + size.y);
  result.hovered = ImGui::IsItemHovered();
  result.active = ImGui::IsItemActive();
  result.clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  result.amount = Animate(app, animId, result.active ? 1.0f : (result.hovered ? 0.65f : 0.0f));
  ImGui::PopID();
  return result;
}

inline void DrawFooter(App& app) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Panel);
  ImGui::BeginChild("##Footer", ImVec2(0.0f, 56.0f), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleColor();
  const ImVec2 pos = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), Pack(Theme::Panel));
  draw->AddLine(pos, ImVec2(pos.x + size.x, pos.y), Pack(Theme::Border), 1.0f);

  // Compact floating Script Hub status card, matching the reference UI.
  const ImVec2 hubMin(pos.x + 10.0f, pos.y + 5.0f);
  const ImVec2 hubMax(hubMin.x + 154.0f, hubMin.y + 46.0f);
  draw->AddRectFilled(hubMin, hubMax, Pack(Theme::PanelLight), 10.0f);
  draw->AddRect(hubMin, hubMax, Pack(WithAlpha(Theme::Accent2, 0.25f)), 10.0f, 0, 1.0f);
  const ImVec2 logoMin(hubMin.x + 7.0f, hubMin.y + 7.0f);
  const ImVec2 logoMax(logoMin.x + 32.0f, logoMin.y + 32.0f);
  AddRoundedGradient(draw, logoMin, logoMax, Theme::Accent, Theme::Accent2,
                     8.0f, Theme::PanelLight);
  const ImVec2 bookCenter(logoMin.x + 16.0f, logoMin.y + 16.0f);
  const ImU32 logoInk = Pack(Theme::TextMain);
  draw->AddLine(ImVec2(bookCenter.x, bookCenter.y - 8.0f),
                ImVec2(bookCenter.x, bookCenter.y + 8.0f), logoInk, 1.2f);
  draw->AddLine(ImVec2(bookCenter.x - 7.0f, bookCenter.y - 6.0f),
                ImVec2(bookCenter.x - 1.5f, bookCenter.y - 4.5f), logoInk, 1.2f);
  draw->AddLine(ImVec2(bookCenter.x - 7.0f, bookCenter.y - 6.0f),
                ImVec2(bookCenter.x - 7.0f, bookCenter.y + 6.0f), logoInk, 1.2f);
  draw->AddLine(ImVec2(bookCenter.x - 7.0f, bookCenter.y + 6.0f),
                ImVec2(bookCenter.x - 1.5f, bookCenter.y + 7.0f), logoInk, 1.2f);
  draw->AddLine(ImVec2(bookCenter.x + 7.0f, bookCenter.y - 6.0f),
                ImVec2(bookCenter.x + 1.5f, bookCenter.y - 4.5f), logoInk, 1.2f);
  draw->AddLine(ImVec2(bookCenter.x + 7.0f, bookCenter.y - 6.0f),
                ImVec2(bookCenter.x + 7.0f, bookCenter.y + 6.0f), logoInk, 1.2f);
  draw->AddLine(ImVec2(bookCenter.x + 7.0f, bookCenter.y + 6.0f),
                ImVec2(bookCenter.x + 1.5f, bookCenter.y + 7.0f), logoInk, 1.2f);
  AddText(draw, UiBoldFont(app), UiFontSize(app),
          ImVec2(hubMin.x + 48.0f, hubMin.y + 8.0f), Pack(Theme::TextMain), "Script Hub");
  AddText(draw, UiFont(app), UiFontSize(app) - 2.0f,
          ImVec2(hubMin.x + 48.0f, hubMin.y + 25.0f), Pack(Theme::TextDim), "synced & ready");

  // Status dot softly pulses; status text stays green for quick scanning.
  const float pulse = 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 3.2f);
  const ImVec2 statusCenter(hubMax.x + 14.0f, pos.y + size.y * 0.5f);
  draw->AddCircleFilled(statusCenter, 8.0f,
                        Pack(WithAlpha(Theme::Green, 0.08f + pulse * 0.11f)), 24);
  draw->AddCircleFilled(statusCenter, 3.5f, Pack(Theme::Green), 24);
  const char* status = app.attached ? "Attached" : "idle";
  AddText(draw, UiBoldFont(app), UiFontSize(app),
          ImVec2(statusCenter.x + 12.0f, pos.y + (size.y - UiFontSize(app)) * 0.5f),
          Pack(Theme::Green), status);

  constexpr float kButtonHeight = 38.0f;
  constexpr float kGap = 10.0f;
  constexpr float kClearWidth = 104.0f;
  constexpr float kAttachWidth = 122.0f;
  constexpr float kExecuteWidth = 138.0f;
  const float right = pos.x + size.x - 16.0f;
  const float y = pos.y + (size.y - kButtonHeight) * 0.5f;
  const ImVec2 executeMin(right - kExecuteWidth, y);
  const ImVec2 attachMin(executeMin.x - kGap - kAttachWidth, y);
  const ImVec2 clearMin(attachMin.x - kGap - kClearWidth, y);
  const ImU32 white = Pack(Theme::TextMain);

  ButtonInput clear = SubmitActionButton(app, "clear", clearMin, ImVec2(kClearWidth, kButtonHeight));
  const ImVec4 clearBg = ImLerp(Theme::FooterButton, Theme::PanelLight, clear.amount * 0.55f);
  draw->AddRectFilled(clear.min, clear.max, Pack(clearBg), 8.0f);
  draw->AddRect(clear.min, clear.max, Pack(Theme::Border), 8.0f, 0, 1.0f);
  AddClearIcon(draw, ImVec2(clear.min.x + 14.0f, clear.min.y + 9.0f),
               Pack(ImLerp(Theme::TextDim, Theme::TextMain, clear.amount)));
  AddText(draw, UiFont(app), UiFontSize(app), ImVec2(clear.min.x + 42.0f, clear.min.y + 11.0f),
          Pack(Theme::TextMain), "Clear");
  if (clear.clicked) {
    app.console.Clear();
    app.console.Add(LogTag::Sys, "console cleared");
  }

  ButtonInput attach = SubmitActionButton(app, "attach", attachMin, ImVec2(kAttachWidth, kButtonHeight));
  const ImVec4 attachBg = ImLerp(Theme::FooterButton, Theme::NavActive, attach.amount * 0.60f);
  const ImVec4 attachBorder = ImLerp(Theme::Border, Theme::Accent2, 0.45f + attach.amount * 0.55f);
  const ImVec4 attachText = ImLerp(Theme::Accent2, Theme::TextMain, attach.amount * 0.30f);
  draw->AddRectFilled(attach.min, attach.max, Pack(attachBg), 8.0f);
  draw->AddRect(attach.min, attach.max, Pack(attachBorder), 8.0f, 0, 1.0f);
  AddLinkIcon(draw, ImVec2(attach.min.x + 13.0f, attach.min.y + 9.0f), Pack(attachText));
  AddText(draw, UiFont(app), UiFontSize(app), ImVec2(attach.min.x + 42.0f, attach.min.y + 11.0f),
          Pack(attachText), app.attached ? "Detach" : "Attach");
  if (attach.clicked) {
    app.attached = !app.attached;
    app.console.Add(LogTag::Net, app.attached
        ? "local workspace connection simulated"
        : "local workspace connection released");
  }

  ButtonInput execute = SubmitActionButton(app, "execute", executeMin, ImVec2(kExecuteWidth, kButtonHeight));
  const ImVec4 gradientLeft = Lighten(Theme::Accent, execute.amount * 0.12f);
  const ImVec4 gradientRight = Lighten(Theme::Accent2, execute.amount * 0.12f);
  AddRoundedGradient(draw, execute.min, execute.max, gradientLeft, gradientRight, 8.0f);
  draw->AddRect(execute.min, execute.max,
                Pack(WithAlpha(Theme::TextMain, 0.10f + execute.amount * 0.10f)), 8.0f, 0, 1.0f);
  const ImVec2 playCenter(execute.min.x + 27.0f, execute.min.y + kButtonHeight * 0.5f);
  AddPlayIcon(draw, playCenter, white, 0.90f);
  const char* executeLabel = "Execute";
  ImFont* bold = UiBoldFont(app);
  const ImVec2 labelSize = bold->CalcTextSizeA(bold->FontSize, FLT_MAX, 0.0f, executeLabel);
  AddText(draw, UiBoldFont(app), UiFontSize(app),
          ImVec2(execute.min.x + 45.0f,
                 execute.min.y + (kButtonHeight - labelSize.y) * 0.5f),
          white, executeLabel);
  if (execute.clicked) {
    WorkspaceDocument* file = app.ws.ActiveFile();
    if (!file) {
      app.console.Add(LogTag::Err, "no active file to preview");
    } else {
      int lines = 1;
      for (char ch : file->buffer) if (ch == '\n') ++lines;
      app.console.Add(LogTag::Mind,
          "execute requested: " + file->name + " (" + std::to_string(lines) + " lines)");
      app.console.Add(LogTag::Sys,
          "safe mode: Lua is not evaluated; local preview status=ready");
    }
  }
  ImGui::EndChild();
}

// ---------- settings / central work area -----------------------------------

inline void DrawSettings(App& app) {
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Panel);
  ImGui::BeginChild("##SettingsPanel", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_None);
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();

  ImGui::Text("Settings");
  ImGui::Separator();
  ImGui::ColorEdit3("Accent color", reinterpret_cast<float*>(&app.accent));
  ImGui::Checkbox("Console autoscroll", &app.autoscroll);
  ImGui::Checkbox("Lua syntax highlighting", &app.editor.highlightEnabled);
  ImGui::Separator();
  ImGui::TextWrapped(
      "Mind Workspace 1.0.0. Lua 5.4 is a profile badge only: this safe-mode build "
      "does not evaluate scripts. Files stay in the local ./workspace folder.");
  ImGui::EndChild();
  ApplyTheme(app.accent, app.rounding);
}

} // namespace MindUi

// ---------- layout ----------------------------------------------------------

inline void App::Render(HWND hwnd) {
  g_titlebarButtons.clear();
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::BgDark);
  ImGui::Begin("##Main", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
               ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(3);

  MindUi::DrawTitleBar(*this, hwnd);
  MindUi::DrawTabs(*this);
  MindUi::DrawRuntimeToast(*this, viewport);

  const float footerHeight = 56.0f;
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const float middleHeight = std::max(160.0f, available.y - footerHeight);
  const float sidebarWidth = std::clamp(available.x * 0.285f, 240.0f, 372.0f);
  const ImVec2 sidebarSize(sidebarWidth, middleHeight);
  MindUi::DrawSidebar(*this, sidebarSize);
  ImGui::SameLine(0.0f, 10.0f);

  ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BgDark);
  ImGui::BeginChild("##Center", ImVec2(0.0f, middleHeight), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleColor();
  if (page == 1) {
    MindUi::DrawSettings(*this);
  } else {
    const bool consoleOnly = section == 2;
    const bool renderConsole = showConsole || consoleOnly;
    float availableHeight = ImGui::GetContentRegionAvail().y;
    float renderedConsoleHeight = 0.0f;
    if (renderConsole) {
      renderedConsoleHeight = consoleOnly
          ? std::max(120.0f, availableHeight - 12.0f)
          : std::clamp(consoleHeight, 100.0f, std::max(100.0f, availableHeight - 170.0f));
    }
    const float splitterHeight = renderConsole && !consoleOnly ? 8.0f : 0.0f;
    const float editorHeight = std::max(80.0f, availableHeight - renderedConsoleHeight - splitterHeight);

    if (!consoleOnly) {
      WorkspaceDocument* file = ws.ActiveFile();
      if (file) {
        ImGui::PushFont(monoFont ? monoFont : ImGui::GetFont());
        const bool edited = editor.Render("##Code", file->buffer, ImVec2(0.0f, editorHeight));
        ImGui::PopFont();
        if (edited) file->dirty = true;
      }
    }

    if (renderConsole && !consoleOnly) {
      const ImVec2 splitterMin = ImGui::GetCursorScreenPos();
      const float splitterWidth = ImGui::GetContentRegionAvail().x;
      ImGui::InvisibleButton("##ConsoleSplitter", ImVec2(splitterWidth, splitterHeight));
      if (ImGui::IsItemActive()) {
        consoleHeight = std::clamp(consoleHeight - ImGui::GetIO().MouseDelta.y,
                                   100.0f, std::max(100.0f, availableHeight - 120.0f));
      }
      const bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
      const ImVec2 splitterMax(splitterMin.x + splitterWidth, splitterMin.y + splitterHeight);
      ImGui::GetWindowDrawList()->AddLine(
          ImVec2(splitterMin.x, (splitterMin.y + splitterMax.y) * 0.5f),
          ImVec2(splitterMax.x, (splitterMin.y + splitterMax.y) * 0.5f),
          MindUi::Pack(hot ? Theme::Accent2 : Theme::Border), hot ? 1.5f : 1.0f);
    }
    if (renderConsole) console.Render(autoscroll, renderedConsoleHeight);
  }
  ImGui::EndChild();

  MindUi::DrawFooter(*this);

  // Глобальные горячие клавиши работают и при фокусе в редакторе.
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) SaveActive();
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) ws.CreateNewTab(console);
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_W) && ws.active >= 0)
    ws.CloseTab(ws.active, console);

  ImGui::End();
}
