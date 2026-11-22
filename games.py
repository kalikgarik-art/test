"""Game outcome generation. All random outcomes use Python's ``secrets`` module."""

from __future__ import annotations

import secrets
from collections import Counter
from dataclasses import dataclass
from decimal import Decimal, ROUND_FLOOR, ROUND_HALF_UP


@dataclass(frozen=True, slots=True)
class GameOutcome:
    """The description to show the player and gross payout (including the stake)."""

    description: str
    payout_cents: int

    @property
    def won(self) -> bool:
        return self.payout_cents > 0


def _payout(stake_cents: int, multiplier: Decimal) -> int:
    return int(
        (Decimal(stake_cents) * multiplier).quantize(
            Decimal("1"), rounding=ROUND_HALF_UP
        )
    )


def _multiplier_text(multiplier: Decimal) -> str:
    return format(multiplier.normalize(), "f")


_RED_NUMBERS = {
    1, 3, 5, 7, 9, 12, 14, 16, 18,
    19, 21, 23, 25, 27, 30, 32, 34, 36,
}
_BLACK_NUMBERS = set(range(1, 37)) - _RED_NUMBERS


def play_roulette(stake_cents: int, selection: str) -> GameOutcome:
    """Play European roulette. ``0`` selects green; ``#0`` selects the exact number."""
    token = selection.casefold().replace("ё", "е")
    if token in {"к", "красное", "красный"}:
        kind, chosen_number, label, multiplier = "red", None, "красное", Decimal("2")
    elif token in {"ч", "черное", "черный"}:
        kind, chosen_number, label, multiplier = "black", None, "чёрное", Decimal("2")
    elif token in {"з", "зеленое", "зеленый", "0"}:
        kind, chosen_number, label, multiplier = "green", None, "зелёное (0)", Decimal("14")
    elif token in {"чет", "четное"}:
        kind, chosen_number, label, multiplier = "even", None, "чёт", Decimal("2")
    elif token in {"нечет", "нечетное"}:
        kind, chosen_number, label, multiplier = "odd", None, "нечёт", Decimal("2")
    elif token in {"#0", "n0", "число:0"}:
        kind, chosen_number, label, multiplier = "number", 0, "число 0", Decimal("36")
    elif token.isdecimal() and 1 <= int(token) <= 36:
        chosen_number = int(token)
        kind, label, multiplier = "number", f"число {chosen_number}", Decimal("36")
    else:
        raise ValueError(
            "Выбор рулетки: к/красное, ч/чёрное, з/зелёное, чет, нечет, "
            "число 1–36 или #0 для точного числа 0."
        )

    number = secrets.randbelow(37)
    if number == 0:
        color, emoji = "зелёное", "🟢"
    elif number in _RED_NUMBERS:
        color, emoji = "красное", "🔴"
    else:
        color, emoji = "чёрное", "⚫"

    won = (
        (kind == "red" and number in _RED_NUMBERS)
        or (kind == "black" and number in _BLACK_NUMBERS)
        or (kind == "green" and number == 0)
        or (kind == "even" and number != 0 and number % 2 == 0)
        or (kind == "odd" and number % 2 == 1)
        or (kind == "number" and number == chosen_number)
    )
    payout = _payout(stake_cents, multiplier) if won else 0
    description = (
        f"🎰 Рулетка: выпало {number} {color} {emoji}. "
        f"Вы выбрали {label} (x{_multiplier_text(multiplier)})."
    )
    return GameOutcome(description=description, payout_cents=payout)


def play_dice_number(stake_cents: int, chosen_number: int) -> GameOutcome:
    if not 1 <= chosen_number <= 6:
        raise ValueError("В кости нужно выбрать число от 1 до 6.")
    roll = secrets.randbelow(6) + 1
    multiplier = Decimal("5")
    payout = _payout(stake_cents, multiplier) if roll == chosen_number else 0
    description = (
        f"🎲 Кости: выпало {roll}. Вы выбрали {chosen_number} (x5)."
    )
    return GameOutcome(description=description, payout_cents=payout)


def play_dice_parity(stake_cents: int, parity: str) -> GameOutcome:
    token = parity.casefold().replace("ё", "е")
    if token in {"ч", "чет", "четное"}:
        is_even, label = True, "чёт"
    elif token in {"н", "нечет", "нечетное"}:
        is_even, label = False, "нечёт"
    else:
        raise ValueError("Для .куб выберите чет или нечет.")

    roll = secrets.randbelow(6) + 1
    multiplier = Decimal("1.9")
    won = (roll % 2 == 0) == is_even
    payout = _payout(stake_cents, multiplier) if won else 0
    description = (
        f"🎲 Кубик: выпало {roll}. Вы выбрали {label} (x1.9)."
    )
    return GameOutcome(description=description, payout_cents=payout)


def play_coin(stake_cents: int, guess: str) -> GameOutcome:
    token = guess.casefold().replace("ё", "е")
    if token in {"о", "орел"}:
        chosen, label = "орёл", "орёл"
    elif token in {"р", "решка"}:
        chosen, label = "решка", "решка"
    else:
        raise ValueError("Для монетки выберите орел/о или решка/р.")

    result = secrets.choice(("орёл", "решка"))
    multiplier = Decimal("1.95")
    payout = _payout(stake_cents, multiplier) if result == chosen else 0
    description = (
        f"🪙 Монетка: выпало {result}. Вы выбрали {label} (x1.95)."
    )
    return GameOutcome(description=description, payout_cents=payout)


def _generate_crash_multiplier() -> Decimal:
    """Sample a capped, inverse-tail crash point with an approximately 1% edge."""
    # For U uniformly distributed on [0, 1), 0.99 / (1 - U) has a heavy upper tail.
    # Flooring to cents and capping at 100 keeps the displayed range at 1.00–100.00.
    u = Decimal(secrets.randbelow(1_000_000)) / Decimal(1_000_000)
    raw = Decimal("0.99") / (Decimal(1) - u)
    multiplier = raw.quantize(Decimal("0.01"), rounding=ROUND_FLOOR)
    return min(Decimal("100.00"), max(Decimal("1.00"), multiplier))


def play_crash(stake_cents: int, cashout: Decimal) -> GameOutcome:
    if cashout < Decimal("1.00") or cashout > Decimal("100.00"):
        raise ValueError("Автовывод должен быть от 1.00 до 100.00.")
    crash_point = _generate_crash_multiplier()
    won = crash_point >= cashout
    payout = _payout(stake_cents, cashout) if won else 0
    description = (
        f"🚀 Краш: {crash_point:.2f}x. Автовывод: {cashout:.2f}x "
        f"(выплата при краше не ниже выбранного коэффициента)."
    )
    return GameOutcome(description=description, payout_cents=payout)


_SLOT_SYMBOLS = ("🍒", "🍋", "🍇", "💎", "7️⃣")


def play_slots(stake_cents: int) -> GameOutcome:
    symbols = tuple(secrets.choice(_SLOT_SYMBOLS) for _ in range(3))
    counts = Counter(symbols)
    most_common_count = max(counts.values())

    if most_common_count == 3:
        symbol = symbols[0]
        if symbol == "7️⃣":
            multiplier, prize = Decimal("50"), "три семёрки"
        elif symbol == "💎":
            multiplier, prize = Decimal("15"), "три бриллианта"
        else:
            multiplier, prize = Decimal("5"), "три одинаковых фрукта"
    elif most_common_count == 2:
        multiplier, prize = Decimal("1.5"), "два одинаковых символа"
    else:
        multiplier, prize = Decimal("0"), "выигрышной комбинации нет"

    payout = _payout(stake_cents, multiplier) if multiplier else 0
    description = (
        f"🎰 Слоты: {' | '.join(symbols)} — {prize}"
        + (f" (x{_multiplier_text(multiplier)})." if multiplier else ".")
    )
    return GameOutcome(description=description, payout_cents=payout)


def play_high_low(stake_cents: int, direction: str) -> GameOutcome:
    token = direction.casefold().replace("ё", "е")
    if token in {"б", "больше"}:
        chosen, label = "high", "больше 50"
    elif token in {"м", "меньше"}:
        chosen, label = "low", "меньше 50"
    else:
        raise ValueError("Для .бм выберите б (больше) или м (меньше).")

    number = secrets.randbelow(100) + 1
    won = (chosen == "high" and number > 50) or (chosen == "low" and number < 50)
    multiplier = Decimal("1.95")
    payout = _payout(stake_cents, multiplier) if won else 0
    if number == 50:
        result_text = "50 — ставка проиграла казино"
    else:
        result_text = f"ваш выбор: {label} (x1.95)"
    description = f"🎲 Больше/меньше: выпало {number}; {result_text}."
    return GameOutcome(description=description, payout_cents=payout)


def play_mines(stake_cents: int, chosen_cell: int) -> GameOutcome:
    if not 1 <= chosen_cell <= 9:
        raise ValueError("Номер ячейки должен быть от 1 до 9.")

    remaining = list(range(1, 10))
    mines: set[int] = set()
    for _ in range(3):
        mines.add(remaining.pop(secrets.randbelow(len(remaining))))

    safe = chosen_cell not in mines
    board: list[str] = []
    for cell in range(1, 10):
        if cell in mines:
            symbol = "💣" if cell != chosen_cell else "💥"
        elif cell == chosen_cell:
            symbol = "✅"
        else:
            symbol = "⬜"
        board.append(f"{cell}{symbol}")

    rows = ["  ".join(board[index:index + 3]) for index in (0, 3, 6)]
    field = "\n".join(rows)
    if safe:
        multiplier = Decimal("1.4")
        payout = _payout(stake_cents, multiplier)
        ending = "Безопасно! Выигрыш x1.4."
    else:
        payout = 0
        ending = "Мина! Ставка потеряна."

    description = (
        f"💣 Мины: выбрана ячейка {chosen_cell}.\n{field}\n{ending}"
    )
    return GameOutcome(description=description, payout_cents=payout)
