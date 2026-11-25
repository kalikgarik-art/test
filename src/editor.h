#pragma once
// ---------------------------------------------------------------------------
// editor.h — центральная область редактора кода.
// Нумерация строк слева + базовая подсветка синтаксиса Lua.
// Реализация: InputTextMultiline с прозрачным текстом, поверх которого
// рисуется подсвеченный текст (тот же моноширинный шрифт, те же метрики).
// Курсор рисуется вручную (позиция берётся из InputTextCallbackData).
// ---------------------------------------------------------------------------
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <string>
#include <unordered_set>
#include "imgui.h"
#include "imgui_stdlib.h"
#include "theme.h"

class CodeEditor {
public:
  bool highlightEnabled = true;
  int cursorPos = 0;      // позиция курсора (байтовый индекс в буфере)
  ImFont* font = nullptr; // моноширинный шрифт (Consolas)

  // Возвращает true, если текст был изменён в этом кадре.
  bool Render(const char* id, std::string& buffer, ImVec2 size) {
    ImFont* f = font ? font : ImGui::GetFont();
    float fs = ImGui::GetFontSize();
    float charW = f->GetCharAdvance('M'); // ширина символа (моноширинный шрифт)
    float lineH = ImGui::GetTextLineHeight();

    ImVec2 sz = size;
    if (sz.x <= 0) sz.x = ImGui::GetContentRegionAvail().x;
    if (sz.y <= 0) sz.y = ImGui::GetContentRegionAvail().y;

    int lineCount = 1;
    for (char c : buffer) if (c == '\n') lineCount++;
    int digits = (int)std::to_string(lineCount).size();
    float gutterW = std::max(40.0f, digits * charW + 18.0f);

    bool edited = false;

    // --- Гуттер с номерами строк (скролл берём с прошлого кадра) ---
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("##Gutter", ImVec2(gutterW, sz.y), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);
    {
      ImDrawList* dl = ImGui::GetWindowDrawList();
      ImVec2 base = ImGui::GetCursorScreenPos();
      float availW = ImGui::GetContentRegionAvail().x;
      int curLine = 0;
      for (int i = 0; i < cursorPos && i < (int)buffer.size(); i++)
        if (buffer[i] == '\n') curLine++;
      for (int i = 0; i < lineCount; i++) {
        float y = base.y + i * lineH - scrollY_;
        if (y < base.y - lineH || y > base.y + sz.y) continue;
        std::string num = std::to_string(i + 1);
        ImVec2 ts = ImGui::CalcTextSize(num.c_str());
        ImVec4 col = (i == curLine) ? Theme::TextMain : Theme::TextDim;
        dl->AddText(f, fs, ImVec2(base.x + availW - ts.x - 4.0f, y),
                    ImGui::ColorConvertFloat4ToU32(col), num.c_str());
      }
    }
    ImGui::EndChild();
    ImGui::SameLine(0, 0);

    // --- Область кода ---
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::BgDark);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("##Editor", ImVec2(sz.x - gutterW, sz.y), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleVar(2);
    {
      ImVec2 avail = ImGui::GetContentRegionAvail();
      ImVec2 content = ImGui::CalcTextSize(buffer.c_str(), buffer.c_str() + buffer.size(),
                                           false, FLT_MAX);
      ImVec2 inputSize(std::max(avail.x, content.x + 24.0f),
                       std::max(avail.y, content.y + 24.0f));

      // Прозрачный InputTextMultiline: видимый текст рисуем сами с подсветкой.
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
      ImGui::InputTextMultiline(id, &buffer, inputSize,
                                ImGuiInputTextFlags_CallbackAlways |
                                    ImGuiInputTextFlags_AllowTabInput,
                                &Callback, this);
      ImGui::PopStyleColor(5);
      ImGui::PopStyleVar();
      edited = ImGui::IsItemEdited();

      ImVec2 itemMin = ImGui::GetItemRectMin();
      ImVec2 itemMax = ImGui::GetItemRectMax();
      ImDrawList* dl = ImGui::GetWindowDrawList();

      if (highlightEnabled)
        DrawHighlighted(dl, buffer, itemMin, f, fs, charW, lineH);

      // Собственный курсор (текст прозрачный — курсор ImGui не виден).
      if (ImGui::IsItemActive()) {
        int line = 0, col = 0;
        for (int i = 0; i < cursorPos && i < (int)buffer.size(); i++) {
          if (buffer[i] == '\n') { line++; col = 0; }
          else if (buffer[i] == '\t') col += 4 - (col % 4);
          else col++;
        }
        if ((int)(ImGui::GetTime() * 1.6f) % 2 == 0) {
          ImVec2 cp = itemMin + ImVec2(col * charW, line * lineH);
          dl->PushClipRect(itemMin, itemMax, true);
          dl->AddLine(cp, cp + ImVec2(0, lineH),
                      ImGui::ColorConvertFloat4ToU32(Theme::Accent2), 1.5f);
          dl->PopClipRect();
        }
      }
      scrollY_ = ImGui::GetScrollY();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    return edited;
  }

private:
  float scrollY_ = 0.0f;

  static int Callback(ImGuiInputTextCallbackData* data) {
    auto* self = static_cast<CodeEditor*>(data->UserData);
    if (self) self->cursorPos = data->CursorPos;
    return 0;
  }

  // Однопроходный токенизатор Lua: ключевые слова, строки, комментарии,
  // числа, встроенные функции и вызовы функций. Сегменты не содержат '\n'
  // (многострочные блоки рисуются построчно), поэтому AddText не ломается.
  void DrawHighlighted(ImDrawList* dl, const std::string& text, ImVec2 origin,
                       ImFont* f, float fontSize, float charW, float lineH) {
    const ImVec4 cDefault = ImVec4(0.839f, 0.871f, 0.910f, 1.0f); // #D6DEEB
    const ImVec4 cKeyword = Theme::Accent2;                       // фиолетовый
    const ImVec4 cString  = ImVec4(0.639f, 0.745f, 0.549f, 1.0f); // #A3BE8C
    const ImVec4 cComment = ImVec4(0.420f, 0.447f, 0.502f, 1.0f); // серый
    const ImVec4 cNumber  = ImVec4(0.969f, 0.549f, 0.424f, 1.0f); // #F78C6C
    const ImVec4 cBuiltin = ImVec4(0.510f, 0.667f, 1.000f, 1.0f); // #82AAFF
    const ImVec4 cFunc    = ImVec4(1.000f, 0.796f, 0.420f, 1.0f); // #FFCB6B

    static const std::unordered_set<std::string> kKeywords = {
        "and", "break", "do", "else", "elseif", "end", "false", "for",
        "function", "goto", "if", "in", "local", "nil", "not", "or",
        "repeat", "return", "then", "true", "until", "while", "continue"};
    static const std::unordered_set<std::string> kBuiltins = {
        "print", "pairs", "ipairs", "next", "tostring", "tonumber", "type",
        "select", "require", "pcall", "xpcall", "error", "assert",
        "setmetatable", "getmetatable", "rawget", "rawset", "rawequal",
        "rawlen", "unpack", "string", "table", "math", "io", "os",
        "coroutine", "utf8", "debug", "package", "load", "loadstring",
        "collectgarbage", "dofile", "_G", "_VERSION",
        "game", "workspace", "script", "Instance", "Vector3", "Vector2",
        "CFrame", "Color3", "UDim2", "UDim", "Enum", "TweenInfo", "task",
        "tick", "wait", "spawn", "delay", "warn", "typeof", "settings",
        "plugin", "shared", "BrickColor", "Rect", "NumberRange",
        "NumberSequence", "ColorSequence", "PhysicalProperties", "Random",
        "DateTime", "Buffer", "Region3", "Region3int16", "Faces", "Axes",
        "HttpService", "UserInputService", "Players", "RunService",
        "StarterGui", "CoreGui", "ReplicatedStorage", "ServerStorage",
        "Lighting", "Teams", "Chat", "TweenService", "Debris",
        "CollectionService", "ContextActionService", "PathfindingService"};

    enum class St { Normal, LineComment, BlockComment, Str, BlockStr };
    St st = St::Normal;
    char quote = 0;
    size_t n = text.size();
    size_t segStart = 0;
    int segLine = 0, segCol = 0;
    int line = 0, col = 0;
    ImVec4 segColor = cDefault;

    auto flush = [&](size_t end) {
      if (end > segStart) {
        dl->AddText(f, fontSize, origin + ImVec2(segCol * charW, segLine * lineH),
                    ImGui::ColorConvertFloat4ToU32(segColor),
                    text.c_str() + segStart, text.c_str() + end);
      }
    };
    auto startSeg = [&](size_t idx, ImVec4 c) {
      segStart = idx; segLine = line; segCol = col; segColor = c;
    };
    auto adv = [&](char ch) {
      if (ch == '\n') { line++; col = 0; }
      else if (ch == '\t') col += 4 - (col % 4);
      else col++;
    };

    startSeg(0, cDefault);
    size_t i = 0;
    while (i < n) {
      char ch = text[i];
      if (st == St::Normal) {
        if (ch == '-' && i + 1 < n && text[i + 1] == '-') {
          flush(i);
          if (i + 3 < n && text[i + 2] == '[' && text[i + 3] == '[') {
            st = St::BlockComment; startSeg(i, cComment);
            adv(text[i]); i++; adv(text[i]); i++; adv(text[i]); i++; adv(text[i]); i++;
          } else {
            st = St::LineComment; startSeg(i, cComment);
            adv(text[i]); i++; adv(text[i]); i++;
          }
        } else if (ch == '"' || ch == '\'') {
          flush(i); st = St::Str; quote = ch; startSeg(i, cString);
          adv(ch); i++;
        } else if (ch == '[' && i + 1 < n && text[i + 1] == '[') {
          flush(i); st = St::BlockStr; startSeg(i, cString);
          adv(text[i]); i++; adv(text[i]); i++;
        } else if (std::isalpha((unsigned char)ch) || ch == '_') {
          size_t s = i; int sl = line, sc = col;
          while (i < n && (std::isalnum((unsigned char)text[i]) || text[i] == '_')) {
            adv(text[i]); i++;
          }
          flush(s);
          std::string word = text.substr(s, i - s);
          ImVec4 c = cDefault;
          if (kKeywords.count(word)) c = cKeyword;
          else if (kBuiltins.count(word)) c = cBuiltin;
          else {
            size_t j = i;
            while (j < n && (text[j] == ' ' || text[j] == '\t')) j++;
            if (j < n && (text[j] == '(' || text[j] == ':')) c = cFunc;
          }
          dl->AddText(f, fontSize, origin + ImVec2(sc * charW, sl * lineH),
                      ImGui::ColorConvertFloat4ToU32(c),
                      text.c_str() + s, text.c_str() + i);
          startSeg(i, cDefault);
        } else if (std::isdigit((unsigned char)ch)) {
          size_t s = i; int sl = line, sc = col;
          if (ch == '0' && i + 1 < n && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
            adv(text[i]); i++; adv(text[i]); i++;
            while (i < n && std::isxdigit((unsigned char)text[i])) { adv(text[i]); i++; }
          } else {
            while (i < n && (std::isdigit((unsigned char)text[i]) || text[i] == '.')) {
              adv(text[i]); i++;
            }
            if (i < n && (text[i] == 'e' || text[i] == 'E')) {
              adv(text[i]); i++;
              if (i < n && (text[i] == '+' || text[i] == '-')) { adv(text[i]); i++; }
              while (i < n && std::isdigit((unsigned char)text[i])) { adv(text[i]); i++; }
            }
          }
          flush(s);
          dl->AddText(f, fontSize, origin + ImVec2(sc * charW, sl * lineH),
                      ImGui::ColorConvertFloat4ToU32(cNumber),
                      text.c_str() + s, text.c_str() + i);
          startSeg(i, cDefault);
        } else if (ch == '\n') {
          flush(i); adv(ch); i++; startSeg(i, cDefault);
        } else {
          adv(ch); i++;
        }
      } else if (st == St::LineComment) {
        if (ch == '\n') { flush(i); st = St::Normal; adv(ch); i++; startSeg(i, cDefault); }
        else { adv(ch); i++; }
      } else if (st == St::BlockComment) {
        if (ch == ']' && i + 1 < n && text[i + 1] == ']') {
          adv(text[i]); i++; adv(text[i]); i++;
          flush(i); st = St::Normal; startSeg(i, cDefault);
        } else if (ch == '\n') { flush(i); adv(ch); i++; startSeg(i, cComment); }
        else { adv(ch); i++; }
      } else if (st == St::Str) {
        if (ch == '\\' && i + 1 < n) { adv(text[i]); i++; adv(text[i]); i++; }
        else if (ch == quote) { adv(ch); i++; flush(i); st = St::Normal; startSeg(i, cDefault); }
        else if (ch == '\n') { flush(i); st = St::Normal; adv(ch); i++; startSeg(i, cDefault); }
        else { adv(ch); i++; }
      } else { // BlockStr
        if (ch == ']' && i + 1 < n && text[i + 1] == ']') {
          adv(text[i]); i++; adv(text[i]); i++;
          flush(i); st = St::Normal; startSeg(i, cDefault);
        } else if (ch == '\n') { flush(i); adv(ch); i++; startSeg(i, cString); }
        else { adv(ch); i++; }
      }
    }
    flush(n);
  }
};
