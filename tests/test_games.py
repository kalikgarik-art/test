from decimal import Decimal

import games


def test_roulette_red_win_uses_secure_roll(monkeypatch):
    monkeypatch.setattr(games.secrets, "randbelow", lambda upper: 1)  # red 1
    outcome = games.play_roulette(1000, "К")
    assert outcome.payout_cents == 2000
    assert outcome.won


def test_roulette_zero_is_green_but_hash_zero_is_exact(monkeypatch):
    monkeypatch.setattr(games.secrets, "randbelow", lambda upper: 0)
    green = games.play_roulette(1000, "0")
    exact = games.play_roulette(1000, "#0")
    assert green.payout_cents == 14_000
    assert exact.payout_cents == 36_000


def test_coin_pays_gross_amount_including_stake(monkeypatch):
    monkeypatch.setattr(games.secrets, "choice", lambda values: "орёл")
    outcome = games.play_coin(10_000, "о")
    assert outcome.payout_cents == 19_500


def test_crash_target_is_rounded_down_for_result_and_uses_threshold(monkeypatch):
    monkeypatch.setattr(games.secrets, "randbelow", lambda upper: 500_000)
    outcome = games.play_crash(10_000, Decimal("1.50"))
    assert outcome.won
    assert outcome.payout_cents == 15_000


def test_mines_reveals_selected_safe_cell(monkeypatch):
    # Choose the first remaining cell each time, leaving cell 4 safe.
    values = iter((0, 0, 0))
    monkeypatch.setattr(games.secrets, "randbelow", lambda upper: next(values))
    outcome = games.play_mines(10_000, 4)
    assert outcome.won
    assert outcome.payout_cents == 14_000
    assert "✅" in outcome.description
