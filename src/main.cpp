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

// Borderless-окно: перетаскивание за заголовок, ресайз за края,
// двойной клик по заголовку — развернуть/свернуть.
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;

  switch (msg) {
    case WM_NCHITTEST: {
      POINT pt;
      pt.x = (LONG)(short)LOWORD(lParam);
      pt.y = (LONG)(short)HIWORD(lParam);
      // Win32 передаёт WM_NCHITTEST в экранных координатах, а ImGui
      // main viewport хранит геометрию в координатах клиентской области.
      ScreenToClient(hwnd, &pt);
      ImVec2 mp((float)pt.x, (float)pt.y);

      // Ресайз должен иметь приоритет над заголовком у верхней кромки.
      // WS_THICKFRAME оставляет системе штатную обработку этих HT-кодов;
      // WM_NCCALCSIZE ниже при этом скрывает стандартную рамку.
      if (!IsZoomed(hwnd)) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        const int B = 6;
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        bool l = pt.x < B;
        bool r = pt.x >= w - B;
        bool t = pt.y < B;
        bool b = pt.y >= h - B;
        if (t && l) return HTTOPLEFT;
        if (t && r) return HTTOPRIGHT;
        if (b && l) return HTBOTTOMLEFT;
        if (b && r) return HTBOTTOMRIGHT;
        if (l) return HTLEFT;
        if (r) return HTRIGHT;
        if (t) return HTTOP;
        if (b) return HTBOTTOM;
      }
      // Кнопки заголовка — обычная клиентская область.
      for (const auto& r : g_titlebarButtons)
        if (r.Contains(mp)) return HTCLIENT;
      // Остальной заголовок — перетаскивание окна.
      if (g_titlebarRect.Contains(mp)) return HTCAPTION;
      return HTCLIENT;
    }
    case WM_NCLBUTTONDBLCLK: {
      POINT pt;
      pt.x = (LONG)(short)LOWORD(lParam);
      pt.y = (LONG)(short)HIWORD(lParam);
      ScreenToClient(hwnd, &pt);
      if (g_titlebarRect.Contains(ImVec2((float)pt.x, (float)pt.y)))
        ToggleMaximize(hwnd);
      return 0;
    }
    case WM_NCCALCSIZE:
      // Клиентская область растягивается на всё окно: системная рамка скрыта.
      return 0;
    case WM_GETMINMAXINFO: {
      auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
      mmi->ptMinTrackSize.x = 900;
      mmi->ptMinTrackSize.y = 600;
      HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi;
      mi.cbSize = sizeof(mi);
      if (GetMonitorInfoW(mon, &mi)) {
        mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
        mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
        mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
        mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
      }
      return 0;
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
  wc.style = CS_CLASSDC;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = L"MindWorkspace";
  RegisterClassExW(&wc);

  const int W = 1280, H = 800;
  const int X = (GetSystemMetrics(SM_CXSCREEN) - W) / 2;
  const int Y = (GetSystemMetrics(SM_CYSCREEN) - H) / 2;
  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Mind \x2014 workspace",
                              WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
                                  WS_VISIBLE | WS_CLIPCHILDREN,
                              X, Y, W, H, nullptr, nullptr, wc.hInstance, nullptr);
  if (!hwnd) return 1;
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
  static const ImWchar glyphRanges[] = {
      0x0020, 0x00FF, // ASCII + Latin supplement
      0x0400, 0x04FF, // Cyrillic
      0x2000, 0x206F, // typographic punctuation (en/em dash)
      0x25A1, 0x25A1, // outline square used by the title-bar controls
      0};
  app.uiFont = io.Fonts->AddFontFromFileTTF(
      "C:\\Windows\\Fonts\\segoeui.ttf", 14.0f, nullptr, glyphRanges);
  if (!app.uiFont) app.uiFont = io.Fonts->AddFontDefault();
  app.monoFont = io.Fonts->AddFontFromFileTTF(
      "C:\\Windows\\Fonts\\consola.ttf", 15.0f, nullptr, glyphRanges);
  if (!app.monoFont) app.monoFont = app.uiFont;

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
