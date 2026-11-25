#pragma once
// ---------------------------------------------------------------------------
// theme.h — палитра Mind Workspace и чистая базовая тема Dear ImGui.
// Базовые рамки окон/полей отключены; деликатная обводка #23263A, 1 px
// включается локально только для карточек, поиска, редактора и консоли.
// ---------------------------------------------------------------------------
#include "imgui.h"

namespace Theme {
inline const ImVec4 BgDark       = ImVec4(13.0f / 255.0f, 14.0f / 255.0f, 20.0f / 255.0f, 1.0f); // #0D0E14
inline const ImVec4 Panel        = ImVec4(19.0f / 255.0f, 21.0f / 255.0f, 31.0f / 255.0f, 1.0f); // #13151F
inline const ImVec4 PanelLight   = ImVec4(27.0f / 255.0f, 30.0f / 255.0f, 45.0f / 255.0f, 1.0f);
inline const ImVec4 Border       = ImVec4(35.0f / 255.0f, 38.0f / 255.0f, 58.0f / 255.0f, 1.0f); // #23263A
inline const ImVec4 Accent       = ImVec4(99.0f / 255.0f, 91.0f / 255.0f, 255.0f / 255.0f, 1.0f); // #635BFF
inline const ImVec4 Accent2      = ImVec4(139.0f / 255.0f, 92.0f / 255.0f, 246.0f / 255.0f, 1.0f); // #8B5CF6
inline const ImVec4 NavActive    = ImVec4(37.0f / 255.0f, 33.0f / 255.0f, 66.0f / 255.0f, 1.0f); // #252142
inline const ImVec4 TabActive    = ImVec4(26.0f / 255.0f, 29.0f / 255.0f, 46.0f / 255.0f, 1.0f); // #1A1D2E
inline const ImVec4 FooterButton = ImVec4(28.0f / 255.0f, 31.0f / 255.0f, 46.0f / 255.0f, 1.0f); // #1C1F2E
inline const ImVec4 TextMain     = ImVec4(232.0f / 255.0f, 234.0f / 255.0f, 241.0f / 255.0f, 1.0f);
inline const ImVec4 TextDim      = ImVec4(144.0f / 255.0f, 148.0f / 255.0f, 169.0f / 255.0f, 1.0f);
inline const ImVec4 Green        = ImVec4(59.0f / 255.0f, 210.0f / 255.0f, 122.0f / 255.0f, 1.0f); // #3BD27A
inline const ImVec4 Red          = ImVec4(239.0f / 255.0f, 83.0f / 255.0f, 80.0f / 255.0f, 1.0f);
inline const ImVec4 Yellow       = ImVec4(255.0f / 255.0f, 193.0f / 255.0f, 7.0f / 255.0f, 1.0f);
inline const ImVec4 Cyan         = ImVec4(54.0f / 255.0f, 207.0f / 255.0f, 255.0f / 255.0f, 1.0f);
} // namespace Theme

inline ImVec4 Lighten(const ImVec4& c, float amount) {
  return ImVec4(c.x + (1.0f - c.x) * amount,
                c.y + (1.0f - c.y) * amount,
                c.z + (1.0f - c.z) * amount, c.w);
}
inline ImVec4 Darken(const ImVec4& c, float amount) {
  return ImVec4(c.x * (1.0f - amount), c.y * (1.0f - amount),
                c.z * (1.0f - amount), c.w);
}
inline ImVec4 WithAlpha(ImVec4 c, float alpha) {
  c.w = alpha;
  return c;
}

inline void ApplyTheme(const ImVec4& accent, float rounding = 8.0f) {
  ImGuiStyle& s = ImGui::GetStyle();
  s.WindowRounding    = rounding;
  s.ChildRounding     = rounding;
  s.FrameRounding     = 8.0f;
  s.PopupRounding     = 8.0f;
  s.ScrollbarRounding = 8.0f;
  s.GrabRounding      = 6.0f;
  s.TabRounding       = 8.0f;

  s.WindowPadding    = ImVec2(12.0f, 12.0f);
  s.FramePadding     = ImVec2(12.0f, 8.0f);
  s.ItemSpacing      = ImVec2(8.0f, 8.0f);
  s.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
  s.ScrollbarSize    = 10.0f;

  // Никаких стандартных тяжёлых рамок. Детальные карточки включают
  // ImGuiStyleVar_ChildBorderSize / FrameBorderSize локально в ui.h.
  s.WindowBorderSize = 0.0f;
  s.FrameBorderSize  = 0.0f;
  s.ChildBorderSize  = 0.0f;
  s.PopupBorderSize  = 1.0f;

  ImVec4* c = s.Colors;
  c[ImGuiCol_Text]                  = Theme::TextMain;
  c[ImGuiCol_TextDisabled]          = Theme::TextDim;
  c[ImGuiCol_WindowBg]              = Theme::BgDark;
  c[ImGuiCol_ChildBg]               = Theme::Panel;
  c[ImGuiCol_PopupBg]               = Theme::PanelLight;
  c[ImGuiCol_Border]                = Theme::Border;
  c[ImGuiCol_BorderShadow]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  c[ImGuiCol_FrameBg]               = Theme::FooterButton;
  c[ImGuiCol_FrameBgHovered]        = WithAlpha(accent, 0.15f);
  c[ImGuiCol_FrameBgActive]         = WithAlpha(accent, 0.25f);
  c[ImGuiCol_TitleBg]               = Theme::Panel;
  c[ImGuiCol_TitleBgActive]         = Theme::Panel;
  c[ImGuiCol_MenuBarBg]             = Theme::Panel;
  c[ImGuiCol_ScrollbarBg]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  c[ImGuiCol_ScrollbarGrab]         = Theme::PanelLight;
  c[ImGuiCol_ScrollbarGrabHovered]  = Lighten(Theme::PanelLight, 0.15f);
  c[ImGuiCol_ScrollbarGrabActive]   = accent;
  c[ImGuiCol_CheckMark]             = accent;
  c[ImGuiCol_SliderGrab]            = accent;
  c[ImGuiCol_SliderGrabActive]      = Lighten(accent, 0.15f);
  c[ImGuiCol_Button]                = Theme::FooterButton;
  c[ImGuiCol_ButtonHovered]         = Theme::PanelLight;
  c[ImGuiCol_ButtonActive]          = WithAlpha(accent, 0.35f);
  c[ImGuiCol_Header]                = WithAlpha(accent, 0.15f);
  c[ImGuiCol_HeaderHovered]         = WithAlpha(accent, 0.22f);
  c[ImGuiCol_HeaderActive]          = WithAlpha(accent, 0.32f);
  c[ImGuiCol_Separator]             = Theme::Border;
  c[ImGuiCol_SeparatorHovered]      = accent;
  c[ImGuiCol_SeparatorActive]       = accent;
  c[ImGuiCol_ResizeGrip]            = WithAlpha(accent, 0.20f);
  c[ImGuiCol_ResizeGripHovered]     = WithAlpha(accent, 0.40f);
  c[ImGuiCol_ResizeGripActive]      = accent;
  c[ImGuiCol_Tab]                   = Theme::Panel;
  c[ImGuiCol_TabHovered]            = Theme::PanelLight;
  c[ImGuiCol_TabSelected]           = Theme::TabActive;
  c[ImGuiCol_TabDimmed]             = Theme::Panel;
  c[ImGuiCol_TabDimmedSelected]     = Theme::TabActive;
  c[ImGuiCol_TextSelectedBg]        = WithAlpha(accent, 0.30f);
  c[ImGuiCol_DragDropTarget]        = accent;
  c[ImGuiCol_NavCursor]             = accent;
  c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.0f, 0.0f, 0.0f, 0.60f);
}
