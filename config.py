"""Application settings loaded from environment variables (optionally from .env)."""

from __future__ import annotations

import os
from dataclasses import dataclass

from dotenv import load_dotenv

load_dotenv()


@dataclass(frozen=True, slots=True)
class Settings:
    bot_token: str
    owner_id: int | None
    database_path: str
    starting_balance_cents: int = 100_000  # 1,000 virtual dollars
    bonus_cooldown_seconds: int = 24 * 60 * 60


def load_settings() -> Settings:
    """Read and validate settings. OWNER_ID may be omitted to disable admin commands."""
    bot_token = os.getenv("BOT_TOKEN", "").strip()
    if not bot_token:
        raise RuntimeError("BOT_TOKEN не задан. Добавьте токен бота в файл .env.")

    raw_owner_id = os.getenv("OWNER_ID", "").strip()
    owner_id: int | None = None
    if raw_owner_id:
        try:
            owner_id = int(raw_owner_id)
        except ValueError as exc:
            raise RuntimeError("OWNER_ID должен быть числовым Telegram ID.") from exc
        if owner_id <= 0:
            raise RuntimeError("OWNER_ID должен быть положительным числом.")

    database_path = os.getenv("DATABASE_PATH", "data/bot.sqlite3").strip()
    if not database_path:
        database_path = "data/bot.sqlite3"

    return Settings(
        bot_token=bot_token,
        owner_id=owner_id,
        database_path=database_path,
    )
