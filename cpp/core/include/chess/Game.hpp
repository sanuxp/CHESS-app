#pragma once

#include <optional>
#include <string>
#include <vector>

#include "chess/Board.hpp"
#include "chess/MoveGen.hpp"

namespace chess {

enum class GameStatus {
    Playing,
    Check,
    Checkmate,
    Stalemate,
    DrawFiftyMove,
    DrawRepetition,
    DrawInsufficient,
};

inline bool isGameOver(GameStatus s) {
    return s != GameStatus::Playing && s != GameStatus::Check;
}

struct HistoryEntry {
    Board before;      // position before the move
    Move move;
    std::string san;   // e.g. "Nf3", "exd5", "O-O", "e8=Q#"
};

// A game in progress: current position, move history, undo and end-of-game detection.
class Game {
public:
    Game();                           // standard start
    explicit Game(const Board& start);
    static bool fromFEN(const std::string& fen, Game& out);

    const Board& board() const { return board_; }
    const Board& initialBoard() const { return initial_; }
    const std::vector<HistoryEntry>& history() const { return history_; }
    const std::vector<Move>& legalMoves() const { return legal_; }
    std::vector<Move> legalMovesFrom(Square from) const;

    // Plays a legal move; returns false (state unchanged) if the move is not legal here.
    bool makeMove(Square from, Square to, PieceType promo = PieceType::None);
    bool makeMove(const Move& m) { return makeMove(m.from, m.to, m.promo); }
    // Plays a move written like "e2e4" or "e7e8q".
    bool makeMoveUCI(const std::string& uci);
    // Plays a move in standard algebraic notation such as "Nf3" or "O-O".
    bool makeMoveSAN(const std::string& san);
    bool undo();

    GameStatus status() const { return status_; }
    bool inCheck() const { return isInCheck(board_, board_.sideToMove); }
    std::optional<Move> lastMove() const;

    // "1-0", "0-1", "1/2-1/2" or "*" while the game is still going.
    std::string resultToken() const;

private:
    void refresh();
    std::string positionKey(const Board& b) const;

    Board initial_;
    Board board_;
    std::vector<HistoryEntry> history_;
    std::vector<std::string> keys_;  // keys_[i] = position after i moves (keys_[0] = initial)
    std::vector<Move> legal_;
    GameStatus status_ = GameStatus::Playing;
};

}  // namespace chess
