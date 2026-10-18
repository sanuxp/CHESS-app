#include "chess/Game.hpp"

#include "chess/Pgn.hpp"

namespace chess {

namespace {

bool insufficientMaterial(const Board& b) {
    int knights = 0, bishops = 0;
    int bishopsOnLight = 0, bishopsOnDark = 0;
    for (Square s = 0; s < 64; ++s) {
        Piece p = b.pieceAt(s);
        switch (p.type) {
            case PieceType::None:
            case PieceType::King: break;
            case PieceType::Knight: ++knights; break;
            case PieceType::Bishop:
                ++bishops;
                ((fileOf(s) + rankOf(s)) % 2 == 0 ? bishopsOnDark : bishopsOnLight)++;
                break;
            default: return false;  // pawn, rook or queen on the board
        }
    }
    if (knights + bishops == 0) return true;                      // K v K
    if (knights + bishops == 1) return true;                      // K+minor v K
    if (knights == 0) return bishopsOnLight == 0 || bishopsOnDark == 0;  // only same-coloured bishops
    return false;
}

}  // namespace

Game::Game() : Game(Board()) {}

Game::Game(const Board& start) : initial_(start), board_(start) {
    keys_.push_back(positionKey(board_));
    refresh();
}

bool Game::fromFEN(const std::string& fen, Game& out) {
    Board b;
    if (!Board::fromFEN(fen, b)) return false;
    out = Game(b);
    return true;
}

std::vector<Move> Game::legalMovesFrom(Square from) const {
    std::vector<Move> result;
    for (const Move& m : legal_) {
        if (m.from == from) result.push_back(m);
    }
    return result;
}

bool Game::makeMove(Square from, Square to, PieceType promo) {
    for (const Move& m : legal_) {
        if (m.from == from && m.to == to && m.promo == promo) {
            HistoryEntry entry{board_, m, moveToSAN(board_, m)};
            board_.applyMove(m);
            history_.push_back(std::move(entry));
            keys_.push_back(positionKey(board_));
            refresh();
            return true;
        }
    }
    return false;
}

bool Game::makeMoveUCI(const std::string& uci) {
    if (uci.size() < 4 || uci.size() > 5) return false;
    Square from = squareFromName(uci.substr(0, 2));
    Square to = squareFromName(uci.substr(2, 2));
    if (from == NoSquare || to == NoSquare) return false;
    PieceType promo = PieceType::None;
    if (uci.size() == 5) {
        switch (uci[4]) {
            case 'q': promo = PieceType::Queen; break;
            case 'r': promo = PieceType::Rook; break;
            case 'b': promo = PieceType::Bishop; break;
            case 'n': promo = PieceType::Knight; break;
            default: return false;
        }
    }
    return makeMove(from, to, promo);
}

bool Game::makeMoveSAN(const std::string& san) {
    std::optional<Move> m = moveFromSAN(board_, san);
    return m && makeMove(*m);
}

bool Game::undo() {
    if (history_.empty()) return false;
    board_ = history_.back().before;
    history_.pop_back();
    keys_.pop_back();
    refresh();
    return true;
}

std::optional<Move> Game::lastMove() const {
    if (history_.empty()) return std::nullopt;
    return history_.back().move;
}

std::string Game::resultToken() const {
    switch (status_) {
        case GameStatus::Checkmate: return board_.sideToMove == Color::White ? "0-1" : "1-0";
        case GameStatus::Stalemate:
        case GameStatus::DrawFiftyMove:
        case GameStatus::DrawRepetition:
        case GameStatus::DrawInsufficient: return "1/2-1/2";
        default: return "*";
    }
}

void Game::refresh() {
    legal_ = generateLegal(board_);
    bool check = isInCheck(board_, board_.sideToMove);

    if (legal_.empty()) {
        status_ = check ? GameStatus::Checkmate : GameStatus::Stalemate;
    } else if (insufficientMaterial(board_)) {
        status_ = GameStatus::DrawInsufficient;
    } else {
        int repeats = 0;
        for (const std::string& k : keys_) {
            if (k == keys_.back()) ++repeats;
        }
        if (repeats >= 3) status_ = GameStatus::DrawRepetition;
        else if (board_.halfmoveClock >= 100) status_ = GameStatus::DrawFiftyMove;
        else status_ = check ? GameStatus::Check : GameStatus::Playing;
    }
}

// Repetition key: placement, side, castling rights and an en-passant square only if a capture onto it is legal.
std::string Game::positionKey(const Board& b) const {
    Board copy = b;
    bool epUsable = false;
    if (copy.epSquare != NoSquare) {
        for (const Move& m : generateLegal(copy)) {
            if (m.isEnPassant()) {
                epUsable = true;
                break;
            }
        }
    }
    if (!epUsable) copy.epSquare = NoSquare;
    copy.halfmoveClock = 0;
    copy.fullmoveNumber = 1;
    return copy.toFEN();
}

}  // namespace chess
