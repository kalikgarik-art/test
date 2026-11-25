cmake_minimum_required(VERSION 3.21)
project(MindWorkspace VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# ---------------------------------------------------------------------------
# Dear ImGui скачивается автоматически (FetchContent) — копировать вручную
# ничего не нужно. Требуется интернет при первой настройке CMake.
# Альтернатива через git (если нужен git вместо zip):
#   FetchContent_Declare(imgui
#     GIT_REPOSITORY https://github.com/ocornut/imgui.git
#     GIT_TAG v1.91.9b)
# ---------------------------------------------------------------------------
FetchContent_Declare(
  imgui
  URL https://codeload.github.com/ocornut/imgui/zip/refs/tags/v1.91.9b
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${imgui_SOURCE_DIR}/misc/cpp
)
target_compile_definitions(imgui PRIVATE
  NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00
)
# Системные библиотеки DirectX/Win32 — без них линковка падает.
target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler dwmapi imm32 user32 gdi32)

add_executable(MindWorkspace WIN32 src/main.cpp)
target_include_directories(MindWorkspace PRIVATE src)
target_compile_definitions(MindWorkspace PRIVATE
  NOMINMAX WIN32_LEAN_AND_MEAN IMGUI_DEFINE_MATH_OPERATORS UNICODE _UNICODE
  WINVER=0x0A00 _WIN32_WINNT=0x0A00
)
target_link_libraries(MindWorkspace PRIVATE imgui)

if(MSVC)
  # /utf-8 — корректная работа с кириллицей в исходниках.
  target_compile_options(MindWorkspace PRIVATE /W3 /EHsc /MP /utf-8)
endif()

if(MINGW)
  target_compile_options(MindWorkspace PRIVATE -Wall -Wextra)
  # -municode выбирает wWinMain как точку входа для MinGW; остальные флаги
  # уменьшают зависимость от внешних libgcc/libstdc++ DLL.
  target_link_options(MindWorkspace PRIVATE -municode -static-libgcc -static-libstdc++)
endif()

# Готовый .exe всегда в build/bin (независимо от конфигурации).
set_target_properties(MindWorkspace PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)
