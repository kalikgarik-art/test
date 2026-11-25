# Mind Workspace

Десктопный редактор скриптов и менеджер рабочего пространства (IDE/Workspace UI)
на **C++17 + Win32 API + DirectX 11 + Dear ImGui**. Полностью автономное
приложение: только локальные файлы и интерфейс — без работы с памятью других
процессов, DLL-инъекций и системных хуков.

## Что внутри

- **Borderless-окно** с кастомным заголовком: перетаскивание мышью, ресайз за края,
  двойной клик по заголовку — развернуть/свернуть. macOS-кружки слева,
  стандартные иконки справа, название «Mind — workspace», бейдж «Lua 5.4»,
  кнопка сохранения в заголовке.
- **Тёмная тема**: фон `#0D0E14`, панели `#13151F`, акценты `#635BFF` / `#8B5CF6`,
  скругления 8.0 (окна/карточки) и 6.0 (кнопки).
- **Вкладки файлов** (`esp.lua`, `aimassist.lua`, `ui_theme.lua`, …), кнопка `+`,
  индикатор активной вкладки, `*` — несохранённые изменения.
- **Сайдбар**: живой поиск `Search scripts`, меню `Script Hub / Editor / Console /
  Settings`, секция `POPULAR` (Universal Hub, Infinite Yield, Owl Hub, Speed Run X
  со счётчиками — клик загружает код в редактор), список файлов из `workspace`,
  статус `Script Hub: synced & ready`.
- **Редактор кода**: нумерация строк, моноширинный шрифт (Consolas),
  базовая подсветка синтаксиса Lua (ключевые слова, строки, комментарии,
  числа, встроенные функции).
- **Консоль** с цветными префиксами `[MIND] / [NET] / [SYS] / [ERR]`,
  автопрокруткой и перетаскиваемым разделителем.
- **Футер**: статус `idle` / `Attached`, кнопки `Clear`, `Attach`, `Execute`.
  `Attach` только имитирует подключение; `Execute` выводит статус предпросмотра.
- **Файловая система**: при запуске создаётся `./workspace` рядом с `.exe`
  (при необходимости — с тестовыми `.lua` файлами). Сохранение — `Ctrl+S`
  или кнопка `Save` в заголовке, в консоли появляется
  `[SYS] saved <filename> -> ./workspace`.

> **Safe mode:** `Lua 5.4` — индикатор профиля, встроенного интерпретатора нет.
> Скрипты не исполняются, а `Execute` только пишет статус в консоль. Приложение
> не обращается к другим процессам, не внедряет DLL и не ставит системные хуки.

## Структура проекта

```
CMakeLists.txt        — сборка (ImGui скачивается автоматически через FetchContent)
src/
  main.cpp            — Win32/DirectX11 каркас, цикл сообщений, точка входа
  app.h               — состояние приложения (App)
  ui.h                — весь интерфейс: заголовок, вкладки, сайдбар, футер, настройки
  theme.h             — палитра и стиль ImGui
  editor.h            — редактор кода с подсветкой Lua
  console.h           — консоль вывода
  workspace.h         — файловая система ./workspace, вкладки, шаблоны POPULAR
workspace/            — примеры .lua файлов (в рантайме создаётся рядом с .exe)
```

> **Куда класть файлы ImGui?** Обычно никуда — копировать вручную не нужно.
> `CMakeLists.txt` скачивает Dear ImGui (v1.91.9b) автоматически при первой
> настройке через `FetchContent`. Нужен интернет один раз. Если скачиваете
> исходники вручную, распакуйте их в `third_party/imgui` и замените строки
> `FetchContent_Declare(...)` / `FetchContent_MakeAvailable(imgui)` на:
> `set(imgui_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")`.
> Корневой каталог ImGui не предоставляет `add_subdirectory`-совместимый
> CMake-проект, поэтому ниже CMake напрямую собирает нужные `.cpp` файлы.

## Требования

- Windows 10/11 (x64)
- **Вариант A:** Visual Studio 2022 с компонентом «Разработка классических
  приложений на C++» (MSVC v143, Windows SDK, CMake)
- **Вариант B:** MinGW-w64 (GCC 10+, `mingw32-make`) и CMake 3.21+
- Интернет при первой настройке (для загрузки Dear ImGui)

## Сборка в Visual Studio 2022 / MSVC

1. Установите VS 2022 с workload «Desktop development with C++» и Windows SDK.
2. Откройте **Developer PowerShell for VS 2022** в корне проекта.
3. Настройте x64-проект (при первой настройке CMake загрузит Dear ImGui):

   ```powershell
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   ```

4. Соберите Release:

   ```powershell
   cmake --build build --config Release --target MindWorkspace
   ```

5. Готовый файл: `build\bin\MindWorkspace.exe`. Его можно запускать из
   PowerShell (`.\build\bin\MindWorkspace.exe`) или открыть solution
   `build\MindWorkspace.sln` в Visual Studio и собрать конфигурацию
   **Release | x64**. При запуске рядом с `.exe` появится `workspace`.

## Сборка через MinGW (командная строка)

```bat
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw --config Release --target MindWorkspace
build-mingw\bin\MindWorkspace.exe
```

> Если генератор `MinGW Makefiles` не находится, убедитесь, что `mingw32-make`
> (или `make`) есть в PATH. Для MSYS2: `pacman -S mingw-w64-x86_64-gcc
> mingw-w64-x86_64-make mingw-w64-x86_64-cmake`.

## Управление

| Действие | Как |
|---|---|
| Сохранить файл | `Ctrl+S` или кнопка `Save` в заголовке |
| Новая вкладка | `Ctrl+N` или кнопка `+` |
| Закрыть вкладку | `Ctrl+W` или крестик на вкладке |
| Перетаскивание окна | за заголовок (не за кнопки) |
| Развернуть/свернуть | двойной клик по заголовку или кнопки окна |
| Ресайз окна | за края окна (рамка 6 px) |
| Высота консоли | перетаскивание разделителя |
| Открыть шаблон | клик по элементу в секции `POPULAR` |
| Открыть файл | клик по имени в секции `WORKSPACE` |

## Troubleshooting

- **«CMake не найден»** — установите CMake и добавьте его в PATH, либо
  используйте встроенный CMake из Visual Studio.
- **Ошибки линковки Win32/DirectX** — убедитесь, что проект собирается под
  Windows (x64) и что сборка идёт через `CMakeLists.txt`: он линкует
  `d3d11`, `dxgi`, `d3dcompiler`, `dwmapi`, `imm32`, `user32` и `gdi32`.
  При ручном `.vcxproj` укажите эти библиотеки в
  *Linker → Input → Additional Dependencies*.
- **Первая настройка падает без интернета** — `FetchContent` не может скачать
  Dear ImGui. Подключитесь к интернету один раз или положите ImGui вручную
  в `third_party/imgui` (см. примечание выше).
- **Кракозябры вместо кириллицы в исходниках** — в `CMakeLists.txt` уже
  включён флаг `/utf-8` для MSVC; исходники сохранены в UTF-8.
- **Окно не перетаскивается** — перетаскивание работает только за заголовок;
  кнопки окна (`Save`, кружки, иконки справа) намеренно исключены из
  перетаскивания через `WM_NCHITTEST`.
