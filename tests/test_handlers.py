import logging
from unittest.mock import AsyncMock

import pytest
from aiogram import Bot
from aiogram.exceptions import TelegramBadRequest
from aiogram.methods import SendMessage
from aiogram.types import Message

from database import Database
from handlers import _process_message


def make_business_message(bot: Bot) -> Message:
    return Message.model_validate(
        {
            "message_id": 12,
            "date": 1_700_000_000,
            "business_connection_id": "business-connection-1",
            "chat": {"id": 42, "type": "private", "first_name": "Client"},
            "from": {"id": 42, "is_bot": False, "first_name": "Client"},
            "text": ".б",
        },
        context={"bot": bot},
    )


@pytest.mark.asyncio
async def test_business_reply_uses_connection_id_and_creates_profile():
    bot = Bot(token="123456:TEST")
    bot.send_message = AsyncMock()
    message = make_business_message(bot)
    db = Database(":memory:")
    await db.initialize()
    try:
        await _process_message(message, db, owner_id=None, business_update=True)
        sent = bot.send_message.await_args.kwargs
        assert sent["business_connection_id"] == "business-connection-1"
        assert sent["reply_parameters"].message_id == 12
        assert sent["text"] == "💰 Ваш баланс: 1 000$."
        assert await db.get_balance(42) == 100_000
    finally:
        await db.close()
        await bot.session.close()


@pytest.mark.asyncio
async def test_business_peer_invalid_logs_actionable_permission_hint(caplog):
    bot = Bot(token="123456:TEST")
    bot.send_message = AsyncMock(
        side_effect=TelegramBadRequest(
            method=SendMessage(chat_id=42, text="test"),
            message="Bad Request: BUSINESS_PEER_INVALID",
        )
    )
    db = Database(":memory:")
    await db.initialize()
    try:
        with caplog.at_level(logging.ERROR, logger="handlers"):
            await _process_message(
                make_business_message(bot),
                db,
                owner_id=None,
                business_update=True,
            )
        assert "BUSINESS_PEER_INVALID" in caplog.text
        assert "allowed chats" in caplog.text
    finally:
        await db.close()
        await bot.session.close()
