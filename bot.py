"""Telegram audio/video converter bot (aiogram 3.x + FFmpeg).

Set BOT_TOKEN in the environment, install FFmpeg, then run: python bot.py
"""
import asyncio
import logging
import os
import shutil
import tempfile
from pathlib import Path

from dotenv import load_dotenv
from aiogram import Bot, Dispatcher, F
from aiogram.enums import ChatAction
from aiogram.filters import CommandStart
from aiogram.types import FSInputFile, Message

# Load BOT_TOKEN (and optional FFMPEG_PATH) from a .env file in the project directory.
load_dotenv()

# Telegram's standard hosted Bot API download limit.
MAX_DOWNLOAD_BYTES = 20 * 1024 * 1024
MAX_VIDEO_SECONDS = 59
FFMPEG = os.getenv("FFMPEG_PATH", "ffmpeg")

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger(__name__)
dp = Dispatcher()


def file_details(message: Message):
    """Return (Telegram file, filename) for supported incoming media."""
    if message.audio:
        item = message.audio
        return item, item.file_name or "audio.mp3"
    if message.voice:
        return message.voice, "voice.ogg"
    if message.video:
        item = message.video
        return item, item.file_name or "video.mp4"
    if message.document:
        item = message.document
        name = item.file_name or ""
        ext = Path(name).suffix.lower()
        mime = (item.mime_type or "").lower()
        if ext in {".mp3", ".wav", ".m4a", ".ogg"} or mime.startswith("audio/"):
            return item, name or "audio"
        if ext in {".mp4", ".mov", ".avi"} or mime.startswith("video/"):
            return item, name or "video.mp4"
    return None, None


async def run_ffmpeg(*args: str) -> None:
    """Run FFmpeg without blocking the event loop; include useful errors in logs."""
    try:
        proc = await asyncio.create_subprocess_exec(
            FFMPEG, "-hide_banner", "-loglevel", "error", "-y", *args,
            stdout=asyncio.subprocess.DEVNULL,
            stderr=asyncio.subprocess.PIPE,
        )
    except FileNotFoundError as exc:
        raise RuntimeError("FFmpeg не найден. Установите его и добавьте в PATH.") from exc
    _, stderr = await proc.communicate()
    if proc.returncode != 0:
        detail = stderr.decode("utf-8", errors="replace").strip()[-1500:]
        log.warning("FFmpeg exited %s: %s", proc.returncode, detail)
        raise RuntimeError("Не удалось обработать файл. Проверьте, что он не повреждён.")


@dp.message(CommandStart())
async def start(message: Message) -> None:
    await message.answer(
        "Привет! Я конвертирую медиа для Telegram.\n\n"
        "• Отправьте аудио или файл .mp3, .wav, .m4a, .ogg — верну голосовое сообщение.\n"
        "• Отправьте видео или файл .mp4, .mov, .avi — верну видеокружок.\n"
        "Максимальный размер исходного файла — 20 МБ. Видео длиннее 59 секунд будет обрезано."
    )


@dp.message(F.audio | F.voice | F.video | F.document)
async def convert_media(message: Message, bot: Bot) -> None:
    media, filename = file_details(message)
    if media is None:
        await message.answer("Этот формат не поддерживается. Пришлите аудио или видео подходящего формата.")
        return
    if media.file_size is not None and media.file_size > MAX_DOWNLOAD_BYTES:
        await message.answer("Файл больше 20 МБ — Bot API не позволяет его скачать. Пришлите файл меньшего размера.")
        return

    suffix = Path(filename).suffix.lower()
    is_video = bool(message.video) or suffix in {".mp4", ".mov", ".avi"} or (
        bool(message.document) and (message.document.mime_type or "").lower().startswith("video/")
    )
    # A voice/audio Telegram object is always audio, regardless of its filename.
    if message.audio or message.voice:
        is_video = False

    status = ChatAction.RECORD_VIDEO_NOTE if is_video else ChatAction.RECORD_VOICE
    try:
        # Per-request directory makes names unique and guarantees cleanup, including failures.
        with tempfile.TemporaryDirectory(prefix=f"tgconv_{message.from_user.id if message.from_user else 'user'}_") as tmp:
            source = Path(tmp) / f"input{suffix or ('.mp4' if is_video else '.audio')}"
            output = Path(tmp) / ("result.mp4" if is_video else "result.ogg")
            await bot.send_chat_action(message.chat.id, status)
            await bot.download(media, destination=source)
            actual_size = source.stat().st_size
            if actual_size > MAX_DOWNLOAD_BYTES:
                await message.answer("Файл больше 20 МБ — обработка отменена. Пришлите файл меньшего размера.")
                return
            if actual_size == 0:
                await message.answer("Файл пустой или не удалось его скачать.")
                return

            if is_video:
                # Scale to fill a centered square, then crop; normalize dimensions for H.264.
                await run_ffmpeg(
                    "-i", str(source), "-t", str(MAX_VIDEO_SECONDS),
                    "-vf", "scale=480:480:force_original_aspect_ratio=increase,crop=480:480,setsar=1",
                    "-c:v", "libx264", "-preset", "veryfast", "-crf", "25",
                    "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "96k",
                    "-movflags", "+faststart", str(output),
                )
                await bot.send_video_note(message.chat.id, video_note=FSInputFile(output))
            else:
                await run_ffmpeg(
                    "-i", str(source), "-vn", "-ac", "1", "-ar", "48000",
                    "-c:a", "libopus", "-b:a", "64k", "-application", "voip", str(output),
                )
                await bot.send_voice(message.chat.id, voice=FSInputFile(output))
    except RuntimeError as exc:
        await message.answer(str(exc))
    except Exception:
        log.exception("Media processing failed")
        await message.answer("Не получилось обработать или отправить файл. Попробуйте другой файл позже.")


async def main() -> None:
    token = os.getenv("BOT_TOKEN")
    if not token:
        raise RuntimeError("Задайте токен бота в переменной окружения BOT_TOKEN.")
    if shutil.which(FFMPEG) is None:
        raise RuntimeError(f"FFmpeg не найден ({FFMPEG}). Установите FFmpeg или задайте FFMPEG_PATH.")
    bot = Bot(token=token)
    try:
        await dp.start_polling(bot)
    finally:
        await bot.session.close()


if __name__ == "__main__":
    asyncio.run(main())
