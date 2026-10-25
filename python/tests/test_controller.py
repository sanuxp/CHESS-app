"""Interaction logic of the game screen: clicks, drags, promotion, animation, undo, flip, status text."""
from chessapp.core import START_FEN, Game
from chessapp.core.types import KNIGHT, NO_SQUARE, QUEEN, square_from_name as sq
from chessapp.ui.controller import ANIM_SECONDS, GameController, status_detail, status_headline


def ctl(fen=None, animations=True):
    return GameController(Game.from_fen(fen) if fen else Game(), animations=animations)


def settle(c, now=0.0):
    c.update(now + ANIM_SECONDS + 0.01)


def test_click_piece_then_target_plays_move_with_animation():
    c = ctl()
    c.click(sq("e2"), 0)
    assert c.selected == sq("e2") and {m.to_sq for m in c.targets} == {sq("e3"), sq("e4")}
    c.click(sq("e4"), 0)
    assert c.game.move_count == 1 and c.selected == NO_SQUARE
    assert c.busy and c.anim.slides[0][1:] == (sq("e2"), sq("e4")) and c.anim.hide == {sq("e4")}
    settle(c)
    assert not c.busy


def test_input_ignored_while_animating():
    c = ctl()
    c.click(sq("e2"), 0)
    c.click(sq("e4"), 0)
    c.click(sq("e7"), 0.05)
    assert c.selected == NO_SQUARE
    assert not c.undo(0.05)


def test_click_other_own_piece_switches_selection_and_empty_deselects():
    c = ctl()
    c.click(sq("e2"), 0)
    c.click(sq("g1"), 0)
    assert c.selected == sq("g1")
    c.click(sq("e5"), 0)  # not a legal target
    assert c.selected == NO_SQUARE


def test_cannot_select_opponent_piece():
    c = ctl()
    c.click(sq("e7"), 0)
    assert c.selected == NO_SQUARE


def test_clicking_selected_piece_again_deselects():
    c = ctl()
    c.click(sq("e2"), 0)
    c.click(sq("e2"), 0)
    assert c.selected == NO_SQUARE


def test_drag_plays_without_animation_after_threshold():
    c = ctl()
    c.press(sq("g1"), (100, 100))
    c.move_pointer((103, 100))
    assert not c.dragging  # under 6 px
    c.move_pointer((120, 80))
    assert c.dragging and c.drag_piece_square == sq("g1")
    c.release(sq("f3"), 0)
    assert c.game.move_count == 1 and not c.busy


def test_drag_dropped_on_illegal_square_returns_piece():
    c = ctl()
    c.press(sq("g1"), (0, 0))
    c.move_pointer((50, 50))
    c.release(sq("g5"), 0)
    assert c.game.move_count == 0 and not c.dragging and c.selected == sq("g1")


def test_castling_slides_king_and_rook_together():
    c = ctl("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1")
    c.click(sq("e1"), 0)
    c.click(sq("g1"), 0)
    assert {(a, b) for _, a, b in c.anim.slides} == {(sq("e1"), sq("g1")), (sq("h1"), sq("f1"))}
    assert c.anim.hide == {sq("g1"), sq("f1")}


def test_capture_fades_victim_out_and_en_passant_uses_real_square():
    c = ctl("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1")
    c.click(sq("e5"), 0)
    c.click(sq("d6"), 0)
    assert [(sq_, fading_in) for _, sq_, fading_in in c.anim.fades] == [(sq("d5"), False)]


def test_promotion_picker_and_choice():
    c = ctl("4k3/P7/8/8/8/8/8/4K3 w - - 0 1")
    c.click(sq("a7"), 0)
    c.click(sq("a8"), 0)
    assert c.promotion is not None and c.game.move_count == 0
    c.click(sq("e4"), 0)  # board input ignored while the picker is open
    assert c.game.move_count == 0
    assert c.choose_promotion(KNIGHT, 0)
    assert c.game.history[0].san.startswith("a8=N") and c.promotion is None


def test_promotion_cancel_with_escape_keeps_selection_then_deselects():
    c = ctl("4k3/P7/8/8/8/8/8/4K3 w - - 0 1")
    c.click(sq("a7"), 0)
    c.click(sq("a8"), 0)
    c.cancel()
    assert c.promotion is None and c.selected == sq("a7")
    c.cancel()
    assert c.selected == NO_SQUARE


def test_undo_animates_in_reverse_and_restores_position():
    c = ctl("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1")
    c.click(sq("e4"), 0)
    c.click(sq("d5"), 0)
    settle(c)
    assert c.undo(1.0)
    assert c.game.board.to_fen() == "4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1"
    assert c.anim.reverse and c.anim.slides[0][1:] == (sq("d5"), sq("e4"))
    assert [(s, fading_in) for _, s, fading_in in c.anim.fades] == [(sq("d5"), True)]


def test_animations_off_means_no_animation():
    c = ctl(animations=False)
    c.click(sq("e2"), 0)
    c.click(sq("e4"), 0)
    assert not c.busy
    assert c.undo(0) and not c.busy


def test_undo_with_nothing_to_undo_and_new_game():
    c = ctl()
    assert not c.undo(0)
    c.click(sq("e2"), 0)
    c.click(sq("e4"), 0)
    settle(c)
    c.new_game()
    assert c.game.board.to_fen() == START_FEN


def test_flip_mode_faces_the_side_to_move():
    c = ctl(animations=False)
    assert not c.flipped
    c.flip_mode = True
    assert not c.flipped
    c.click(sq("e2"), 0)
    c.click(sq("e4"), 0)
    assert c.flipped


def test_game_over_blocks_input():
    c = ctl("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1")  # stalemate
    c.click(sq("h8"), 0)
    assert c.selected == NO_SQUARE


def test_status_text():
    g = Game()
    assert status_headline(g) == "WHITE TO MOVE" and status_detail(g) == ""
    for u in ("f2f3", "e7e5", "g2g4", "d8h4"):
        g.make_move_uci(u)
    assert status_headline(g) == "GAME OVER" and status_detail(g) == "CHECKMATE - BLACK WINS"
    assert status_detail(Game.from_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1")) == "STALEMATE - DRAW"
    assert status_detail(Game.from_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1")) == "DRAW - INSUFFICIENT MATERIAL"


def test_toast_visible_three_seconds_then_fades():
    c = ctl()
    assert c.toast_alpha(0) == 0
    c.show_toast("SAVED", 10)
    assert c.toast_alpha(12.9) == 1
    assert 0 < c.toast_alpha(13.5) < 1
    assert c.toast_alpha(14.1) == 0


def test_queen_promotion_move_is_recorded():
    c = ctl("4k3/P7/8/8/8/8/8/4K3 w - - 0 1")
    c.click(sq("a7"), 0)
    c.click(sq("a8"), 0)
    c.choose_promotion(QUEEN, 0)
    assert c.game.history[0].san == "a8=Q+"
