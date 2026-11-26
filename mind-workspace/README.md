# Mind — workspace

Современный десктопный редактор скриптов / менеджер рабочего пространства в стиле
тёмных IDE: **Win32 API + DirectX 11 + Dear ImGui**, один исполняемый файл.

Приложение **полностью автономно и безопасно**: оно работает только с локальными
файлами в папке `./workspace` и собственным интерфейсом. Никаких функций работы
с памятью других процессов, инъекций DLL или системных хуков в коде нет.

## Возможности

* **Безрамочное окно** с кастомным заголовком: «светофор» macOS слева,
  название проекта *Mind — workspace*, индикатор *Lua 5.4* и *runtime v3.2.1*,
  кнопка сохранения, стандартные кнопки свернуть/развернуть/закрыть справа.
  Перетаскивание за заголовок, ресайз за края, двойной клик — максимизация.
* **Вкладки** файлов (`esp.lua`, `aimassist.lua`, `ui_theme.lua`), кнопка `+`,
  индикатор несохранённых изменений.
* **Боковая панель**: поиск с живой фильтрацией, навигация
  (Script Hub / Editor / Console / Settings), список файлов `workspace`,
  секция `POPULAR` со счётчиками (Universal Hub, Infinite Yield, Owl Hub,
  Speed RunX) — клик загружает шаблон в редактор; нижний блок статуса.
* **Редактор кода**: нумерация строк, подсветка синтаксиса Lua
  (ключевые слова, строки, комментарии, числа), выделение мышью,
  Ctrl+C/X/V (буфер обмена Windows), Undo/Redo, Ctrl+S — сохранение.
* **Консоль** с цветными префиксами `[MIND]` / `[NET]` / `[SYS]` / `[ERR]`,
  автопрокруткой и разделителем, перетаскиваемым мышью.
* **Action Bar**: статус `idle / ready / Attached`, кнопки
  `Clear` (консоль или редактор), `Attach`, `Execute`.
* **Файловая система** (`<filesystem>`, C++17): при запуске создаётся
  `./workspace` рядом с `.exe` плюс тестовые `.lua` файлы; список файлов
  загружается в боковую панель; сохранение пишет лог
  `[SYS] saved <filename> -> ./workspace`.

## Структура проекта

```
mind-workspace/
├── CMakeLists.txt
├── src/
│   └── main.cpp            # всё приложение: Win32 + DX11 + UI + редактор
└── external/
    └── imgui/              # Dear ImGui v1.90.9 (уже в комплекте)
        ├── imgui.cpp / imgui.h / imgui_draw.cpp / imgui_tables.cpp ...
        └── backends/
            ├── imgui_impl_win32.cpp / .h
            └── imgui_impl_dx11.cpp / .h
```

Файлы ImGui уже лежат в `external/imgui/`. Если хотите обновить их — скачайте
https://github.com/ocornut/imgui и положите в `external/imgui/` файлы
`imgui.cpp, imgui.h, imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp,
imgui_demo.cpp, imgui_internal.h, imconfig.h, imstb_*.h` и из `backends/`
только `imgui_impl_win32.*` и `imgui_impl_dx11.*`.

## Сборка

### Вариант A. Visual Studio 2022 + CMake (рекомендуется)

1. Установите VS 2022 с рабочей нагрузкой **«Разработка классических приложений на C++»**
   (включая CMake и Windows 10/11 SDK).
2. `File → Open → Folder…` и выберите папку `mind-workspace`.
3. VS сам распознает `CMakeLists.txt`. В панели инструментов CMake выберите
   конфигурацию **x64-Debug** или **x64-Release** и цель `mind_workspace`.
4. `Ctrl+F5` (запуск без отладчика). Готовый `mind_workspace.exe` появится в
   `out/build/x64-Debug/` (или `x64-Release`). Рядом с ним создастся папка
   `workspace/`.

Либо из «Developer Command Prompt for VS 2022»:

```bat
cd mind-workspace
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\Release\mind_workspace.exe
```

### Вариант B. Visual Studio 2022 вручную (без CMake)

1. `Create a new project → Desktop Application (C++)`, пустой проект Win32.
2. Добавьте в проект: `src/main.cpp` и файлы ImGui из `external/imgui/`
   (`imgui.cpp, imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp, imgui_demo.cpp,
   backends/imgui_impl_win32.cpp, backends/imgui_impl_dx11.cpp`).
3. `C/C++ → General → Additional Include Directories`: путь к `external/imgui`
   и `external/imgui/backends`.
4. `C/C++ → Language → C++ Language Standard`: **ISO C++17 (/std:c++17)**.
5. `C/C++ → Command Line → Additional Options`: добавьте **/utf-8**
   (в исходниках есть UTF-8 строки).
6. `Linker → Input → Additional Dependencies`: `d3d11.lib; dxgi.lib; dwmapi.lib;
   imm32.lib; gdi32.lib; user32.lib`.
7. Система: **Windows (/SUBSYSTEM:WINDOWS)** (у проекта Desktop Application уже так).
8. `Ctrl+F5`.

### Вариант C. MinGW-w64 (одна команда)

```sh
cd mind-workspace
g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DNOMINMAX \
    src/main.cpp \
    external/imgui/imgui.cpp external/imgui/imgui_draw.cpp \
    external/imgui/imgui_tables.cpp external/imgui/imgui_widgets.cpp \
    external/imgui/imgui_demo.cpp \
    external/imgui/backends/imgui_impl_win32.cpp \
    external/imgui/backends/imgui_impl_dx11.cpp \
    -Iexternal/imgui -Iexternal/imgui/backends \
    -ld3d11 -ldxgi -ldwmapi -limm32 -lgdi32 -luser32 -lole32 -lshell32 \
    -mwindows -static -o MindWorkspace.exe
```

> Для очень старого GCC (< 9) добавьте `-lstdc++fs` (поддержка `<filesystem>`).

### Вариант D. CMake + Ninja/MinGW

```sh
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Управление

| Действие | Как |
|---|---|
| Сохранить файл | `Ctrl+S` или кнопка-диск в заголовке |
| Отменить / вернуть | `Ctrl+Z` / `Ctrl+Y` |
| Копировать / вырезать / вставить | `Ctrl+C` / `Ctrl+X` / `Ctrl+V` |
| Выделить всё | `Ctrl+A` |
| Перетащить окно | мышью за заголовок |
| Свернуть / развернуть / закрыть | кнопки справа в заголовке или «светофор» слева |
| Загрузить шаблон | клик по пункту в `POPULAR` |
| Открыть файл workspace | клик по файлу в секции `WORKSPACE` |
| Высота консоли | перетащите разделитель над консолью |

## Замечания

* Все шаблоны в `POPULAR` — безобидные демо-скрипты на Lua (печать в консоль),
  название и счётчики повторяют референс-скриншот интерфейса.
* Иконки и скругления рисуются непосредственно через ImDrawList — сторонние
  шрифты-иконки не нужны. UI-шрифт — Segoe UI, моноширинный — Consolas
  (стандартные шрифты Windows; при их отсутствии используется встроенный шрифт).
* Файл `imgui.ini` не создаётся: состояние окна между запусками не сохраняется.
