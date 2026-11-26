cmake_minimum_required(VERSION 3.20)
project(mind_workspace LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)

add_executable(mind_workspace WIN32
    src/main.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(mind_workspace PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_compile_definitions(mind_workspace PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    # /utf-8 обязателен: в исходниках есть UTF-8 строки («Mind — workspace»)
    target_compile_options(mind_workspace PRIVATE /utf-8 /W3)
else()
    target_compile_options(mind_workspace PRIVATE -Wall -Wno-unused-parameter)
endif()

target_link_libraries(mind_workspace PRIVATE
    d3d11
    dxgi
    dwmapi
    imm32
    gdi32
    user32
    shell32
    ole32
)
