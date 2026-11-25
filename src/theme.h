#pragma once
// ---------------------------------------------------------------------------
// theme.h — цветовая палитра и стиль Dear ImGui.
// Палитра: глубокий тёмно-синий фон, панели чуть светлее,
// фиолетово-синие акценты. Скругления: 8.0 для окон/карточек, 6.0 для кнопок.
// ---------------------------------------------------------------------------
#include "imgui.h"

namespace Theme {
inline const ImVec4 BgDark     = ImVec4(0.051f, 0.055f, 0.078f, 1.0f); // #0D0E14
inline const ImVec4 Panel      = ImVec4(0.075f, 0.082f, 0.122f, 1.0f); // #13151F
inline const ImVec4 PanelLight = ImVec4(0.110f, 0.118f, 0.165f, 1.0f);
inline const ImVec4 Accent     = ImVec4(0.388f, 0.357f, 1.000f, 1.0f); // #635BFF
inline const ImVec4 Accent2    = ImVec4(0.545f, 0.361f, 0.965f, 1.0f); // #8B5CF6
inline const ImVec4 TextMain   = ImVec4(0.910f, 0.918f, 0.945f, 1.0f);
inline const ImVec4 TextDim    = ImVec4(0.565f, 0.580f, 0.663f, 1.0f);
inline const ImVec4 Border     = ImVec4(0.165f, 0.176f, 0.243f, 1.0f);
inline const ImVec4 Green      = ImVec4(0.231f, 0.824f, 0.478f, 1.0f);
inline const ImVec4 Red        = ImVec4(0.937f, 0.267f, 0.267f, 1.0f);
inline const ImVec4 Yellow     = ImVec4(0.961f, 0.620f, 0.043f, 1.0f);
inline const ImVec4 Cyan       = ImVec4(0.216f, 0.812f, 0.976f, 1.0f);
} // namespace Theme

inline ImVec4 Lighten(const ImVec4& c, float f) {
  return ImVec4(c.x + (1.0f - c.x) * f, c.y + (1.0f - c.y) * f, c.z + (1.0f - c.z) * f, c.w);
}
inline ImVec4 Darken(const ImVec4& c, float f) {
  return ImVec4(c.x * (1.0f - f), c.y * (1.0f - f), c.z * (1.0f - f), c.w);
}

// Применяет тему к текущему стилю ImGui. Вызывается при старте
// и при изменении акцента/скруглений в настройках.
inline void ApplyTheme(const ImVec4& accent, float rounding = 8.0f) {
  ImGuiStyle& s = ImGui::GetStyle();
  s.WindowRounding   = rounding;
  s.ChildRounding    = rounding;
  s.FrameRounding    = 6.0f;
  s.PopupRounding    = 6.0f;
  s.ScrollbarRounding = 6.0f;
  s.GrabRounding     = 6.0f;
  s.TabRounding      = 6.0f;
  s.WindowPadding    = ImVec2(10, 10);
  s.FramePadding     = ImVec2(8, 5);
  s.ItemSpacing      = ImVec2(8, 6);
  s.ItemInnerSpacing = ImVec2(6, 4);
  s.ScrollbarSize    = 10.0f;
  s.FrameBorderSize  = 0.0f;
  s.WindowBorderSize = 0.0f;
  s.ChildBorderSize  = 1.0f;
  s.PopupBorderSize  = 1.0f;

  ImVec4* c = s.Colors;
  c[ImGuiCol_WindowBg]            = Theme::BgDark;
  c[ImGuiCol_ChildBg]             = Theme::Panel;
  c[ImGuiCol_PopupBg]             = Theme::PanelLight;
  c[ImGuiCol_Border]              = Theme::Border;
  c[ImGuiCol_BorderShadow]        = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_FrameBg]             = Theme::PanelLight;
  c[ImGuiCol_FrameBgHovered]      = ImVec4(accent.x, accent.y, accent.z, 0.20f);
  c[ImGuiCol_FrameBgActive]       = ImVec4(accent.x, accent.y, accent.z, 0.35f);
  c[ImGuiCol_TitleBg]             = Theme::Panel;
  c[ImGuiCol_TitleBgActive]       = Theme::Panel;
  c[ImGuiCol_MenuBarBg]           = Theme::Panel;
  c[ImGuiCol_ScrollbarBg]         = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_ScrollbarGrab]       = Theme::PanelLight;
  c[ImGuiCol_ScrollbarGrabHovered] = Lighten(Theme::PanelLight, 0.2f);
  c[ImGuiCol_ScrollbarGrabActive] = accent;
  c[ImGuiCol_CheckMark]           = accent;
  c[ImGuiCol_SliderGrab]          = accent;
  c[ImGuiCol_SliderGrabActive]    = Lighten(accent, 0.2f);
  c[ImGuiCol_Button]              = accent;
  c[ImGuiCol_ButtonHovered]       = Lighten(accent, 0.15f);
  c[ImGuiCol_ButtonActive]        = Darken(accent, 0.15f);
  c[ImGuiCol_Header]              = ImVec4(accent.x, accent.y, accent.z, 0.20f);
  c[ImGuiCol_HeaderHovered]       = ImVec4(accent.x, accent.y, accent.z, 0.30f);
  c[ImGuiCol_HeaderActive]        = ImVec4(accent.x, accent.y, accent.z, 0.40f);
  c[ImGuiCol_Separator]           = Theme::Border;
  c[ImGuiCol_SeparatorHovered]    = accent;
  c[ImGuiCol_SeparatorActive]     = accent;
  c[ImGuiCol_ResizeGrip]          = ImVec4(accent.x, accent.y, accent.z, 0.25f);
  c[ImGuiCol_ResizeGripHovered]   = ImVec4(accent.x, accent.y, accent.z, 0.45f);
  c[ImGuiCol_ResizeGripActive]    = accent;
  c[ImGuiCol_Tab]                 = Theme::Panel;
  c[ImGuiCol_TabHovered]          = Theme::PanelLight;
  c[ImGuiCol_TabSelected]         = Theme::PanelLight;
  c[ImGuiCol_TabDimmed]           = Theme::Panel;
  c[ImGuiCol_TabDimmedSelected]   = Theme::PanelLight;
  c[ImGuiCol_Text]                = Theme::TextMain;
  c[ImGuiCol_TextDisabled]        = Theme::TextDim;
  c[ImGuiCol_TextSelectedBg]      = ImVec4(accent.x, accent.y, accent.z, 0.35f);
  c[ImGuiCol_DragDropTarget]      = accent;
  c[ImGuiCol_NavCursor]           = accent;
  c[ImGuiCol_ModalWindowDimBg]    = ImVec4(0, 0, 0, 0.6f);
}
