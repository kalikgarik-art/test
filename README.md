# Telegram-конвертер аудио и видео

Асинхронный бот на aiogram 3.x, преобразующий аудио в Telegram voice (Ogg/Opus), а видео — в квадратное video note. Входящие и выходные файлы сохраняются во временной папке для каждого запроса и автоматически удаляются после обработки.

## Требования

- Python 3.10 или новее
- FFmpeg с поддержкой `libx264` и `libopus` (для аудио также нужен `aac`)
- Токен Telegram-бота от [@BotFather](https://t.me/BotFather)

## Установка и запуск — Linux

1. Установите FFmpeg, например на Ubuntu/Debian:
   ```bash
   sudo apt update
   sudo apt install ffmpeg
   ffmpeg -version
   ```
2. Перейдите в папку проекта и создайте виртуальное окружение:
   ```bash
   python3 -m venv .venv
   source .venv/bin/activate
   pip install -r requirements.txt
   ```
3. Создайте в корне проекта файл `.env` (рядом с `bot.py`) и добавьте строку:
   ```env
   BOT_TOKEN=токен_от_BotFather
   ```
   Затем установите зависимости и запустите бота:
   ```bash
   pip install -r requirements.txt
   python bot.py
   ```
   Файл `.env` исключён из Git, поэтому токен не попадёт в коммит.

## Установка и запуск — Windows

1. Установите FFmpeg одним из способов:
   - скачайте сборку с https://www.gyan.dev/ffmpeg/builds/ (например, release essentials), распакуйте и добавьте папку `bin` в системную переменную `PATH`; или
   - выполните в PowerShell `winget install Gyan.FFmpeg.Shared`.
2. Откройте новый PowerShell и проверьте `ffmpeg -version`.
3. В папке проекта создайте виртуальное окружение и установите зависимости:
   ```powershell
   py -m venv .venv
   .\.venv\Scripts\Activate.ps1
   pip install -r requirements.txt
   ```
4. Создайте рядом с `bot.py` файл `.env` (не `.env.txt`) со строкой:
   ```env
   BOT_TOKEN=токен_от_BotFather
   ```
   Установите зависимости и запустите бота:
   ```powershell
   pip install -r requirements.txt
   python bot.py
   ```

Если FFmpeg не добавлен в `PATH`, можно указать полный путь к исполняемому файлу через `FFMPEG_PATH` (например, `$env:FFMPEG_PATH='C:\ffmpeg\bin\ffmpeg.exe'` в PowerShell).

## Использование

Отправьте боту аудио или аудиофайл `.mp3`, `.wav`, `.m4a`, `.ogg`, чтобы получить голосовое сообщение. Для видеокружка отправьте видео или видеофайл `.mp4`, `.mov`, `.avi`. Видео обрезается по центру до квадрата 480×480 и первых 59 секунд. Максимальный размер скачиваемого файла — 20 МБ.
