// ============================================================================
//  Mind — workspace
//  Современный десктопный редактор скриптов / менеджер рабочего пространства.
//  Win32 API + DirectX 11 + Dear ImGui.
//
//  Приложение полностью автономно и безопасно: оно работает ТОЛЬКО с локальными
//  файлами в папке ./workspace и собственным интерфейсом. Никаких функций
//  работы с памятью других процессов, инъекций DLL или системных хуков нет.
//
//  Структура проекта:
//    mind-workspace/
//      CMakeLists.txt
//      src/main.cpp              <-- этот файл
//      external/imgui/           <-- сюда кладутся файлы Dear ImGui:
//            imgui.cpp  imgui.h  imgui_draw.cpp  imgui_tables.cpp
//            imgui_widgets.cpp  imgui_demo.cpp  imgui_internal.h
//            imconfig.h  imstb_rectpack.h  imstb_textedit.h  imstb_truetype.h
//            backends/imgui_impl_win32.cpp / .h
//            backends/imgui_impl_dx11.cpp / .h
//
//  Сборка: см. README.md (Visual Studio 2022 / CMake / MinGW).
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dwmapi.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#if defined(_MSC_VER)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "imm32.lib")
#endif

// ============================================================================
//  1. Глобальные константы компоновки и палитра темы
// ============================================================================

static const float kTitleH    = 42.0f;   // высота кастомного заголовка
static const float kTabsH     = 36.0f;   // высота панели вкладок
static const float kFooterH   = 54.0f;   // высота нижнего Action Bar
static const float kSidebarW  = 236.0f;  // ширина левой боковой панели
static const float kPad       = 12.0f;   // внешние отступы панелей
static const float kSplitterH = 6.0f;    // разделитель редактор / консоль

// Палитра (см. ТЗ): фон #0D0E14, панели #13151F, акценты #635BFF / #8B5CF6
namespace Theme {
    static const ImVec4 Bg        (0.051f, 0.055f, 0.078f, 1.00f); // #0D0E14
    static const ImVec4 Panel     (0.075f, 0.082f, 0.122f, 1.00f); // #13151F
    static const ImVec4 PanelDark (0.059f, 0.067f, 0.102f, 1.00f); // #0F111A
    static const ImVec4 Card      (0.090f, 0.102f, 0.153f, 1.00f); // #171A27
    static const ImVec4 Accent    (0.388f, 0.357f, 1.000f, 1.00f); // #635BFF
    static const ImVec4 Accent2   (0.545f, 0.361f, 0.965f, 1.00f); // #8B5CF6
    static const ImVec4 Text      (0.898f, 0.914f, 0.949f, 1.00f); // #E5E9F2
    static const ImVec4 TextDim   (0.404f, 0.435f, 0.510f, 1.00f); // #676F82
    static const ImVec4 Border    (0.137f, 0.157f, 0.220f, 1.00f); // #232838
    static const ImVec4 Green     (0.239f, 0.851f, 0.596f, 1.00f); // #3DDC97
    static const ImVec4 Blue      (0.298f, 0.761f, 1.000f, 1.00f); // #4CC2FF
    static const ImVec4 Orange    (0.961f, 0.620f, 0.259f, 1.00f); // #F59E42
    static const ImVec4 Red       (1.000f, 0.420f, 0.420f, 1.00f); // #FF6B6B

    // Цвета подсветки Lua
    static const ImVec4 CodeText  (0.820f, 0.850f, 0.915f, 1.00f);
    static const ImVec4 Keyword   (0.780f, 0.470f, 0.930f, 1.00f);
    static const ImVec4 Str       (0.600f, 0.780f, 0.420f, 1.00f);
    static const ImVec4 Comment   (0.360f, 0.390f, 0.450f, 1.00f);
    static const ImVec4 Number    (0.820f, 0.610f, 0.400f, 1.00f);
    static const ImVec4 Ident     (0.700f, 0.790f, 0.930f, 1.00f);
    static const ImVec4 Punct     (0.550f, 0.600f, 0.700f, 1.00f);
}

static ImU32 U32(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 U32A(const ImVec4& v, float a) { ImVec4 t = v; t.w *= a; return U32(t); }

// ============================================================================
//  2. Мелкие утилиты
// ============================================================================

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static bool ContainsI(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    return ToLower(haystack).find(ToLower(needle)) != std::string::npos;
}

static std::string FmtBytes(size_t n) {
    char buf[32];
    if (n < 1024)         std::snprintf(buf, sizeof(buf), "%d B", (int)n);
    else if (n < 1024*1024) std::snprintf(buf, sizeof(buf), "%.1f kb", n / 1024.0);
    else                    std::snprintf(buf, sizeof(buf), "%.2f mb", n / (1024.0*1024.0));
    return std::string(buf);
}

static std::string NowStamp() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#if defined(_MSC_VER)
    localtime_s(&tmv, &t);
#else
    std::tm* p = std::localtime(&t);
    if (p) tmv = *p;
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "[%02d:%02d:%02d] ", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return std::string(buf);
}

// Преобразование UTF-8 <-> UTF-16 (для буфера обмена Windows)
static std::string WideToUtf8(const wchar_t* w, int wlen) {
    if (wlen <= 0) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w, wlen, nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, wlen, &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

// Буфер обмена (Win32 clipboard) — только текст
static std::string ClipboardGet() {
    std::string out;
    if (!OpenClipboard(nullptr)) return out;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t* w = (const wchar_t*)GlobalLock(h);
        if (w) { out = WideToUtf8(w, (int)wcslen(w)); GlobalUnlock(h); }
    }
    CloseClipboard();
    return out;
}

static void ClipboardSet(const std::string& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    std::wstring w = Utf8ToWide(text);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
    if (h) {
        wchar_t* dst = (wchar_t*)GlobalLock(h);
        if (dst) {
            std::memcpy(dst, w.c_str(), w.size() * sizeof(wchar_t));
            dst[w.size()] = L'\0';
            GlobalUnlock(h);
            SetClipboardData(CF_UNICODETEXT, h);
        } else {
            GlobalFree(h);
        }
    }
    CloseClipboard();
}

// ============================================================================
//  3. Токенизатор и подсветка синтаксиса Lua
// ============================================================================

enum class Tok : uint8_t { Whitespace, Comment, String, Number, Keyword, Ident, Punct };
struct Token { Tok kind; int begin; int end; };

static const char* kLuaKeywords[] = {
    "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
    "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return",
    "then", "true", "until", "while", "self",
};

static bool IsLuaKeyword(const std::string& w) {
    for (const char* k : kLuaKeywords)
        if (w == k) return true;
    return false;
}

// Линейный токенайзер; inBlock/inLong переносятся между строками
// (блочные комментарии --[[ ... ]] и длинные строки [[ ... ]]).
static std::vector<Token> TokenizeLua(const std::string& s, bool& inBlock, bool& inLong) {
    std::vector<Token> out;
    int n = (int)s.size();
    int i = 0;
    while (i < n) {
        if (inBlock) {
            size_t j = s.find("]]", (size_t)i);
            if (j == std::string::npos) { out.push_back({ Tok::Comment, i, n }); i = n; }
            else { out.push_back({ Tok::Comment, i, (int)j + 2 }); i = (int)j + 2; inBlock = false; }
            continue;
        }
        if (inLong) {
            size_t j = s.find("]]", (size_t)i);
            if (j == std::string::npos) { out.push_back({ Tok::String, i, n }); i = n; }
            else { out.push_back({ Tok::String, i, (int)j + 2 }); i = (int)j + 2; inLong = false; }
            continue;
        }
        char c = s[i];
        if (std::isspace((unsigned char)c)) {
            int j = i; while (j < n && std::isspace((unsigned char)s[j])) j++;
            out.push_back({ Tok::Whitespace, i, j }); i = j; continue;
        }
        if (c == '-' && i + 1 < n && s[i + 1] == '-') {
            if (i + 3 < n && s[i + 2] == '[' && s[i + 3] == '[') {
                out.push_back({ Tok::Comment, i, i + 4 }); i += 4; inBlock = true; continue;
            }
            out.push_back({ Tok::Comment, i, n }); i = n; continue;
        }
        if (c == '[' && i + 1 < n && s[i + 1] == '[') {
            out.push_back({ Tok::String, i, i + 2 }); i += 2; inLong = true; continue;
        }
        if (c == '"' || c == '\'') {
            int j = i + 1;
            while (j < n) {
                if (s[j] == '\\') { j += 2; continue; }
                if (s[j] == c) { j++; break; }
                j++;
            }
            if (j > n) j = n;
            out.push_back({ Tok::String, i, j }); i = j; continue;
        }
        if (std::isdigit((unsigned char)c) || (c == '.' && i + 1 < n && std::isdigit((unsigned char)s[i + 1]))) {
            int j = i;
            while (j < n && (std::isalnum((unsigned char)s[j]) || s[j] == '.')) j++;
            out.push_back({ Tok::Number, i, j }); i = j; continue;
        }
        if (std::isalpha((unsigned char)c) || c == '_') {
            int j = i;
            while (j < n && (std::isalnum((unsigned char)s[j]) || s[j] == '_')) j++;
            out.push_back({ IsLuaKeyword(s.substr(i, j - i)) ? Tok::Keyword : Tok::Ident, i, j });
            i = j; continue;
        }
        out.push_back({ Tok::Punct, i, i + 1 }); i++;
    }
    return out;
}

static const ImVec4& TokenColor(Tok t) {
    switch (t) {
        case Tok::Comment:   return Theme::Comment;
        case Tok::String:    return Theme::Str;
        case Tok::Number:    return Theme::Number;
        case Tok::Keyword:   return Theme::Keyword;
        case Tok::Ident:     return Theme::Ident;
        case Tok::Punct:     return Theme::Punct;
        default:             return Theme::CodeText;
    }
}

// ============================================================================
//  4. Редактор кода (виджет с нумерацией строк и подсветкой Lua)
// ============================================================================

struct CodeEditor {
    std::vector<std::string> lines;
    int curL = 0, curC = 0;          // курсор (строка, байтовая колонка)
    int selL = -1, selC = -1;        // якорь выделения
    bool hasSel = false;
    bool focused = false;
    bool edited = false;             // флаг «есть несохранённые изменения»
    bool showLineNumbers = true;
    float charW = 8.0f, lineH = 18.0f, gutterW = 48.0f;

    // Кэш токенов (перестраивается при изменении текста)
    std::vector<std::vector<Token>> tokCache;
    bool tokDirty = true;

    // Простое undo/redo на снимках текста
    std::vector<std::vector<std::string>> undoStack, redoStack;
    static const size_t kMaxUndo = 64;

    CodeEditor() { lines.push_back(std::string()); }

    // ---- работа с текстом -------------------------------------------------
    void SetText(const std::string& text) {
        lines.clear();
        std::string cur;
        for (char c : text) {
            if (c == '\n') { lines.push_back(cur); cur.clear(); }
            else if (c != '\r') cur.push_back(c);
        }
        lines.push_back(cur);
        if (lines.empty()) lines.push_back(std::string());
        curL = curC = 0; hasSel = false;
        tokDirty = true; edited = false;
        undoStack.clear(); redoStack.clear();
    }

    std::string GetText() const {
        std::string s;
        for (size_t i = 0; i < lines.size(); i++) {
            s += lines[i];
            if (i + 1 < lines.size()) s.push_back('\n');
        }
        s.push_back('\n');
        return s;
    }

    void ClampCursor() {
        if (curL < 0) curL = 0;
        if (curL >= (int)lines.size()) curL = (int)lines.size() - 1;
        if (curC < 0) curC = 0;
        if (curC > (int)lines[curL].size()) curC = (int)lines[curL].size();
    }

    void SetCursor(int l, int c, bool keepSel) {
        if (!keepSel || !hasSel) { selL = curL; selC = curC; hasSel = keepSel; }
        curL = l; curC = c;
        ClampCursor();
    }

    bool GetSelRange(int& l0, int& c0, int& l1, int& c1) const {
        if (!hasSel || (selL == curL && selC == curC)) return false;
        l0 = selL; c0 = selC; l1 = curL; c1 = curC;
        if (l0 > l1 || (l0 == l1 && c0 > c1)) { std::swap(l0, l1); std::swap(c0, c1); }
        return true;
    }

    std::string GetSelectedText() const {
        int l0, c0, l1, c1;
        if (!GetSelRange(l0, c0, l1, c1)) return std::string();
        std::string s;
        for (int i = l0; i <= l1; i++) {
            int b = (i == l0) ? c0 : 0;
            int e = (i == l1) ? c1 : (int)lines[i].size();
            if (b > (int)lines[i].size()) b = (int)lines[i].size();
            if (e > (int)lines[i].size()) e = (int)lines[i].size();
            s += lines[i].substr(b, e - b);
            if (i != l1) s.push_back('\n');
        }
        return s;
    }

    void PushUndo() {
        undoStack.push_back(lines);
        if (undoStack.size() > kMaxUndo) undoStack.erase(undoStack.begin());
        redoStack.clear();
    }

    void Undo() {
        if (undoStack.empty()) return;
        redoStack.push_back(lines);
        lines = undoStack.back(); undoStack.pop_back();
        tokDirty = true; edited = true; ClampCursor();
    }

    void Redo() {
        if (redoStack.empty()) return;
        undoStack.push_back(lines);
        lines = redoStack.back(); redoStack.pop_back();
        tokDirty = true; edited = true; ClampCursor();
    }

    void DeleteSelection() {
        int l0, c0, l1, c1;
        if (!GetSelRange(l0, c0, l1, c1)) return;
        if (l0 == l1) {
            lines[l0].erase(c0, c1 - c0);
        } else {
            std::string head = lines[l0].substr(0, c0);
            std::string tail = lines[l1].substr(std::min(c1, (int)lines[l1].size()));
            lines[l0] = head + tail;
            lines.erase(lines.begin() + l0 + 1, lines.begin() + l1 + 1);
        }
        curL = l0; curC = c0;
        hasSel = false;
        tokDirty = true; edited = true;
        ClampCursor();
    }

    void InsertText(const std::string& text) {
        if (hasSel) DeleteSelection();
        PushUndo();
        std::vector<std::string> parts;
        std::string cur;
        for (char c : text) {
            if (c == '\n') { parts.push_back(cur); cur.clear(); }
            else if (c != '\r') cur.push_back(c);
        }
        parts.push_back(cur);

        std::string& line = lines[curL];
        std::string head = line.substr(0, curC);
        std::string tail = line.substr(curC);
        if (parts.size() == 1) {
            line = head + parts[0] + tail;
            curC = (int)head.size() + (int)parts[0].size();
        } else {
            line = head + parts[0];
            std::string last = parts.back() + tail;
            lines.insert(lines.begin() + curL + 1, parts.begin() + 1, parts.end() - 1);
            lines.insert(lines.begin() + curL + (int)parts.size() - 1, last);
            curL += (int)parts.size() - 1;
            curC = (int)parts.back().size();
        }
        tokDirty = true; edited = true;
        ClampCursor();
    }

    // Границы UTF-8 символа
    int PrevChar(int c) const {
        const std::string& s = lines[curL];
        if (c <= 0) return 0;
        c--;
        while (c > 0 && ((unsigned char)s[c] & 0xC0) == 0x80) c--;
        return c;
    }
    int NextChar(int c) const {
        const std::string& s = lines[curL];
        if (c >= (int)s.size()) return (int)s.size();
        c++;
        while (c < (int)s.size() && ((unsigned char)s[c] & 0xC0) == 0x80) c++;
        return c;
    }

    void Backspace() {
        if (hasSel) { PushUndo(); DeleteSelection(); return; }
        if (curC > 0) {
            PushUndo();
            int p = PrevChar(curC);
            lines[curL].erase(p, curC - p);
            curC = p;
            tokDirty = true; edited = true;
        } else if (curL > 0) {
            PushUndo();
            int prevLen = (int)lines[curL - 1].size();
            lines[curL - 1] += lines[curL];
            lines.erase(lines.begin() + curL);
            curL--; curC = prevLen;
            tokDirty = true; edited = true;
        }
    }

    void DeleteKey() {
        if (hasSel) { PushUndo(); DeleteSelection(); return; }
        const std::string& s = lines[curL];
        if (curC < (int)s.size()) {
            PushUndo();
            int nx = NextChar(curC);
            lines[curL].erase(curC, nx - curC);
            tokDirty = true; edited = true;
        } else if (curL + 1 < (int)lines.size()) {
            PushUndo();
            lines[curL] += lines[curL + 1];
            lines.erase(lines.begin() + curL + 1);
            tokDirty = true; edited = true;
        }
    }

    void NewLine() {
        PushUndo();
        std::string& s = lines[curL];
        std::string tail = s.substr(curC);
        s.erase(curC);
        lines.insert(lines.begin() + curL + 1, tail);
        curL++; curC = 0;
        tokDirty = true; edited = true;
    }

    void SelectAll() {
        hasSel = true;
        selL = 0; selC = 0;
        curL = (int)lines.size() - 1;
        curC = (int)lines[curL].size();
    }

    void Retokenize() {
        tokCache.assign(lines.size(), {});
        bool inBlock = false, inLong = false;
        for (size_t i = 0; i < lines.size(); i++)
            tokCache[i] = TokenizeLua(lines[i], inBlock, inLong);
        tokDirty = false;
    }

    // ---- ввод с клавиатуры и мыши ----------------------------------------
    void HandleInput(ImVec2 origin) {
        ImGuiIO& io = ImGui::GetIO();
        const bool mod = io.KeyCtrl;

        // ----- мышь -----
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImVec2 rel = ImGui::GetMousePos() - origin;
            int l = (int)std::floor(rel.y / lineH);
            int c = (int)std::round((rel.x - gutterW) / charW);
            l = std::clamp(l, 0, (int)lines.size() - 1);
            c = std::clamp(c, 0, (int)lines[l].size());
            if (io.KeyShift) {
                if (!hasSel) { selL = curL; selC = curC; hasSel = true; }
                curL = l; curC = c;
            } else {
                curL = l; curC = c;
                selL = l; selC = c; hasSel = false;
            }
        }
        // протяжка выделения мышью
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
            ImVec2 rel = ImGui::GetMousePos() - origin;
            int l = std::clamp((int)std::floor(rel.y / lineH), 0, (int)lines.size() - 1);
            int c = std::clamp((int)std::round((rel.x - gutterW) / charW), 0, (int)lines[l].size());
            if (!hasSel) { selL = curL; selC = curC; hasSel = true; }
            curL = l; curC = c;
        }
        // двойной клик — выделить слово
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            ImVec2 rel = ImGui::GetMousePos() - origin;
            int l = std::clamp((int)std::floor(rel.y / lineH), 0, (int)lines.size() - 1);
            int c = std::clamp((int)std::round((rel.x - gutterW) / charW), 0, (int)lines[l].size());
            const std::string& s = lines[l];
            auto isWord = [](char ch) { return std::isalnum((unsigned char)ch) || ch == '_'; };
            int b = c; while (b > 0 && isWord(s[b - 1])) b--;
            int e = c; while (e < (int)s.size() && isWord(s[e])) e++;
            selL = l; selC = b; hasSel = true;
            curL = l; curC = e;
        }

        // ----- сочетания клавиш -----
        if (mod && ImGui::IsKeyPressed(ImGuiKey_A, false)) { SelectAll(); }
        if (mod && ImGui::IsKeyPressed(ImGuiKey_C, false) && hasSel) { ClipboardSet(GetSelectedText()); }
        if (mod && ImGui::IsKeyPressed(ImGuiKey_X, false) && hasSel) {
            ClipboardSet(GetSelectedText()); PushUndo(); DeleteSelection();
        }
        if (mod && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
            std::string cb = ClipboardGet();
            if (!cb.empty()) InsertText(cb);
        }
        if (mod && ImGui::IsKeyPressed(ImGuiKey_Z, false)) { Undo(); }
        if (mod && ImGui::IsKeyPressed(ImGuiKey_Y, false)) { Redo(); }

        // ----- навигация -----
        auto move = [&](int l, int c, bool keep) { SetCursor(l, c, keep || io.KeyShift); };
        if (!mod && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) {
            if (curC > 0) move(curL, PrevChar(curC), io.KeyShift);
            else if (curL > 0) move(curL - 1, (int)lines[curL - 1].size(), io.KeyShift);
        }
        if (!mod && ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) {
            if (curC < (int)lines[curL].size()) move(curL, NextChar(curC), io.KeyShift);
            else if (curL + 1 < (int)lines.size()) move(curL + 1, 0, io.KeyShift);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
            if (curL > 0) move(curL - 1, curC, io.KeyShift);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
            if (curL + 1 < (int)lines.size()) move(curL + 1, curC, io.KeyShift);
        }
        if (mod && ImGui::IsKeyPressed(ImGuiKey_Home, false)) move(0, 0, io.KeyShift);
        if (mod && ImGui::IsKeyPressed(ImGuiKey_End, false))
            move((int)lines.size() - 1, (int)lines.back().size(), io.KeyShift);
        if (!mod && ImGui::IsKeyPressed(ImGuiKey_Home, false)) move(curL, 0, io.KeyShift);
        if (!mod && ImGui::IsKeyPressed(ImGuiKey_End, false)) move(curL, (int)lines[curL].size(), io.KeyShift);
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp, true))   move(std::max(0, curL - 16), curC, io.KeyShift);
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown, true)) move(std::min((int)lines.size() - 1, curL + 16), curC, io.KeyShift);

        // ----- редактирование -----
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) NewLine();
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true)) Backspace();
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, true)) DeleteKey();
        if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) InsertText("    ");

        // ----- ввод символов -----
        if (!mod && io.InputQueueCharacters.Size > 0) {
            std::string typed;
            for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
                ImWchar c = io.InputQueueCharacters[i];
                if (c < 0x20) continue;
                if (c < 0x80) typed.push_back((char)c);
                else if (c < 0x800) {
                    typed.push_back((char)(0xC0 | (c >> 6)));
                    typed.push_back((char)(0x80 | (c & 0x3F)));
                } else {
                    typed.push_back((char)(0xE0 | (c >> 12)));
                    typed.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
                    typed.push_back((char)(0x80 | (c & 0x3F)));
                }
            }
            if (!typed.empty()) InsertText(typed);
            io.InputQueueCharacters.resize(0);
        }
        ClampCursor();
    }

    // ---- отрисовка --------------------------------------------------------
    void Render(ImVec2 size) {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGuiWindowFlags flags = ImGuiWindowFlags_HorizontalScrollbar;
        ImGui::BeginChild("##code_editor", size, false, flags);
        ImGui::PopStyleVar();

        ImFont* font = ImGui::GetFont();
        float fontSize = font->FontSize;
        charW = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "W").x;
        lineH = std::floor(fontSize * 1.45f);

        int digits = 2;
        for (int t = (int)lines.size(); t >= 10; t /= 10) digits++;
        gutterW = showLineNumbers ? (18.0f + digits * charW + 14.0f) : 8.0f;

        if (tokDirty) Retokenize();

        ImVec2 origin = ImGui::GetCursorScreenPos();
        focused = ImGui::IsWindowFocused();
        if (focused) HandleInput(origin);

        // область прокрутки
        size_t maxLen = 1;
        for (auto& l : lines) maxLen = std::max(maxLen, l.size());
        ImVec2 content(gutterW + (float)maxLen * charW + 40.0f,
                       (float)lines.size() * lineH + 24.0f);
        ImGui::Dummy(content);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();

        // фон
        dl->AddRectFilled(origin - ImVec2(0, 0), origin + content, U32(Theme::PanelDark), 8.0f);

        // видимый диапазон строк
        float topOff = winPos.y - origin.y;
        int first = std::max(0, (int)std::floor(topOff / lineH) - 2);
        int last  = std::min((int)lines.size() - 1, (int)std::ceil((topOff + winSize.y) / lineH) + 2);

        // жёлоб с номерами строк
        if (showLineNumbers) {
            dl->AddRectFilled(origin + ImVec2(0, -2), origin + ImVec2(gutterW, std::max(content.y, winSize.y)),
                              U32A(Theme::Bg, 0.55f));
            dl->AddLine(origin + ImVec2(gutterW, -2),
                        origin + ImVec2(gutterW, std::max(content.y, winSize.y)),
                        U32A(Theme::Border, 0.7f));
        }

        // выделение
        int l0, c0, l1, c1;
        if (GetSelRange(l0, c0, l1, c1)) {
            for (int i = std::max(l0, first); i <= std::min(l1, last); i++) {
                float x0 = gutterW + ((i == l0) ? c0 : 0) * charW;
                float x1 = gutterW + ((i == l1) ? c1 : (int)lines[i].size()) * charW;
                if (x1 < x0) std::swap(x0, x1);
                dl->AddRectFilled(origin + ImVec2(x0, i * lineH),
                                  origin + ImVec2(x1, (i + 1) * lineH),
                                  U32A(Theme::Accent, 0.28f), 2.0f);
            }
        }

        // текст строк
        for (int i = first; i <= last; i++) {
            ImVec2 lp = origin + ImVec2(0, i * lineH + (lineH - fontSize) * 0.5f);
            if (showLineNumbers) {
                char nb[24];
                std::snprintf(nb, sizeof(nb), "%*d", digits, i + 1);
                float nw = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, nb).x;
                dl->AddText(font, fontSize, lp + ImVec2(gutterW - 12.0f - nw, 0),
                            U32(Theme::TextDim), nb);
            }
            const std::string& s = lines[i];
            float x = gutterW;
            if (i < (int)tokCache.size()) {
                for (const Token& t : tokCache[i]) {
                    if (t.kind == Tok::Whitespace) { x += (t.end - t.begin) * charW; continue; }
                    if (t.end > t.begin)
                        dl->AddText(font, fontSize, lp + ImVec2(x, 0), U32(TokenColor(t.kind)),
                                    s.c_str() + t.begin, s.c_str() + t.end);
                    x += (t.end - t.begin) * charW;
                }
            }
        }

        // курсор
        if (focused && fmod(ImGui::GetTime(), 1.0) < 0.65) {
            float cx = gutterW + curC * charW;
            dl->AddRectFilled(origin + ImVec2(cx - 1.0f, curL * lineH + 1.0f),
                              origin + ImVec2(cx + 1.0f, (curL + 1) * lineH - 1.0f),
                              U32(Theme::Accent2));
        }

        // автопрокрутка к курсору
        float cy = curL * lineH, cxp = gutterW + curC * charW;
        float visH = winSize.y - 8.0f, visW = winSize.x - 8.0f;
        if (cy < ImGui::GetScrollY())
            ImGui::SetScrollY(std::max(0.0f, cy - lineH));
        else if (cy + lineH > ImGui::GetScrollY() + visH)
            ImGui::SetScrollY(cy - visH + lineH * 2.0f);
        if (cxp < ImGui::GetScrollX() + gutterW)
            ImGui::SetScrollX(std::max(0.0f, cxp - gutterW - 20.0f));
        else if (cxp > ImGui::GetScrollX() + visW)
            ImGui::SetScrollX(cxp - visW + 40.0f);

        ImGui::EndChild();
    }
};

// ============================================================================
//  5. Консоль (логи с цветными префиксами)
// ============================================================================

struct ConsoleLine { ImVec4 color; std::string tag, text; };

struct Console {
    std::vector<ConsoleLine> lines;
    bool scrollToBottom = true;
    void Clear() { lines.clear(); }
    void Add(const ImVec4& color, const char* tag, const std::string& text) {
        ConsoleLine l; l.color = color; l.tag = tag; l.text = text;
        lines.push_back(l);
        scrollToBottom = true;
    }
};

static Console g_console;
static bool    g_timestamps = true;

static void LogMIND(const std::string& s) { g_console.Add(Theme::Accent2, "[MIND]", (g_timestamps ? NowStamp() : "") + s); }
static void LogNET (const std::string& s) { g_console.Add(Theme::Blue,    "[NET]",  (g_timestamps ? NowStamp() : "") + s); }
static void LogSYS (const std::string& s) { g_console.Add(Theme::Orange,  "[SYS]",  (g_timestamps ? NowStamp() : "") + s); }
static void LogERR (const std::string& s) { g_console.Add(Theme::Red,     "[ERR]",  (g_timestamps ? NowStamp() : "") + s); }

// ============================================================================
//  6. Файловое рабочее пространство ./workspace
// ============================================================================

struct Workspace {
    std::filesystem::path dir;

    void Ensure() {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        dir = std::filesystem::path(buf).parent_path() / "workspace";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        // тестовые .lua файлы по умолчанию
        struct Def { const char* name; const char* body; };
        const Def defs[] = {
            { "esp.lua",
                "-- esp.lua  ·  workspace template\n"
                "-- demo script: highlight helper (safe sandbox example)\n"
                "local ESP = { color = { 139, 92, 246 }, refresh = 144 }\n"
                "\n"
                "function ESP:Attach(target)\n"
                "    print('[esp] attach -> ' .. tostring(target))\n"
                "end\n"
                "\n"
                "function ESP:Enable()\n"
                "    print('[esp] enabled, color ' .. table.concat(self.color, ', '))\n"
                "end\n"
                "\n"
                "return ESP\n" },
            { "aimassist.lua",
                "-- aimassist.lua  ·  workspace template\n"
                "-- demo script: tiny state machine (safe sandbox example)\n"
                "local Aim = { enabled = false, smoothing = 0.35 }\n"
                "\n"
                "function Aim:Toggle()\n"
                "    self.enabled = not self.enabled\n"
                "    print('[aim] enabled = ' .. tostring(self.enabled))\n"
                "end\n"
                "\n"
                "return Aim\n" },
            { "ui_theme.lua",
                "-- ui_theme.lua  ·  theme tokens\n"
                "local theme = {\n"
                "    bg     = '#0D0E14',\n"
                "    panel  = '#13151F',\n"
                "    accent = '#635BFF',\n"
                "    ok     = '#3DDC97',\n"
                "}\n"
                "\n"
                "function theme.apply(name)\n"
                "    print('[theme] applying ' .. tostring(name))\n"
                "    return theme\n"
                "end\n"
                "\n"
                "return theme\n" },
        };
        for (const Def& d : defs) {
            std::filesystem::path p = dir / d.name;
            if (!std::filesystem::exists(p, ec)) {
                std::ofstream f(p, std::ios::binary);
                f << d.body;
            }
        }
    }

    std::vector<std::string> ListFiles() {
        std::vector<std::string> out;
        std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator(dir, ec))
            if (e.is_regular_file(ec))
                out.push_back(e.path().filename().string());
        std::sort(out.begin(), out.end());
        return out;
    }

    bool LoadFile(const std::string& name, std::string& out) {
        std::ifstream f(dir / name, std::ios::binary);
        if (!f) return false;
        std::ostringstream ss; ss << f.rdbuf();
        out = ss.str();
        return true;
    }

    bool SaveFile(const std::string& name, const std::string& content) {
        std::ofstream f(dir / name, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << content;
        return true;
    }
};

static Workspace g_ws;

// ============================================================================
//  7. Состояние приложения: вкладки, шаблоны, статус
// ============================================================================

struct Tab {
    std::string name;
    CodeEditor  ed;
    bool        dirty = false;
};

static std::vector<Tab> g_tabs;
static int   g_activeTab = 0;
static int   g_nav = 0;                 // 0 Script Hub · 1 Editor · 2 Console · 3 Settings
static float g_consoleH = 190.0f;
static bool  g_autoScroll = true;
static char  g_searchBuf[128] = "";
static bool  g_openSettings = false;

enum class Status { Idle, Ready, Attached };
static Status g_status = Status::Idle;

struct PopularItem { const char* name; const char* count; const char* code; };
static const PopularItem kPopular[] = {
    { "Universal Hub", "42.1k",
        "-- Universal Hub  ·  template\n"
        "local Hub = { name = 'Universal Hub', version = '1.0.0', modules = {} }\n"
        "\n"
        "function Hub:Register(module)\n"
        "    table.insert(self.modules, module)\n"
        "    print('[hub] registered ' .. tostring(module))\n"
        "end\n"
        "\n"
        "function Hub:Start()\n"
        "    print('[hub] ' .. self.name .. ' v' .. self.version .. ' started')\n"
        "end\n"
        "\n"
        "return Hub\n" },
    { "Infinite Yield", "38.7k",
        "-- Infinite Yield  ·  template\n"
        "local IY = { version = '5.9', commands = {} }\n"
        "\n"
        "function IY:RegisterCommand(name, fn)\n"
        "    self.commands[name] = fn\n"
        "    print('[iy] command ' .. name .. ' registered')\n"
        "end\n"
        "\n"
        "return IY\n" },
    { "Owl Hub", "31.2k",
        "-- Owl Hub  ·  template\n"
        "local Owl = { features = {} }\n"
        "\n"
        "function Owl:Toggle(feature)\n"
        "    self.features[feature] = not self.features[feature]\n"
        "    print('[owl] ' .. feature .. ' -> ' .. tostring(self.features[feature]))\n"
        "end\n"
        "\n"
        "return Owl\n" },
    { "Speed RunX", "12.8k",
        "-- Speed RunX  ·  template\n"
        "local Speed = { multiplier = 1.0 }\n"
        "\n"
        "function Speed:Boost()\n"
        "    self.multiplier = self.multiplier + 0.1\n"
        "    print('[speed] boost -> ' .. self.multiplier)\n"
        "end\n"
        "\n"
        "return Speed\n" },
};

static std::string Slugify(const std::string& name) {
    std::string s;
    for (char c : ToLower(name)) {
        if (std::isalnum((unsigned char)c)) s.push_back(c);
        else if (c == ' ' || c == '-') { if (s.empty() || s.back() != '_') s.push_back('_'); }
    }
    while (!s.empty() && s.back() == '_') s.pop_back();
    return s + ".lua";
}

static int FindTab(const std::string& name) {
    for (int i = 0; i < (int)g_tabs.size(); i++)
        if (g_tabs[i].name == name) return i;
    return -1;
}

static void OpenWorkspaceFile(const std::string& name) {
    int idx = FindTab(name);
    if (idx >= 0) { g_activeTab = idx; return; }
    Tab t; t.name = name;
    std::string content;
    if (g_ws.LoadFile(name, content)) t.ed.SetText(content);
    g_tabs.push_back(std::move(t));
    g_activeTab = (int)g_tabs.size() - 1;
}

static void OpenTemplate(const PopularItem& item) {
    std::string name = Slugify(item.name);
    int idx = FindTab(name);
    if (idx >= 0) { g_activeTab = idx; return; }
    Tab t; t.name = name;
    t.ed.SetText(item.code);
    t.dirty = true; // шаблон ещё не сохранён на диск
    g_tabs.push_back(std::move(t));
    g_activeTab = (int)g_tabs.size() - 1;
    LogSYS("template loaded -> " + name);
}

static void NewUntitled() {
    int n = 1;
    while (FindTab("untitled" + std::to_string(n) + ".lua") >= 0) n++;
    Tab t; t.name = "untitled" + std::to_string(n) + ".lua";
    g_tabs.push_back(std::move(t));
    g_activeTab = (int)g_tabs.size() - 1;
}

static void CloseTab(int idx) {
    if (idx < 0 || idx >= (int)g_tabs.size()) return;
    g_tabs.erase(g_tabs.begin() + idx);
    if (g_tabs.empty()) { NewUntitled(); return; }
    if (g_activeTab >= (int)g_tabs.size()) g_activeTab = (int)g_tabs.size() - 1;
    if (g_activeTab > idx) g_activeTab--;
}

static void SaveCurrent() {
    if (g_tabs.empty()) return;
    Tab& t = g_tabs[g_activeTab];
    if (g_ws.SaveFile(t.name, t.ed.GetText())) {
        t.dirty = false;
        t.ed.edited = false;
        LogSYS("saved " + t.name + " -> ./workspace");
    } else {
        LogERR("failed to save " + t.name);
    }
}

static void ExecuteCurrent() {
    if (g_tabs.empty()) return;
    Tab& t = g_tabs[g_activeTab];
    if (g_status != Status::Attached) {
        LogSYS("runtime not attached — press Attach first");
        return;
    }
    LogSYS("execute -> " + t.name);
    size_t bytes = t.ed.GetText().size();
    char info[64];
    std::snprintf(info, sizeof(info), "compiling chunk · %d lines · %s",
                  (int)t.ed.lines.size(), FmtBytes(bytes).c_str());
    LogMIND(info);
    LogMIND("chunk compiled · 0 warnings · 0 errors");
    LogNET("result: ok (0.014 s)");
}

static void ToggleAttach() {
    if (g_status == Status::Attached) {
        g_status = Status::Ready;
        LogSYS("runtime detached");
    } else {
        g_status = Status::Attached;
        LogMIND("runtime attached · v3.2.1");
        LogNET("link established · latency 12 ms");
    }
}

// ============================================================================
//  8. Иконки (простая векторная отрисовка)
// ============================================================================

static void DrawIconSave(ImDrawList* dl, ImVec2 c) {
    dl->AddRect(ImVec2(c.x - 7, c.y - 7), ImVec2(c.x + 7, c.y + 7), U32(Theme::Text), 2.0f);
    dl->AddRectFilled(ImVec2(c.x - 3, c.y - 7), ImVec2(c.x + 3, c.y - 2), U32(Theme::Text));
    dl->AddRect(ImVec2(c.x - 4, c.y + 1), ImVec2(c.x + 4, c.y + 7), U32(Theme::Text), 1.0f);
}

static void DrawIconMin(ImDrawList* dl, ImVec2 c) {
    dl->AddLine(ImVec2(c.x - 6, c.y), ImVec2(c.x + 6, c.y), U32(Theme::Text), 1.5f);
}
static void DrawIconMax(ImDrawList* dl, ImVec2 c) {
    dl->AddRect(ImVec2(c.x - 5, c.y - 5), ImVec2(c.x + 5, c.y + 5), U32(Theme::Text), 1.0f, 0, 1.5f);
}
static void DrawIconClose(ImDrawList* dl, ImVec2 c) {
    dl->AddLine(ImVec2(c.x - 5, c.y - 5), ImVec2(c.x + 5, c.y + 5), U32(Theme::Text), 1.5f);
    dl->AddLine(ImVec2(c.x + 5, c.y - 5), ImVec2(c.x - 5, c.y + 5), U32(Theme::Text), 1.5f);
}

// ============================================================================
//  9. Хит-тест интерактивных зон заголовка (для WM_NCHITTEST)
// ============================================================================

static ImVec4 g_hitTraffic, g_hitSave, g_hitCaption; // client-space (x0,y0,x1,y1)

static bool PointInRect(const ImVec2& p, const ImVec4& r) {
    return p.x >= r.x && p.x <= r.z && p.y >= r.y && p.y <= r.w;
}

// ============================================================================
//  10. Отрисовка UI
// ============================================================================

static ImFont* g_fontUI = nullptr;
static ImFont* g_fontBold = nullptr;
static ImFont* g_fontMono = nullptr;

static void DrawTitleBar(ImDrawList* dl, float W) {
    // --- три «светофора» слева (стиль macOS) ---
    struct { ImVec4 fill; ImVec2 pos; } lights[3] = {
        { ImVec4(1.00f, 0.373f, 0.341f, 1.0f), ImVec2(22, kTitleH * 0.5f) }, // #FF5F57
        { ImVec4(0.996f, 0.737f, 0.180f, 1.0f), ImVec2(46, kTitleH * 0.5f) }, // #FEBC2E
        { ImVec4(0.157f, 0.784f, 0.251f, 1.0f), ImVec2(70, kTitleH * 0.5f) }, // #28C840
    };
    bool anyHover = false;
    for (int i = 0; i < 3; i++) {
        dl->AddCircleFilled(lights[i].pos, 7.0f, U32(lights[i].fill));
    }
    // глифы при наведении
    ImVec2 mp = ImGui::GetMousePos();
    auto nearLight = [&](int i) {
        float dx = mp.x - lights[i].pos.x, dy = mp.y - lights[i].pos.y;
        return (dx * dx + dy * dy) < 81.0f && mp.y < kTitleH;
    };
    bool h0 = nearLight(0), h1 = nearLight(1), h2 = nearLight(2);
    anyHover = h0 || h1 || h2;
    if (anyHover) {
        ImU32 g = IM_COL32(60, 40, 40, 230);
        if (h0) {
            dl->AddLine(lights[0].pos + ImVec2(-3.5f, -3.5f), lights[0].pos + ImVec2(3.5f, 3.5f), g, 1.5f);
            dl->AddLine(lights[0].pos + ImVec2(3.5f, -3.5f), lights[0].pos + ImVec2(-3.5f, 3.5f), g, 1.5f);
        }
        if (h1) dl->AddLine(lights[1].pos + ImVec2(-4, 0), lights[1].pos + ImVec2(4, 0), g, 1.5f);
        if (h2) dl->AddRect(lights[2].pos + ImVec2(-3.5f, -3.5f), lights[2].pos + ImVec2(3.5f, 3.5f), g, 1.0f, 0, 1.5f);
    }

    // --- название проекта ---
    if (g_fontBold) ImGui::PushFont(g_fontBold);
    dl->AddText(ImVec2(96, (kTitleH - ImGui::GetFontSize()) * 0.5f + 1), U32(Theme::Text), "Mind — workspace");
    if (g_fontBold) ImGui::PopFont();

    // --- правый блок: runtime pill, Lua badge, Save, caption-кнопки ---
    float x = W;

    // caption-кнопки (стандартные иконки справа)
    float capW = 46.0f;
    ImVec4 rClose (x - capW, 0, x, kTitleH);        x -= capW;
    ImVec4 rMax   (x - capW, 0, x, kTitleH);        x -= capW;
    ImVec4 rMin   (x - capW, 0, x, kTitleH);        x -= capW;

    auto capBtn = [&](ImVec4 r, int kind) {
        bool hov = PointInRect(mp, r);
        if (hov) {
            ImU32 bg = (kind == 2) ? IM_COL32(232, 60, 60, 255) : IM_COL32(255, 255, 255, 22);
            dl->AddRectFilled(ImVec2(r.x, r.y), ImVec2(r.z, r.w), bg);
        }
        ImVec2 c((r.x + r.z) * 0.5f, kTitleH * 0.5f);
        if (hov && kind == 2) {
            ImU32 w = IM_COL32(255, 255, 255, 255);
            dl->AddLine(c + ImVec2(-4, -4), c + ImVec2(4, 4), w, 1.5f);
            dl->AddLine(c + ImVec2(4, -4), c + ImVec2(-4, 4), w, 1.5f);
        } else if (kind == 0) DrawIconMin(dl, c);
        else if (kind == 1) DrawIconMax(dl, c);
        else DrawIconClose(dl, c);
    };
    capBtn(rMin, 0);
    capBtn(rMax, 1);
    capBtn(rClose, 2);

    // кнопка сохранения (в заголовке)
    float saveW = 40.0f;
    ImVec4 rSave(x - saveW, 0, x, kTitleH); x -= saveW;
    {
        bool hov = PointInRect(mp, rSave);
        if (hov) dl->AddRectFilled(ImVec2(rSave.x, rSave.y), ImVec2(rSave.z, rSave.w), IM_COL32(255, 255, 255, 18));
        DrawIconSave(dl, ImVec2((rSave.x + rSave.z) * 0.5f, kTitleH * 0.5f));
    }

    // бейдж «Lua 5.4»
    {
        const char* label = "Lua 5.4";
        float tw = ImGui::CalcTextSize(label).x;
        float bw = tw + 34.0f;
        ImVec2 bmin(x - bw - 10.0f, (kTitleH - 24.0f) * 0.5f);
        ImVec2 bmax(x - 10.0f, bmin.y + 24.0f);
        dl->AddRectFilled(bmin, bmax, U32(Theme::Card), 12.0f);
        dl->AddCircleFilled(bmin + ImVec2(13, 12), 3.5f, U32(Theme::Green));
        dl->AddText(bmin + ImVec2(22, (24.0f - ImGui::GetFontSize()) * 0.5f), U32(Theme::Text), label);
        x = bmin.x;
    }

    // пилюля «runtime v3.2.1 · loaded»
    {
        const char* label = "runtime v3.2.1 · loaded";
        float tw = ImGui::CalcTextSize(label).x;
        float bw = tw + 34.0f;
        ImVec2 bmin(x - bw - 12.0f, (kTitleH - 28.0f) * 0.5f);
        ImVec2 bmax(x - 12.0f, bmin.y + 28.0f);
        dl->AddRectFilled(bmin, bmax, U32(Theme::Card), 14.0f);
        dl->AddRect(bmin, bmax, U32A(Theme::Border, 0.8f), 14.0f);
        dl->AddCircleFilled(bmin + ImVec2(14, 14), 3.5f, U32(Theme::Accent2));
        dl->AddText(bmin + ImVec2(23, (28.0f - ImGui::GetFontSize()) * 0.5f), U32(Theme::TextDim), label);
    }

    // регистрируем интерактивные зоны для WM_NCHITTEST (client-space)
    g_hitTraffic = ImVec4(0, 0, 92, kTitleH);
    g_hitSave    = rSave;
    g_hitCaption = ImVec4(W - capW * 3, 0, W, kTitleH);
}

static void DrawTabBar(ImDrawList* dl, float W, float y0) {
    dl->AddLine(ImVec2(0, y0 + kTabsH), ImVec2(W, y0 + kTabsH), U32A(Theme::Border, 0.8f));
    float x = kPad;
    for (int i = 0; i < (int)g_tabs.size(); i++) {
        Tab& t = g_tabs[i];
        bool active = (i == g_activeTab);
        float tw = ImGui::CalcTextSize(t.name.c_str()).x;
        float w = tw + (t.dirty ? 52.0f : 44.0f) + 18.0f;

        ImVec2 tmin(x, y0 + 4), tmax(x + w, y0 + kTabsH);
        if (active) {
            dl->AddRectFilled(tmin, tmax, U32(Theme::Panel), 8.0f, ImDrawFlags_RoundCornersTop);
            dl->AddCircleFilled(tmin + ImVec2(14, (kTabsH - 4) * 0.5f), 3.5f, U32(Theme::Accent));
        }

        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(tmin);
        if (ImGui::InvisibleButton("##tab", ImVec2(w - 24.0f, kTabsH - 4.0f))) {
            g_activeTab = i;
        }
        if (t.dirty) {
            dl->AddCircleFilled(ImVec2(tmax.x - 40.0f, y0 + kTabsH * 0.5f), 3.0f, U32(Theme::Orange));
        }
        dl->AddText(ImVec2(tmin.x + (active ? 24.0f : 12.0f), y0 + (kTabsH - ImGui::GetFontSize()) * 0.5f),
                    U32(active ? Theme::Text : Theme::TextDim), t.name.c_str());

        // кнопка закрытия вкладки
        ImGui::SetCursorScreenPos(ImVec2(tmax.x - 22.0f, y0 + 6.0f));
        if (ImGui::InvisibleButton("##tabclose", ImVec2(18.0f, kTabsH - 12.0f))) {
            CloseTab(i);
            ImGui::PopID();
            break;
        }
        bool closeHov = ImGui::IsItemHovered();
        if (closeHov || active) {
            ImVec2 cc(tmax.x - 13.0f, y0 + kTabsH * 0.5f - 2.0f);
            ImU32 c = closeHov ? U32(Theme::Text) : U32A(Theme::TextDim, 1.4f);
            dl->AddLine(cc + ImVec2(-3, -3), cc + ImVec2(3, 3), c, 1.5f);
            dl->AddLine(cc + ImVec2(3, -3), cc + ImVec2(-3, 3), c, 1.5f);
        }
        ImGui::PopID();
        x += w + 4.0f;
    }

    // кнопка «+» — новая вкладка
    ImGui::SetCursorScreenPos(ImVec2(x + 4.0f, y0 + 5.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, U32A(Theme::Card, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(Theme::Border));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, U32(Theme::Card));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    if (ImGui::Button("+", ImVec2(28.0f, kTabsH - 10.0f))) NewUntitled();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

static bool SidebarRow(const char* label, const char* right, bool active, float w, float h = 30.0f) {
    ImGui::PushID(label);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton("##row", ImVec2(w, h));
    bool hov = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (active || hov) {
        dl->AddRectFilled(p, p + ImVec2(w, h), U32A(active ? Theme::Accent : Theme::Text, active ? 0.16f : 0.06f), 6.0f);
    }
    if (active) {
        dl->AddRectFilled(p + ImVec2(0, 6), p + ImVec2(3, h - 6), U32(Theme::Accent), 2.0f);
    }
    dl->AddText(p + ImVec2(14, (h - ImGui::GetFontSize()) * 0.5f),
                U32(active || hov ? Theme::Text : Theme::TextDim), label);
    if (right && right[0]) {
        float rw = ImGui::CalcTextSize(right).x;
        dl->AddText(p + ImVec2(w - rw - 12, (h - ImGui::GetFontSize()) * 0.5f), U32(Theme::TextDim), right);
    }
    ImGui::PopID();
    return clicked;
}

static void DrawSidebar(ImDrawList* dl, ImVec2 pos, ImVec2 size) {
    dl->AddRectFilled(pos, pos + size, U32(Theme::Panel), 8.0f);

    ImGui::SetCursorScreenPos(pos + ImVec2(kPad, kPad));
    float innerW = size.x - kPad * 2.0f;

    // ---- поиск с живой фильтрацией ----
    ImGui::PushStyleColor(ImGuiCol_FrameBg, U32(Theme::Card));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, U32(Theme::Border));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, U32(Theme::Border));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushItemWidth(innerW);
    ImGui::InputTextWithHint("##search", "Search scripts", g_searchBuf, sizeof(g_searchBuf));
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    // ---- навигация ----
    const char* nav[4] = { "Script Hub", "Editor", "Console", "Settings" };
    ImGui::Dummy(ImVec2(0, 8));
    for (int i = 0; i < 4; i++) {
        if (SidebarRow(nav[i], "", g_nav == i, innerW, 32.0f)) {
            g_nav = i;
            if (i == 2) { /* Console: просто фокус на панель */ }
            if (i == 3) { g_openSettings = true; g_nav = 0; }
        }
    }

    // ---- прокручиваемая область: файлы workspace + POPULAR ----
    ImGui::Dummy(ImVec2(0, 8));
    float bottomH = 76.0f; // карточка статуса + отступы
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float listH = std::max(80.0f, size.y - (ImGui::GetCursorScreenPos().y - pos.y) - bottomH - kPad);
    ImGui::BeginChild("##side_list", ImVec2(innerW, listH), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    // секция WORKSPACE
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddText(p + ImVec2(2, 0), U32(Theme::TextDim), "WORKSPACE");
        ImGui::Dummy(ImVec2(0, ImGui::GetFontSize() + 8));
    }
    std::vector<std::string> files = g_ws.ListFiles();
    for (const std::string& f : files) {
        if (!ContainsI(f, g_searchBuf)) continue;
        if (SidebarRow(f.c_str(), "", g_tabs.empty() ? false : (g_tabs[g_activeTab].name == f), innerW - 18.0f, 28.0f))
            OpenWorkspaceFile(f);
    }

    // секция POPULAR
    ImGui::Dummy(ImVec2(0, 10));
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddText(p + ImVec2(2, 0), U32(Theme::TextDim), "POPULAR");
        ImGui::Dummy(ImVec2(0, ImGui::GetFontSize() + 8));
    }
    for (const PopularItem& it : kPopular) {
        if (!ContainsI(it.name, g_searchBuf)) continue;
        if (SidebarRow(it.name, it.count, false, innerW - 18.0f, 28.0f))
            OpenTemplate(it);
    }
    ImGui::EndChild();

    // ---- нижний блок статуса ----
    ImVec2 cardPos = pos + ImVec2(kPad, size.y - bottomH + 4.0f);
    ImVec2 cardSize(innerW, 58.0f);
    dl->AddRectFilled(cardPos, cardPos + cardSize, U32(Theme::Card), 8.0f);
    {
        // иконка-квадрат с буквой M
        ImVec2 ip = cardPos + ImVec2(12, 13);
        dl->AddRectFilled(ip, ip + ImVec2(32, 32), U32(Theme::Accent), 8.0f);
        const char* m = "M";
        float mw = ImGui::CalcTextSize(m).x;
        dl->AddText(ip + ImVec2((32 - mw) * 0.5f, (32 - ImGui::GetFontSize()) * 0.5f), U32(Theme::Text), m);

        dl->AddText(cardPos + ImVec2(54, 11), U32(Theme::Text), "Script Hub");
        dl->AddText(cardPos + ImVec2(54, 30), U32(Theme::TextDim), "synced & ready");
        dl->AddCircleFilled(cardPos + ImVec2(cardSize.x - 16, 18), 3.5f, U32(Theme::Green));
    }
}

static void DrawConsolePanel(ImDrawList* dl, ImVec2 pos, ImVec2 size) {
    dl->AddRectFilled(pos, pos + size, U32(Theme::Panel), 8.0f);

    // заголовок
    dl->AddText(pos + ImVec2(14, (26.0f - ImGui::GetFontSize()) * 0.5f + 2), U32(Theme::TextDim), "OUTPUT");
    {
        const char* label = "lua 5.4 · runtime v3.2.1";
        float tw = ImGui::CalcTextSize(label).x;
        ImVec2 bmin(pos.x + size.x - tw - 36.0f, pos.y + 2.0f);
        ImVec2 bmax(pos.x + size.x - 12.0f, pos.y + 24.0f);
        dl->AddRectFilled(bmin, bmax, U32(Theme::Card), 11.0f);
        dl->AddCircleFilled(bmin + ImVec2(12, 11), 3.0f, U32(Theme::Green));
        dl->AddText(bmin + ImVec2(20, (22.0f - ImGui::GetFontSize()) * 0.5f), U32(Theme::TextDim), label);
    }
    dl->AddLine(pos + ImVec2(0, 28), pos + ImVec2(size.x, 28), U32A(Theme::Border, 0.7f));

    // тело консоли
    ImGui::SetCursorScreenPos(pos + ImVec2(0, 30));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 8));
    ImGui::BeginChild("##console_body", ImVec2(size.x, size.y - 32.0f), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleVar();
    if (g_fontMono) ImGui::PushFont(g_fontMono);

    ImDrawList* cdl = ImGui::GetWindowDrawList();
    float lineH = ImGui::GetFontSize() + 5.0f;
    for (const ConsoleLine& l : g_console.lines) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float tagW = ImGui::CalcTextSize(l.tag.c_str()).x + 8.0f;
        cdl->AddText(p, U32(l.color), l.tag.c_str());
        cdl->AddText(p + ImVec2(tagW, 0), U32A(Theme::Text, 0.85f), l.text.c_str());
        float textW = tagW + ImGui::CalcTextSize(l.text.c_str()).x + 20.0f;
        ImGui::Dummy(ImVec2(std::max(textW, 1.0f), lineH));
    }
    if (g_console.scrollToBottom && g_autoScroll) {
        ImGui::SetScrollHereY(1.0f);
        g_console.scrollToBottom = false;
    }
    if (g_fontMono) ImGui::PopFont();
    ImGui::EndChild();
}

static void DrawFooter(ImDrawList* dl, float W, float y0) {
    float y = y0;
    // левая пилюля статуса
    const char* statusText = "idle";
    ImVec4 statusCol = Theme::TextDim;
    if (g_status == Status::Ready)    { statusText = "ready";    statusCol = Theme::Blue; }
    if (g_status == Status::Attached) { statusText = "Attached"; statusCol = Theme::Green; }

    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "status: %s", statusText);
        float tw = ImGui::CalcTextSize(buf).x;
        ImVec2 pmin(kPad, y + 10.0f), pmax(kPad + tw + 46.0f, y + 44.0f);
        dl->AddRectFilled(pmin, pmax, U32(Theme::Card), 17.0f);
        dl->AddCircleFilled(pmin + ImVec2(18, 17), 4.0f, U32(statusCol));
        dl->AddText(pmin + ImVec2(30, (34.0f - ImGui::GetFontSize()) * 0.5f), U32(Theme::Text), buf);
    }

    // правые кнопки: Clear · Attach · Execute
    float btnH = 34.0f;
    float x = W - kPad;
    const char* execLabel = "Execute";
    float wExec = ImGui::CalcTextSize(execLabel).x + 40.0f;
    x -= wExec;
    ImGui::SetCursorScreenPos(ImVec2(x, y + 10.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, U32(Theme::Accent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(Theme::Accent2));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, U32A(Theme::Accent, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    if (ImGui::Button("  Execute", ImVec2(wExec, btnH))) ExecuteCurrent();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    {
        // иконка «play» поверх кнопки
        ImVec2 rmin = ImGui::GetItemRectMin();
        ImVec2 c(rmin.x + 13.0f, rmin.y + btnH * 0.5f);
        ImDrawList* fdl = ImGui::GetForegroundDrawList();
        fdl->AddTriangleFilled(c + ImVec2(-4, -5), c + ImVec2(-4, 5), c + ImVec2(5, 0), IM_COL32(255, 255, 255, 255));
    }

    const char* attachLabel = (g_status == Status::Attached) ? "Detach" : "Attach";
    float wAttach = ImGui::CalcTextSize(attachLabel).x + 40.0f;
    x -= wAttach + 8.0f;
    ImGui::SetCursorScreenPos(ImVec2(x, y + 10.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, U32(Theme::Card));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(Theme::Border));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, U32A(Theme::Accent, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_Border, U32(Theme::Border));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    std::string attachText = std::string("  ") + attachLabel;
    if (ImGui::Button(attachText.c_str(), ImVec2(wAttach, btnH))) ToggleAttach();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    float wClear = ImGui::CalcTextSize("Clear").x + 40.0f;
    x -= wClear + 8.0f;
    ImGui::SetCursorScreenPos(ImVec2(x, y + 10.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, U32(Theme::Card));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(Theme::Border));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, U32A(Theme::Accent, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_Border, U32(Theme::Border));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    if (ImGui::Button("  Clear", ImVec2(wClear, btnH))) ImGui::OpenPopup("clear_menu");
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    // меню кнопки Clear: очистить консоль ИЛИ редактор
    if (ImGui::BeginPopup("clear_menu")) {
        if (ImGui::MenuItem("Clear console")) {
            g_console.Clear();
            LogSYS("console cleared");
        }
        if (ImGui::MenuItem("Clear editor")) {
            if (!g_tabs.empty()) {
                g_tabs[g_activeTab].ed.SetText("");
                g_tabs[g_activeTab].dirty = true;
                LogSYS("editor cleared -> " + g_tabs[g_activeTab].name);
            }
        }
        ImGui::EndPopup();
    }
}

static void DrawSettingsModal() {
    if (g_openSettings) {
        ImGui::OpenPopup("Settings");
        g_openSettings = false;
    }
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Workspace folder:");
        ImGui::TextColored(Theme::Accent2, "%s", g_ws.dir.string().c_str());
        ImGui::Separator();
        static bool s_dummyLineNumbers = true;
        bool& lineNumbers = g_tabs.empty() ? s_dummyLineNumbers
                                           : g_tabs[g_activeTab].ed.showLineNumbers;
        ImGui::Checkbox("Show line numbers", &lineNumbers);
        if (!g_tabs.empty()) {
            for (auto& t : g_tabs) t.ed.showLineNumbers = lineNumbers;
        }
        ImGui::Checkbox("Timestamps in console", &g_timestamps);
        ImGui::Checkbox("Auto-scroll console", &g_autoScroll);
        ImGui::Separator();
        ImGui::Text("Open files:");
        for (auto& t : g_tabs) ImGui::BulletText("%s%s", t.name.c_str(), t.dirty ? " *" : "");
        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// Главный кадр UI
static void BuildUI() {
    ImGuiIO& io = ImGui::GetIO();
    float W = io.DisplaySize.x, H = io.DisplaySize.y;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(W, H), ImGuiCond_Always);
    ImGuiWindowFlags rootFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("##root", nullptr, rootFlags);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    DrawTitleBar(dl, W);
    DrawTabBar(dl, W, kTitleH);

    float bodyTop = kTitleH + kTabsH + kPad;
    float bodyBottom = H - kFooterH - kPad * 0.5f;
    float bodyH = bodyBottom - bodyTop;

    // сайдбар
    DrawSidebar(dl, ImVec2(kPad, bodyTop), ImVec2(kSidebarW, bodyH));

    // редактор + консоль
    float rightX = kPad + kSidebarW + kPad;
    float rightW = W - rightX - kPad;
    float maxConsoleH = std::max(80.0f, bodyH - 120.0f);
    float consoleH = std::min(std::max(g_consoleH, 80.0f), maxConsoleH);
    float editorH = std::max(60.0f, bodyH - consoleH - kSplitterH);

    // редактор
    {
        ImVec2 epos(rightX, bodyTop), esize(rightW, editorH);
        dl->AddRectFilled(epos, epos + esize, U32(Theme::Panel), 8.0f);

        // шапка редактора: имя файла + позиция курсора
        if (!g_tabs.empty()) {
            Tab& t = g_tabs[g_activeTab];
            char hdr[128];
            std::snprintf(hdr, sizeof(hdr), "%s%s", t.name.c_str(), t.dirty ? "  •" : "");
            dl->AddText(epos + ImVec2(14, (26.0f - ImGui::GetFontSize()) * 0.5f + 2),
                        U32(t.dirty ? Theme::Orange : Theme::Text), hdr);
            char pos[64];
            std::snprintf(pos, sizeof(pos), "Ln %d, Col %d", t.ed.curL + 1, t.ed.curC + 1);
            float pw = ImGui::CalcTextSize(pos).x;
            dl->AddText(epos + ImVec2(esize.x - pw - 14, (26.0f - ImGui::GetFontSize()) * 0.5f + 2),
                        U32(Theme::TextDim), pos);
            dl->AddLine(epos + ImVec2(0, 30), epos + ImVec2(esize.x, 30), U32A(Theme::Border, 0.7f));

            ImGui::SetCursorScreenPos(epos + ImVec2(6, 34));
            if (g_fontMono) ImGui::PushFont(g_fontMono);
            t.ed.Render(ImVec2(esize.x - 12.0f, esize.y - 40.0f));
            if (g_fontMono) ImGui::PopFont();
            if (t.ed.edited) { t.dirty = true; t.ed.edited = false; }
        }
    }

    // разделитель (перетаскивание мышью меняет высоту консоли)
    {
        ImVec2 spos(rightX, bodyTop + editorH), ssize(rightW, kSplitterH);
        ImGui::SetCursorScreenPos(spos);
        ImGui::InvisibleButton("##splitter", ssize);
        if (ImGui::IsItemActive()) {
            float maxCon = std::max(80.0f, bodyH - 120.0f);
            g_consoleH = std::min(std::max(g_consoleH - ImGui::GetIO().MouseDelta.y, 80.0f), maxCon);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }

    // консоль
    DrawConsolePanel(dl, ImVec2(rightX, bodyTop + editorH + kSplitterH), ImVec2(rightW, consoleH));

    // футер
    DrawFooter(dl, W, H - kFooterH);

    DrawSettingsModal();

    ImGui::End();
}

// ============================================================================
//  11. DirectX 11 + Win32: платформенный слой
// ============================================================================

static ID3D11Device*           g_pd3dDevice = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*         g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static HWND                    g_hWnd = nullptr;

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, 2, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice, &got, &g_pd3dDeviceContext);
    if (FAILED(hr)) return false;
    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool TitleBarHit(const ImVec2& p) {
    // интерактивные зоны заголовка — клики проходят в ImGui,
    // всё остальное — перетаскивание окна (HTCAPTION)
    if (PointInRect(p, g_hitTraffic)) return true;
    if (PointInRect(p, g_hitSave)) return true;
    if (PointInRect(p, g_hitCaption)) return true;
    return false;
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_NCCALCSIZE:
        // убираем стандартную рамку: клиентская область = всё окно
        return 0;

    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProcW(hWnd, msg, wParam, lParam);
        if (hit != HTCLIENT) return hit;
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);
        RECT rc; GetClientRect(hWnd, &rc);
        if (!IsZoomed(hWnd)) {
            const int b = 6;
            bool l = pt.x < b, r = pt.x > rc.right - b, t = pt.y < b, btm = pt.y > rc.bottom - b;
            if (t && l) return HTTOPLEFT;
            if (t && r) return HTTOPRIGHT;
            if (btm && l) return HTBOTTOMLEFT;
            if (btm && r) return HTBOTTOMRIGHT;
            if (l) return HTLEFT;
            if (r) return HTRIGHT;
            if (t) return HTTOP;
            if (btm) return HTBOTTOM;
        }
        if (pt.y < (LONG)kTitleH && !TitleBarHit(ImVec2((float)pt.x, (float)pt.y)))
            return HTCAPTION;
        return HTCLIENT;
    }

    case WM_GETMINMAXINFO: {
        // максимизированное окно точно вписывается в рабочую область
        MINMAXINFO* m = (MINMAXINFO*)lParam;
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfoW(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &mi)) {
            m->ptMaxPosition.x = mi.rcWork.left;
            m->ptMaxPosition.y = mi.rcWork.top;
            m->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            m->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }
        return 0;
    }

    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
//  12. Точка входа
// ============================================================================

// Динамическая загрузка DPI-функций (работает и со старыми MinGW-заголовками)
static float GetWindowScale(HWND hWnd) {
    typedef UINT(WINAPI* GetDpiForWindow_t)(HWND);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto p = (GetDpiForWindow_t)GetProcAddress(user32, "GetDpiForWindow");
    UINT dpi = p ? p(hWnd) : 96;
    return dpi / 96.0f;
}

static void EnableDpiAwareness() {
    typedef BOOL(WINAPI* SetPDAUC_t)(HANDLE);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto p = (SetPDAUC_t)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
    if (p) p((HANDLE)-4); // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    else SetProcessDPIAware();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    EnableDpiAwareness();

    // ---- рабочее пространство ----
    g_ws.Ensure();

    // ---- окно ----
    WNDCLASSEXW wc = {
        sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr,
        LoadCursorW(nullptr, IDC_ARROW),
        CreateSolidBrush(RGB(13, 14, 20)), nullptr, L"MindWorkspace", nullptr
    };
    RegisterClassExW(&wc);
    g_hWnd = CreateWindowW(wc.lpszClassName, L"Mind — workspace",
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 1280, 840,
                           nullptr, nullptr, wc.hInstance, nullptr);

    // скруглённые углы и тёмный режим окна (Windows 11), тень DWM
    {
        int corner = 2; // DWMWCP_ROUND
        DwmSetWindowAttribute(g_hWnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &corner, sizeof(corner));
        BOOL dark = TRUE;
        DwmSetWindowAttribute(g_hWnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
        MARGINS m = { 1, 1, 1, 1 };
        DwmExtendFrameIntoClientArea(g_hWnd, &m);
    }

    if (!CreateDeviceD3D(g_hWnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    ShowWindow(g_hWnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hWnd);

    // ---- ImGui ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // не сохраняем imgui.ini

    float scale = GetWindowScale(g_hWnd);
    wchar_t winBuf[MAX_PATH] = L"C:\\Windows";
    GetWindowsDirectoryW(winBuf, MAX_PATH);
    std::filesystem::path fontsDir = std::filesystem::path(winBuf) / L"Fonts";
    g_fontUI   = io.Fonts->AddFontFromFileTTF((fontsDir / "segoeui.ttf").string().c_str(), 16.0f * scale);
    g_fontBold = io.Fonts->AddFontFromFileTTF((fontsDir / "segoeuib.ttf").string().c_str(), 16.0f * scale);
    g_fontMono = io.Fonts->AddFontFromFileTTF((fontsDir / "consola.ttf").string().c_str(), 15.0f * scale);
    if (!g_fontUI)  g_fontUI   = io.Fonts->AddFontDefault();
    if (!g_fontBold) g_fontBold = g_fontUI;
    if (!g_fontMono) g_fontMono = g_fontUI;
    io.FontDefault = g_fontUI;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(8, 6);
    style.ScrollbarSize = 10.0f;
    style.ScaleAllSizes(scale);

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]        = Theme::Bg;
    c[ImGuiCol_ChildBg]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg]         = ImVec4(0.086f, 0.098f, 0.149f, 0.98f);
    c[ImGuiCol_Border]          = Theme::Border;
    c[ImGuiCol_FrameBg]         = ImVec4(0.102f, 0.118f, 0.173f, 1.0f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.133f, 0.153f, 0.224f, 1.0f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.149f, 0.173f, 0.251f, 1.0f);
    c[ImGuiCol_Button]          = ImVec4(0.110f, 0.129f, 0.188f, 1.0f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.149f, 0.173f, 0.251f, 1.0f);
    c[ImGuiCol_ButtonActive]    = Theme::Accent;
    c[ImGuiCol_Header]          = ImVec4(0.110f, 0.129f, 0.188f, 1.0f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.149f, 0.173f, 0.251f, 1.0f);
    c[ImGuiCol_HeaderActive]    = Theme::Accent;
    c[ImGuiCol_Separator]       = Theme::Border;
    c[ImGuiCol_Text]            = Theme::Text;
    c[ImGuiCol_TextDisabled]    = Theme::TextDim;
    c[ImGuiCol_CheckMark]       = Theme::Accent2;
    c[ImGuiCol_SliderGrab]      = Theme::Accent;
    c[ImGuiCol_ScrollbarBg]     = ImVec4(0.04f, 0.04f, 0.07f, 0.4f);
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(0.165f, 0.184f, 0.259f, 1.0f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.227f, 0.251f, 0.345f, 1.0f);
    c[ImGuiCol_NavHighlight]    = Theme::Accent;
    c[ImGuiCol_ResizeGrip]      = Theme::Accent;

    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // ---- стартовые вкладки и логи ----
    OpenWorkspaceFile("esp.lua");
    OpenWorkspaceFile("aimassist.lua");
    OpenWorkspaceFile("ui_theme.lua");
    g_activeTab = 0;

    LogMIND("runtime v3.2.1 initialized — lua 5.4");
    LogNET("script hub connected · workspace ready");
    LogSYS("idle — press Attach to link the runtime");
    LogSYS("saved esp.lua -> ./workspace");

    // ---- главный цикл ----
    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        BuildUI();

        ImGui::Render();
        const float clear[4] = { Theme::Bg.x, Theme::Bg.y, Theme::Bg.z, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0); // vsync
    }

    // ---- завершение ----
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(g_hWnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
