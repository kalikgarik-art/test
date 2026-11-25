#pragma once
// ---------------------------------------------------------------------------
// console.h — нижняя панель консоли (Terminal / Output).
// Цветные префиксы [MIND] / [NET] / [SYS] / [ERR] и автопрокрутка вниз.
// ---------------------------------------------------------------------------
#include <string>
#include <vector>
#include "imgui.h"
#include "theme.h"

enum class LogTag { Mind, Net, Sys, Err };

struct LogEntry {
  LogTag tag;
  std::string text;
};

class Console {
public:
  void Add(LogTag tag, const std::string& text) {
    logs_.push_back(LogEntry{tag, text});
    dirty_ = true;
  }
  void Clear() {
    logs_.clear();
    dirty_ = true;
  }

  void Render(bool autoscroll, float height) {
    ImGui::BeginChild("##ConsoleBody", ImVec2(0, height), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& e : logs_) {
      ImGui::TextColored(TagColor(e.tag), TagName(e.tag));
      ImGui::SameLine(0, 6);
      ImGui::TextUnformatted(e.text.c_str());
    }
    if (dirty_ && autoscroll && !logs_.empty())
      ImGui::SetScrollHereY(1.0f);
    dirty_ = false;
    ImGui::EndChild();
  }

private:
  static const char* TagName(LogTag t) {
    switch (t) {
      case LogTag::Mind: return "[MIND]";
      case LogTag::Net:  return "[NET]";
      case LogTag::Sys:  return "[SYS]";
      case LogTag::Err:  return "[ERR]";
    }
    return "[SYS]";
  }
  static ImVec4 TagColor(LogTag t) {
    switch (t) {
      case LogTag::Mind: return Theme::Accent2;
      case LogTag::Net:  return Theme::Cyan;
      case LogTag::Sys:  return Theme::Green;
      case LogTag::Err:  return Theme::Red;
    }
    return Theme::TextMain;
  }

  std::vector<LogEntry> logs_;
  bool dirty_ = false;
};
