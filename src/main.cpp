// ---------------------------------------------------------------------------
// Mind Workspace — десктопный редактор скриптов (Win32 + DirectX 11 + Dear ImGui).
// Полностью автономное приложение: только локальные файлы и интерфейс.
// Никакой работы с памятью других процессов, DLL-инъекций и системных хуков.
//
// Структура проекта:
//   src/main.cpp     — Win32/DirectX11 каркас, цикл сообщений, точка входа
//   src/app.h        — состояние приложения (App)
//   src/ui.h         — весь интерфейс (заголовок, вкладки, сайдбар, футер)
//   src/theme.h      — палитра и стиль ImGui
//   src/editor.h     — редактор кода с подсветкой Lua
//   src/console.h    — консоль вывода
//   src/workspace.h  — файловая система ./workspace, вкладки, шаблоны
//
// Dear ImGui НЕ нужно копировать вручную: CMake (FetchContent) скачивает
// его автоматически при первой настройке. См. CMakeLists.txt и README.md.
// ---------------------------------------------------------------------------
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <algorithm>
#include <cstdio>
#include <vector>
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "ui.h"

// Dear ImGui intentionally keeps this platform-backend declaration out of the
// public header to avoid including windows.h for every consumer.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static IDXGISwapChain* g_swapChain = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;

static void CreateRenderTarget() {
  ID3D11Texture2D* backBuffer = nullptr;
  if (FAILED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return;
  g_device->CreateRenderTargetView(backBuffer, nullptr, &g_rtv);
  backBuffer->Release();
}

static void CleanupRenderTarget() {
  if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
}

static bool CreateDeviceD3D(HWND hwnd) {
  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;
  sd.BufferDesc.Height = 0;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = 0;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT flags = 0;
#ifdef _DEBUG
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  const D3D_FEATURE_LEVEL levels[] = {
      D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
  D3D_FEATURE_LEVEL featureLevel;
  auto create = [&](D3D_DRIVER_TYPE driver, UINT createFlags) {
    return D3D11CreateDeviceAndSwapChain(
        nullptr, driver, nullptr, createFlags, levels, 3,
        D3D11_SDK_VERSION, &sd, &g_swapChain, &g_device, &featureLevel, &g_ctx);
  };
  auto releasePartial = []() {
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
    if (g_swapChain) { g_swapChain->Release(); g_swapChain = nullptr; }
    if (g_ctx) { g_ctx->Release(); g_ctx = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
  };

  HRESULT hr = create(D3D_DRIVER_TYPE_HARDWARE, flags);
#ifdef _DEBUG
  // Если Windows не установила Graphics Tools/debug layer, повторяем без неё.
  if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG)) {
    releasePartial();
    flags &= ~D3D11_CREATE_DEVICE_DEBUG;
    hr = create(D3D_DRIVER_TYPE_HARDWARE, flags);
  }
#endif
  // WARP — безопасный программный fallback, если аппаратный D3D недоступен.
  if (FAILED(hr)) {
    releasePartial();
    hr = create(D3D_DRIVER_TYPE_WARP, flags & ~D3D11_CREATE_DEVICE_DEBUG);
  }
  if (FAILED(hr)) {
    releasePartial();
    return false;
  }
  CreateRenderTarget();
  if (!g_rtv) {
    releasePartial();
    return false;
  }
  return true;
}

// Перетаскивание начинается только в пустой части заголовка. Вызов
// WM_NCLBUTTONDOWN передаёт drag системе Windows — это избегает ручного
// SetWindowPos-цикла и сохраняет штатное «прилипание»/возврат окна.
static bool IsTitlebarControlPoint(const ImVec2& point) {
  for (const UiRect& rect : g_titlebarButtons)
    if (rect.Contains(point)) return true;
  return false;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  // Обрабатываем drag до передачи мыши в ImGui, чтобы пустая область
  // заголовка не захватывалась невидимым виджетом.
  if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) {
    POINT screen{};
    if (GetCursorPos(&screen)) {
      POINT client = screen;
      ScreenToClient(hwnd, &client);
      ImVec2 point((float)client.x, (float)client.y);
      if (g_titlebarRect.Contains(point) && !IsTitlebarControlPoint(point)) {
        if (msg == WM_LBUTTONDBLCLK) {
          ToggleMaximize(hwnd);
          return 0;
        }
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION,
                     MAKELPARAM(screen.x, screen.y));
        return 0;
      }
    }
  }

  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;

  switch (msg) {
    case WM_NCHITTEST: {
      POINT pt{(LONG)(short)LOWORD(lParam), (LONG)(short)HIWORD(lParam)};
      // WM_NCHITTEST coordinates are screen-relative; the resize edges below
      // and Dear ImGui item rectangles are client-relative.
      ScreenToClient(hwnd, &pt);

      // WS_THICKFRAME + HT* provide native resize cursors and hit tracking.
      if (!IsZoomed(hwnd)) {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        constexpr int kResizeBorder = 6;
        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;
        const bool left = pt.x < kResizeBorder;
        const bool right = pt.x >= width - kResizeBorder;
        const bool top = pt.y < kResizeBorder;
        const bool bottom = pt.y >= height - kResizeBorder;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
      }
      // Keep the entire custom title bar client-side; blank-space drag is
      // explicitly started above, while buttons remain regular ImGui items.
      return HTCLIENT;
    }
    case WM_NCLBUTTONDBLCLK: {
      POINT pt{(LONG)(short)LOWORD(lParam), (LONG)(short)HIWORD(lParam)};
      ScreenToClient(hwnd, &pt);
      ImVec2 point((float)pt.x, (float)pt.y);
      if (g_titlebarRect.Contains(point) && !IsTitlebarControlPoint(point))
        ToggleMaximize(hwnd);
      return 0;
    }
    case WM_NCCALCSIZE:
      // Клиентская область растягивается на всё окно: системная рамка скрыта.
      if (wParam) return 0;
      break;
    case WM_GETMINMAXINFO: {
      auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
      mmi->ptMinTrackSize.x = 900;
      mmi->ptMinTrackSize.y = 600;
      HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi{};
      mi.cbSize = sizeof(mi);
      if (GetMonitorInfoW(mon, &mi)) {
        mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
        mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
        mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
        mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
      }
      return 0;
    }
    case WM_MOVING: {
      // Оставляем на рабочем столе заголовок и не менее 120 px ширины окна,
      // чтобы его всегда можно было вернуть на экран после перетаскивания.
      auto* moving = reinterpret_cast<RECT*>(lParam);
      POINT cursor{};
      if (GetCursorPos(&cursor)) {
        HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(monitor, &mi)) {
          const int width = moving->right - moving->left;
          const int height = moving->bottom - moving->top;
          const int visibleWidth = (width < 240) ? width / 2 : 120;
          const int leftMin = mi.rcWork.left - width + visibleWidth;
          const int leftMax = mi.rcWork.right - visibleWidth;
          const int topMin = mi.rcWork.top;
          const int topMax = mi.rcWork.bottom - 48;
          const int left = (moving->left < leftMin) ? leftMin
                           : (moving->left > leftMax) ? leftMax
                                                     : moving->left;
          const int top = (moving->top < topMin) ? topMin
                          : (moving->top > topMax) ? topMax
                                                  : moving->top;
          moving->left = left;
          moving->right = left + width;
          moving->top = top;
          moving->bottom = top + height;
        }
      }
      return TRUE;
    }
    case WM_SIZE: {
      if (wParam != SIZE_MINIMIZED && g_device && g_swapChain) {
        UINT w = (UINT)LOWORD(lParam), h = (UINT)HIWORD(lParam);
        if (w > 0 && h > 0) {
          CleanupRenderTarget();
          g_swapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
          CreateRenderTarget();
        }
      }
      return 0;
    }
    case WM_DPICHANGED: {
      RECT* rc = reinterpret_cast<RECT*>(lParam);
      SetWindowPos(hwnd, nullptr, rc->left, rc->top, rc->right - rc->left,
                   rc->bottom - rc->top, SWP_NOZORDER | SWP_NOACTIVATE);
      return 0;
    }
    case WM_SYSCOMMAND: {
      if ((wParam & 0xFFF0) == SC_KEYMENU) return 0; // без меню Alt+Space
      break;
    }
    case WM_ERASEBKGND: return 1;
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  ImGui_ImplWin32_EnableDpiAwareness();

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_CLASSDC | CS_DBLCLKS;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = L"MindWorkspace";
  RegisterClassExW(&wc);

  const int screenW = GetSystemMetrics(SM_CXSCREEN);
  const int screenH = GetSystemMetrics(SM_CYSCREEN);
  const int W = std::min(1280, std::max(900, screenW - 64));
  const int H = std::min(800, std::max(600, screenH - 64));
  const int X = std::max(0, (screenW - W) / 2);
  const int Y = std::max(0, (screenH - H) / 2);
  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Mind \x2014 workspace",
                              WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
                                  WS_VISIBLE | WS_CLIPCHILDREN,
                              X, Y, W, H, nullptr, nullptr, wc.hInstance, nullptr);
  if (!hwnd) return 1;
  // Современные скруглённые углы Windows 11; на более старых Windows
  // неподдерживаемый атрибут просто игнорируется.
  const DWORD roundedCorners = 2; // DWMWCP_ROUND
  DwmSetWindowAttribute(hwnd, static_cast<DWORD>(33),
                        &roundedCorners, sizeof(roundedCorners)); // DWMWA_WINDOW_CORNER_PREFERENCE
  if (!CreateDeviceD3D(hwnd)) {
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 1;
  }
  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;

  App app;
  // GetGlyphRangesCyrillic includes Latin and Cyrillic; add the title's
  // typographic dash explicitly so it is not replaced by a missing-glyph box.
  static ImVector<ImWchar> glyphRangeStorage;
  ImFontGlyphRangesBuilder glyphBuilder;
  glyphBuilder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
  glyphBuilder.AddText("Mind \xE2\x80\x94 workspace Search scripts");
  glyphBuilder.BuildRanges(&glyphRangeStorage);
  const ImWchar* glyphRanges = glyphRangeStorage.Data;

  app.uiFont = io.Fonts->AddFontFromFileTTF(
      "C:\\Windows\\Fonts\\segoeui.ttf", 14.0f, nullptr, glyphRanges);
  if (!app.uiFont) app.uiFont = io.Fonts->AddFontDefault();
  app.uiBoldFont = io.Fonts->AddFontFromFileTTF(
      "C:\\Windows\\Fonts\\seguisb.ttf", 14.0f, nullptr, glyphRanges);
  if (!app.uiBoldFont) app.uiBoldFont = app.uiFont;
  app.monoFont = io.Fonts->AddFontFromFileTTF(
      "C:\\Windows\\Fonts\\consola.ttf", 15.0f, nullptr, glyphRanges);
  if (!app.monoFont) app.monoFont = app.uiFont;
  io.FontDefault = app.uiFont;

  ImGui::StyleColorsDark();
  ApplyTheme(app.accent, app.rounding);
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX11_Init(g_device, g_ctx);
  app.Init();

  MSG msg = {};
  while (msg.message != WM_QUIT) {
    if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
      continue;
    }
    if (IsIconic(hwnd)) {
      WaitMessage();
      continue;
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    app.Render(hwnd);
    ImGui::Render();

    const ImVec4 clearColor = Theme::BgDark;
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_ctx->ClearRenderTargetView(g_rtv, reinterpret_cast<const float*>(&clearColor));
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swapChain->Present(1, 0); // vsync
  }

  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  CleanupRenderTarget();
  if (g_swapChain) g_swapChain->Release();
  if (g_ctx) g_ctx->Release();
  if (g_device) g_device->Release();
  DestroyWindow(hwnd);
  UnregisterClassW(wc.lpszClassName, wc.hInstance);
  return 0;
}
