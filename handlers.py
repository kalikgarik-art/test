"""Dot-command handlers for regular bot chats and Telegram Business messages."""

from __future__ import annotations

import re
import secrets
import time
from collections.abc import Callable
from decimal import Decimal, InvalidOperation

from aiogram import Router
from aiogram.types import Message, ReplyParameters, User

from database import (
    MAX_BALANCE_CENTS,
    BalanceLimitExceeded,
    Database,
    InsufficientFunds,
)
from games import (
    GameOutcome,
    play_coin,
    play_crash,
    play_dice_number,
    play_dice_parity,
    play_high_low,
    play_mines,
    play_roulette,
    play_slots,
)

_MONEY_RE = re.compile(r"^\d+(?:[.,]\d{1,2})?$")
_COEFFICIENT_RE = re.compile(r"^\d+(?:[.,]\d{1,2})?$")
_COMMAND_RE = re.compile(r"^\s*\.([^\s]+)(?:\s+(.*))?$", re.DOTALL)


class CommandInputError(ValueError):
    """A user-facing validation error for command arguments."""


def format_money(cents: int) -> str:
    """Format cents using Russian grouping and decimal separators."""
    sign = "-" if cents < 0 else ""
    absolute = abs(cents)
    whole, fractional = divmod(absolute, 100)
    grouped = f"{whole:,}".replace(",", " ")
    if fractional:
        return f"{sign}{grouped},{fractional:02d}$"
    return f"{sign}{grouped}$"


def _parse_amount(raw: str) -> int:
    value = raw.strip()
    if value.startswith("-"):
        raise CommandInputError("❌ Сумма не может быть отрицательной.")
    if not _MONEY_RE.fullmatch(value):
        raise CommandInputError(
            "❌ Введите положительную сумму, например 100 или 12.50."
        )
    try:
        amount = Decimal(value.replace(",", "."))
    except InvalidOperation as exc:
        raise CommandInputError("❌ Не удалось прочитать сумму.") from exc
    if amount <= 0:
        raise CommandInputError("❌ Сумма должна быть больше нуля.")
    if amount * 100 > MAX_BALANCE_CENTS:
        raise CommandInputError("❌ Указанная сумма слишком велика.")
    return int(amount * 100)


def _parse_coefficient(raw: str) -> Decimal:
    value = raw.strip()
    if not _COEFFICIENT_RE.fullmatch(value):
        raise CommandInputError(
            "❌ Коэффициент должен быть числом с точностью до сотых, например 1.50."
        )
    try:
        coefficient = Decimal(value.replace(",", "."))
    except InvalidOperation as exc:
        raise CommandInputError("❌ Не удалось прочитать коэффициент.") from exc
    if not Decimal("1.00") <= coefficient <= Decimal("100.00"):
        raise CommandInputError("❌ Автовывод должен быть от 1.00 до 100.00.")
    return coefficient


def _parse_command(text: str) -> tuple[str, list[str]] | None:
    match = _COMMAND_RE.match(text)
    if match is None:
        return None
    command = match.group(1).casefold()
    arguments = (match.group(2) or "").split()
    return command, arguments


def _usage(command: str, syntax: str) -> str:
    suffix = f" {syntax}" if syntax else ""
    return f"❌ Неверный формат. Использование: .{command}{suffix}"


def _display_duration(seconds: int) -> str:
    seconds = max(0, seconds)
    hours, remainder = divmod(seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    if hours:
        return f"{hours} ч. {minutes} мин."
    if minutes:
        return f"{minutes} мин."
    return f"{seconds} сек."


async def _send(message: Message, text: str) -> None:
    """Reply in the same chat, preserving a Business connection when present."""
    kwargs: dict[str, object] = {
        "chat_id": message.chat.id,
        "text": text,
        "reply_parameters": ReplyParameters(
            message_id=message.message_id,
            allow_sending_without_reply=True,
        ),
    }
    business_connection_id = getattr(message, "business_connection_id", None)
    if business_connection_id:
        kwargs["business_connection_id"] = business_connection_id
    await message.bot.send_message(**kwargs)  # type: ignore[arg-type]


def _help_text() -> str:
    return (
        "🎮 Команды (баланс игровой, виртуальный):\n"
        ".б / .баланс — баланс; .бонус — бонус раз в 24 часа; .ид — ваш Telegram ID\n"
        ".рулетка <ставка> <выбор> или .р — к/красное, ч/чёрное, з/зелёное, "
        "чет, нечет, число 1–36; 0 — зелёное, #0 — точное число 0\n"
        ".кости <ставка> <1–6> — x5; .куб <ставка> <чет/нечет> — x1.9\n"
        ".монетка <ставка> <орел/решка> или <о/р> — x1.95\n"
        ".краш <ставка> <коэффициент 1.00–100.00>\n"
        ".слоты <ставка> или .с <ставка>\n"
        ".бм <ставка> <б/м> — больше/меньше 50, x1.95\n"
        ".мины <ставка> <ячейка 1–9> — безопасная ячейка x1.4\n"
        ".передать <сумма> — ответом на сообщение игрока\n"
        ".выдать <сумма> — владелец, ответом на сообщение; .снять <сумма> — владелец\n"
        "Ставки можно указывать с точностью до цента, например 12.50."
    )


def _reply_target(message: Message) -> User | None:
    if message.reply_to_message is None:
        return None
    target = message.reply_to_message.from_user
    if target is None or target.is_bot:
        return None
    return target


async def _run_game(
    message: Message,
    db: Database,
    player: User,
    stake_cents: int,
    play: Callable[[], GameOutcome],
) -> None:
    # Validate the game choice before reporting a balance error; no funds are touched
    # until settle_game commits the wager and payout together.
    try:
        outcome = play()
    except ValueError as exc:
        await _send(message, f"❌ {exc}")
        return

    balance = await db.get_balance(player.id)
    if stake_cents > balance:
        await _send(
            message,
            f"❌ Недостаточно средств. Ваш баланс: {format_money(balance)}.",
        )
        return

    try:
        new_balance = await db.settle_game(
            player.id,
            stake_cents,
            outcome.payout_cents,
        )
    except InsufficientFunds as exc:
        # Another simultaneous command may have spent the balance after the check above.
        await _send(
            message,
            f"❌ Недостаточно средств. Ваш баланс: {format_money(exc.balance_cents)}.",
        )
        return
    except BalanceLimitExceeded:
        await _send(
            message,
            "❌ Выплата превысила максимально допустимый виртуальный баланс. "
            "Ставка не списана.",
        )
        return

    if outcome.won:
        profit = outcome.payout_cents - stake_cents
        result_text = (
            f"✅ Выигрыш! Выплата: {format_money(outcome.payout_cents)}; "
            f"чистая прибыль: {format_money(profit)}."
        )
    else:
        result_text = f"❌ Проигрыш. Ставка {format_money(stake_cents)} потеряна."

    await _send(
        message,
        f"{outcome.description}\n{result_text}\n"
        f"Баланс: {format_money(new_balance)}.",
    )


async def _handle_bonus(message: Message, db: Database, player: User) -> None:
    bonus_dollars = 100 + secrets.randbelow(401)
    try:
        claim = await db.claim_bonus(
            player.id,
            player.username,
            player.first_name,
            bonus_dollars * 100,
            now=int(time.time()),
        )
    except BalanceLimitExceeded:
        await _send(message, "❌ Бонус не начислен: достигнут максимальный баланс.")
        return
    if not claim.claimed:
        await _send(
            message,
            "⏳ Бонус уже получен. Следующий можно забрать через "
            f"{_display_duration(claim.remaining_seconds)}.\n"
            f"Баланс: {format_money(claim.balance_cents)}.",
        )
        return
    await _send(
        message,
        f"🎁 Вы получили бонус {format_money(bonus_dollars * 100)}. "
        f"Баланс: {format_money(claim.balance_cents)}.",
    )


async def _handle_transfer(
    message: Message,
    db: Database,
    player: User,
    arguments: list[str],
) -> None:
    if len(arguments) != 1:
        await _send(message, _usage("передать", "<сумма> (ответом на сообщение игрока)"))
        return
    try:
        amount = _parse_amount(arguments[0])
    except CommandInputError as exc:
        await _send(message, str(exc))
        return

    target = _reply_target(message)
    if target is None:
        await _send(
            message,
            "❌ Ответьте командой .передать <сумма> на сообщение игрока "
            "(например, в общей группе).",
        )
        return
    if target.id == player.id:
        await _send(message, "❌ Нельзя переводить средства самому себе.")
        return

    try:
        receipt = await db.transfer(
            player.id,
            player.username,
            player.first_name,
            target.id,
            target.username,
            target.first_name,
            amount,
        )
    except InsufficientFunds as exc:
        await _send(
            message,
            f"❌ Недостаточно средств. Ваш баланс: {format_money(exc.balance_cents)}.",
        )
        return
    except BalanceLimitExceeded:
        await _send(message, "❌ У получателя достигнут максимальный баланс.")
        return

    await _send(
        message,
        f"✅ Переведено {format_money(amount)} пользователю ID {target.id}.\n"
        f"Ваш баланс: {format_money(receipt.sender_balance_cents)}.",
    )


async def _handle_admin(
    message: Message,
    db: Database,
    player: User,
    owner_id: int | None,
    command: str,
    arguments: list[str],
) -> None:
    if owner_id is None:
        await _send(message, "⛔ Админ-команды отключены: задайте OWNER_ID в .env.")
        return
    if player.id != owner_id:
        await _send(message, "⛔ Эта команда доступна только владельцу бота.")
        return
    if len(arguments) != 1:
        syntax = "<сумма> (ответом на сообщение игрока)" if command == "выдать" else "<сумма>"
        await _send(message, _usage(command, syntax))
        return
    try:
        amount = _parse_amount(arguments[0])
    except CommandInputError as exc:
        await _send(message, str(exc))
        return

    target = _reply_target(message)
    if command == "выдать" and target is None:
        await _send(
            message,
            "❌ Для выдачи ответьте командой .выдать <сумма> на сообщение игрока.",
        )
        return

    # .снять without a reply withdraws from the owner's wallet. With a reply, it
    # withdraws from the replied-to player's wallet.
    account = target or player
    delta = amount if command == "выдать" else -amount
    if command == "выдать" and account.id == player.id:
        await _send(message, "❌ Нельзя выдать средства самому себе.")
        return

    try:
        balance = await db.adjust_balance(
            account.id,
            account.username,
            account.first_name,
            delta,
        )
    except InsufficientFunds as exc:
        await _send(
            message,
            f"❌ Недостаточно средств у пользователя ID {account.id}. "
            f"Баланс: {format_money(exc.balance_cents)}.",
        )
        return
    except BalanceLimitExceeded:
        await _send(message, "❌ Операция превысила максимально допустимый баланс.")
        return

    action = "Выдано" if command == "выдать" else "Снято"
    await _send(
        message,
        f"✅ {action} {format_money(amount)}. Пользователь ID {account.id}; "
        f"баланс: {format_money(balance)}.",
    )


async def _dispatch(
    message: Message,
    db: Database,
    owner_id: int | None,
    player: User,
    command: str,
    arguments: list[str],
) -> None:
    if command in {"ид", "id"}:
        if arguments:
            await _send(message, _usage("ид", ""))
        else:
            await _send(message, f"Ваш Telegram ID: {player.id}.")
        return

    if command in {"помощь", "help"}:
        await _send(message, _help_text())
        return

    if command in {"б", "баланс"}:
        if arguments:
            await _send(message, _usage(command, ""))
            return
        balance = await db.get_balance(player.id)
        await _send(message, f"💰 Ваш баланс: {format_money(balance)}.")
        return

    if command == "бонус":
        if arguments:
            await _send(message, _usage("бонус", ""))
            return
        await _handle_bonus(message, db, player)
        return

    if command == "передать":
        await _handle_transfer(message, db, player, arguments)
        return

    if command in {"выдать", "снять"}:
        await _handle_admin(message, db, player, owner_id, command, arguments)
        return

    if command in {"рулетка", "р"}:
        if len(arguments) != 2:
            await _send(message, _usage("рулетка", "<ставка> <к/ч/з/чет/нечет/число/#0>"))
            return
        try:
            stake = _parse_amount(arguments[0])
        except CommandInputError as exc:
            await _send(message, str(exc))
            return
        await _run_game(
            message,
            db,
            player,
            stake,
            lambda: play_roulette(stake, arguments[1]),
        )
        return

    if command == "кости":
        if len(arguments) != 2:
            await _send(message, _usage("кости", "<ставка> <число 1–6>"))
            return
        try:
            stake = _parse_amount(arguments[0])
            if not arguments[1].isdecimal():
                raise CommandInputError("❌ Для костей выберите число от 1 до 6.")
            chosen_number = int(arguments[1])
            if not 1 <= chosen_number <= 6:
                raise CommandInputError("❌ Для костей выберите число от 1 до 6.")
        except CommandInputError as exc:
            await _send(message, str(exc))
            return
        await _run_game(
            message,
            db,
            player,
            stake,
            lambda: play_dice_number(stake, chosen_number),
        )
        return

    if command == "куб":
        if len(arguments) != 2:
            await _send(message, _usage("куб", "<ставка> <чет/нечет>"))
            return
        try:
            stake = _parse_amount(arguments[0])
        except CommandInputError as exc:
            await _send(message, str(exc))
            return
        await _run_game(
            message,
            db,
            player,
            stake,
            lambda: play_dice_parity(stake, arguments[1]),
        )
        return

    if command in {"монетка"}:
        if len(arguments) != 2:
            await _send(message, _usage("монетка", "<ставка> <орел/решка или о/р>"))
            return
        try:
            stake = _parse_amount(arguments[0])
        except CommandInputError as exc:
            await _send(message, str(exc))
            return
        await _run_game(
            message,
            db,
            player,
            stake,
            lambda: play_coin(stake, arguments[1]),
        )
        return

    if command in {"краш", "ракета"}:
        if len(arguments) != 2:
            await _send(message, _usage("краш", "<ставка> <автовывод 1.00–100.00>"))
            return
        try:
            stake = _parse_amount(arguments[0])
            coefficient = _parse_coefficient(arguments[1])
        except CommandInputError as exc:
            await _send(message, str(exc))
            return
        await _run_game(
            message,
            db,
            player,
            stake,
            lambda: play_crash(stake, coefficient),
        )
        return

    if command in {"слоты", "с"}:
        if len(arguments) != 1:
            await _send(message, _usage("слоты", "<ставка>"))
            return
        try:
            stake = _parse_amount(arguments[0])
        except CommandInputError as exc:
            await _send(message, str(exc))
            return
        await _run_game(
            message,
            db,
            player,
            stake,
            lambda: play_slots(stake),
        )
        return

    if command == "бм":
        if len(arguments) != 2:
            await _send(message, _usage("бм", "<ставка> <б/м>"))
            return
        try:
            stake = _parse_amount(arguments[0])
        except CommandInputError as exc:
            await _send(message, str(exc))
            return
        await _run_game(
            message,
            db,
            player,
            stake,
            lambda: play_high_low(stake, arguments[1]),
        )
        return

    if command == "мины":
        if len(arguments) != 2:
            await _send(message, _usage("мины", "<ставка> <ячейка 1–9>"))
            return
        try:
            stake = _parse_amount(arguments[0])
            if not arguments[1].isdecimal():
                raise CommandInputError("❌ Номер ячейки должен быть от 1 до 9.")
            chosen_cell = int(arguments[1])
            if not 1 <= chosen_cell <= 9:
                raise CommandInputError("❌ Номер ячейки должен быть от 1 до 9.")
        except CommandInputError as exc:
            await _send(message, str(exc))
            return
        await _run_game(
            message,
            db,
            player,
            stake,
            lambda: play_mines(stake, chosen_cell),
        )
        return

    await _send(message, "❓ Неизвестная команда. Напишите .помощь для списка команд.")


async def _process_message(
    message: Message,
    db: Database,
    owner_id: int | None,
    *,
    business_update: bool = False,
) -> None:
    player = message.from_user
    if player is None or player.is_bot:
        return
    if message.chat.type == "channel":
        return
    if business_update and message.chat.type != "private":
        return

    # The first incoming message creates the profile, even if it is not a command.
    await db.ensure_user(player.id, player.username, player.first_name)

    text = message.text or message.caption or ""
    parsed = _parse_command(text)
    if parsed is None:
        if text.lstrip().startswith("."):
            await _send(message, "❌ Не удалось разобрать команду. Напишите .помощь.")
        return

    command, arguments = parsed
    await _dispatch(message, db, owner_id, player, command, arguments)


def create_router(db: Database, owner_id: int | None) -> Router:
    """Create a router handling ordinary messages and Telegram Business updates."""
    router = Router(name="virtual_casino")

    @router.message()
    async def on_message(message: Message) -> None:
        await _process_message(message, db, owner_id)

    @router.business_message()
    async def on_business_message(message: Message) -> None:
        await _process_message(message, db, owner_id, business_update=True)

    return router
