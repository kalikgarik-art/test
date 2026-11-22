"""Async SQLite storage for profiles, virtual balances, transfers and bonuses."""

from __future__ import annotations

import asyncio
import time
from dataclasses import dataclass
from pathlib import Path

import aiosqlite

# Keep values safely within SQLite's signed 64-bit INTEGER range.
MAX_BALANCE_CENTS = 9_000_000_000_000_000_000


class InsufficientFunds(Exception):
    """Raised when an operation would make a user's balance negative."""

    def __init__(self, balance_cents: int) -> None:
        super().__init__("Insufficient funds")
        self.balance_cents = balance_cents


class BalanceLimitExceeded(Exception):
    """Raised when an operation would exceed the supported wallet limit."""


@dataclass(frozen=True, slots=True)
class BonusClaim:
    claimed: bool
    balance_cents: int
    remaining_seconds: int = 0


@dataclass(frozen=True, slots=True)
class TransferReceipt:
    sender_balance_cents: int
    recipient_balance_cents: int


class Database:
    """A single async SQLite connection guarded by a lock for safe transactions."""

    def __init__(
        self,
        path: str,
        *,
        starting_balance_cents: int = 100_000,
        bonus_cooldown_seconds: int = 24 * 60 * 60,
    ) -> None:
        self.path = path
        self.starting_balance_cents = starting_balance_cents
        self.bonus_cooldown_seconds = bonus_cooldown_seconds
        self._connection: aiosqlite.Connection | None = None
        self._lock = asyncio.Lock()

    async def initialize(self) -> None:
        async with self._lock:
            if self._connection is not None:
                return

            if self.path != ":memory:":
                Path(self.path).expanduser().parent.mkdir(parents=True, exist_ok=True)

            connection = await aiosqlite.connect(self.path)
            connection.row_factory = aiosqlite.Row
            await connection.execute("PRAGMA foreign_keys = ON")
            await connection.execute("PRAGMA journal_mode = WAL")
            await connection.execute("PRAGMA busy_timeout = 5000")
            await connection.execute(
                """
                CREATE TABLE IF NOT EXISTS users (
                    user_id INTEGER PRIMARY KEY,
                    username TEXT,
                    first_name TEXT NOT NULL DEFAULT '',
                    balance_cents INTEGER NOT NULL CHECK (balance_cents >= 0),
                    bonus_claimed_at INTEGER,
                    created_at INTEGER NOT NULL,
                    updated_at INTEGER NOT NULL
                )
                """
            )
            await connection.commit()
            self._connection = connection

    def _conn(self) -> aiosqlite.Connection:
        if self._connection is None:
            raise RuntimeError("Database.initialize() must be called before use.")
        return self._connection

    async def _ensure_user_unlocked(
        self,
        user_id: int,
        username: str | None,
        first_name: str | None,
    ) -> None:
        now = int(time.time())
        await self._conn().execute(
            """
            INSERT INTO users (
                user_id, username, first_name, balance_cents,
                bonus_claimed_at, created_at, updated_at
            ) VALUES (?, ?, ?, ?, NULL, ?, ?)
            ON CONFLICT(user_id) DO UPDATE SET
                username = excluded.username,
                first_name = excluded.first_name,
                updated_at = excluded.updated_at
            """,
            (
                user_id,
                username,
                first_name or "",
                self.starting_balance_cents,
                now,
                now,
            ),
        )

    async def ensure_user(
        self,
        user_id: int,
        username: str | None,
        first_name: str | None,
    ) -> int:
        """Create a profile with the starting balance, or refresh its profile fields."""
        async with self._lock:
            await self._ensure_user_unlocked(user_id, username, first_name)
            cursor = await self._conn().execute(
                "SELECT balance_cents FROM users WHERE user_id = ?", (user_id,)
            )
            row = await cursor.fetchone()
            await self._conn().commit()
            assert row is not None
            return int(row["balance_cents"])

    async def get_balance(self, user_id: int) -> int:
        async with self._lock:
            cursor = await self._conn().execute(
                "SELECT balance_cents FROM users WHERE user_id = ?", (user_id,)
            )
            row = await cursor.fetchone()
            if row is None:
                raise RuntimeError(f"User {user_id} has not been initialized.")
            return int(row["balance_cents"])

    async def settle_game(
        self,
        user_id: int,
        stake_cents: int,
        payout_cents: int,
    ) -> int:
        """Atomically debit a stake and credit the gross payout; return new balance."""
        if stake_cents <= 0 or payout_cents < 0:
            raise ValueError("Stake must be positive and payout cannot be negative.")

        async with self._lock:
            connection = self._conn()
            await connection.execute("BEGIN IMMEDIATE")
            try:
                cursor = await connection.execute(
                    "SELECT balance_cents FROM users WHERE user_id = ?", (user_id,)
                )
                row = await cursor.fetchone()
                if row is None:
                    raise RuntimeError(f"User {user_id} has not been initialized.")
                balance = int(row["balance_cents"])
                if balance < stake_cents:
                    raise InsufficientFunds(balance)

                new_balance = balance - stake_cents + payout_cents
                if new_balance > MAX_BALANCE_CENTS:
                    raise BalanceLimitExceeded

                await connection.execute(
                    "UPDATE users SET balance_cents = ?, updated_at = ? WHERE user_id = ?",
                    (new_balance, int(time.time()), user_id),
                )
                await connection.commit()
                return new_balance
            except BaseException:
                await connection.rollback()
                raise

    async def claim_bonus(
        self,
        user_id: int,
        username: str | None,
        first_name: str | None,
        amount_cents: int,
        now: int | None = None,
    ) -> BonusClaim:
        """Claim the daily bonus once per cooldown, atomically."""
        now = int(time.time()) if now is None else now
        if amount_cents <= 0:
            raise ValueError("Bonus must be positive.")

        async with self._lock:
            connection = self._conn()
            await connection.execute("BEGIN IMMEDIATE")
            try:
                await self._ensure_user_unlocked(user_id, username, first_name)
                cursor = await connection.execute(
                    "SELECT balance_cents, bonus_claimed_at FROM users WHERE user_id = ?",
                    (user_id,),
                )
                row = await cursor.fetchone()
                assert row is not None
                balance = int(row["balance_cents"])
                last_claim = row["bonus_claimed_at"]

                if last_claim is not None:
                    available_at = int(last_claim) + self.bonus_cooldown_seconds
                    if now < available_at:
                        await connection.commit()
                        return BonusClaim(
                            claimed=False,
                            balance_cents=balance,
                            remaining_seconds=available_at - now,
                        )

                new_balance = balance + amount_cents
                if new_balance > MAX_BALANCE_CENTS:
                    raise BalanceLimitExceeded

                await connection.execute(
                    """
                    UPDATE users
                    SET balance_cents = ?, bonus_claimed_at = ?, updated_at = ?
                    WHERE user_id = ?
                    """,
                    (new_balance, now, now, user_id),
                )
                await connection.commit()
                return BonusClaim(claimed=True, balance_cents=new_balance)
            except BaseException:
                await connection.rollback()
                raise

    async def transfer(
        self,
        sender_id: int,
        sender_username: str | None,
        sender_first_name: str | None,
        recipient_id: int,
        recipient_username: str | None,
        recipient_first_name: str | None,
        amount_cents: int,
    ) -> TransferReceipt:
        """Transfer virtual currency atomically between two users."""
        if amount_cents <= 0:
            raise ValueError("Transfer amount must be positive.")
        if sender_id == recipient_id:
            raise ValueError("Cannot transfer to the same user.")

        async with self._lock:
            connection = self._conn()
            await connection.execute("BEGIN IMMEDIATE")
            try:
                await self._ensure_user_unlocked(
                    sender_id, sender_username, sender_first_name
                )
                await self._ensure_user_unlocked(
                    recipient_id, recipient_username, recipient_first_name
                )
                cursor = await connection.execute(
                    "SELECT balance_cents FROM users WHERE user_id = ?", (sender_id,)
                )
                sender_row = await cursor.fetchone()
                cursor = await connection.execute(
                    "SELECT balance_cents FROM users WHERE user_id = ?", (recipient_id,)
                )
                recipient_row = await cursor.fetchone()
                assert sender_row is not None and recipient_row is not None

                sender_balance = int(sender_row["balance_cents"])
                recipient_balance = int(recipient_row["balance_cents"])
                if sender_balance < amount_cents:
                    raise InsufficientFunds(sender_balance)
                new_recipient_balance = recipient_balance + amount_cents
                if new_recipient_balance > MAX_BALANCE_CENTS:
                    raise BalanceLimitExceeded

                now = int(time.time())
                await connection.execute(
                    "UPDATE users SET balance_cents = ?, updated_at = ? WHERE user_id = ?",
                    (sender_balance - amount_cents, now, sender_id),
                )
                await connection.execute(
                    "UPDATE users SET balance_cents = ?, updated_at = ? WHERE user_id = ?",
                    (new_recipient_balance, now, recipient_id),
                )
                await connection.commit()
                return TransferReceipt(
                    sender_balance_cents=sender_balance - amount_cents,
                    recipient_balance_cents=new_recipient_balance,
                )
            except BaseException:
                await connection.rollback()
                raise

    async def adjust_balance(
        self,
        user_id: int,
        username: str | None,
        first_name: str | None,
        delta_cents: int,
    ) -> int:
        """Apply an admin adjustment. Negative adjustments cannot overdraw a wallet."""
        if delta_cents == 0:
            raise ValueError("Balance adjustment must be non-zero.")

        async with self._lock:
            connection = self._conn()
            await connection.execute("BEGIN IMMEDIATE")
            try:
                await self._ensure_user_unlocked(user_id, username, first_name)
                cursor = await connection.execute(
                    "SELECT balance_cents FROM users WHERE user_id = ?", (user_id,)
                )
                row = await cursor.fetchone()
                assert row is not None
                balance = int(row["balance_cents"])
                new_balance = balance + delta_cents
                if new_balance < 0:
                    raise InsufficientFunds(balance)
                if new_balance > MAX_BALANCE_CENTS:
                    raise BalanceLimitExceeded

                await connection.execute(
                    "UPDATE users SET balance_cents = ?, updated_at = ? WHERE user_id = ?",
                    (new_balance, int(time.time()), user_id),
                )
                await connection.commit()
                return new_balance
            except BaseException:
                await connection.rollback()
                raise

    async def close(self) -> None:
        async with self._lock:
            if self._connection is not None:
                await self._connection.close()
                self._connection = None
