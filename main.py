"""Entrypoint for the aiogram 3 Telegram bot."""

from __future__ import annotations

import asyncio
import logging

from aiogram import Bot, Dispatcher

from config import load_settings
from database import Database
from handlers import create_router


async def main() -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s | %(levelname)s | %(name)s | %(message)s",
    )
    settings = load_settings()
    if settings.owner_id is None:
        logging.warning("OWNER_ID не задан: админ-команды .выдать и .снять отключены.")

    database = Database(
        settings.database_path,
        starting_balance_cents=settings.starting_balance_cents,
        bonus_cooldown_seconds=settings.bonus_cooldown_seconds,
    )
    await database.initialize()

    bot = Bot(token=settings.bot_token)
    dispatcher = Dispatcher()
    dispatcher.include_router(create_router(database, settings.owner_id))

    try:
        # resolve_used_update_types() includes both message and business_message,
        # so private Telegram Business chats are polled along with regular bot chats.
        await dispatcher.start_polling(
            bot,
            allowed_updates=dispatcher.resolve_used_update_types(),
        )
    finally:
        await database.close()
        await bot.session.close()


if __name__ == "__main__":
    asyncio.run(main())
