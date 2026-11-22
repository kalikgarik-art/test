from unittest.mock import AsyncMock

import pytest
from aiogram import Bot
from aiogram.types import Message

from database import Database
from handlers import _process_message


@pytest.mark.asyncio
async def test_business_reply_uses_connection_id_and_creates_profile():
    bot = Bot(token="123456:TEST")
    bot.send_message = AsyncMock()
    message = Message.model_validate(
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
