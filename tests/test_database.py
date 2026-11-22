import pytest

from database import Database, InsufficientFunds


@pytest.mark.asyncio
async def test_starting_balance_transfer_bonus_and_atomic_game_settlement():
    db = Database(":memory:", starting_balance_cents=100_000, bonus_cooldown_seconds=86_400)
    await db.initialize()
    try:
        assert await db.ensure_user(1, "alice", "Alice") == 100_000
        receipt = await db.transfer(1, "alice", "Alice", 2, "bob", "Bob", 10_000)
        assert receipt.sender_balance_cents == 90_000
        assert receipt.recipient_balance_cents == 110_000

        claim = await db.claim_bonus(2, "bob", "Bob", 20_000, now=1_000)
        assert claim.claimed
        assert claim.balance_cents == 130_000
        blocked = await db.claim_bonus(2, "bob", "Bob", 20_000, now=1_001)
        assert not blocked.claimed
        assert blocked.remaining_seconds == 86_399

        new_balance = await db.settle_game(1, 5_000, 10_000)
        assert new_balance == 95_000
        with pytest.raises(InsufficientFunds) as error:
            await db.settle_game(1, 100_000, 0)
        assert error.value.balance_cents == 95_000
        assert await db.get_balance(1) == 95_000
    finally:
        await db.close()
