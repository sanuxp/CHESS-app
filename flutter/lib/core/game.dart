import 'board.dart';
import 'movegen.dart';
import 'san.dart';
import 'types.dart';

enum GameStatus { playing, check, checkmate, stalemate, drawFiftyMove, drawRepetition, drawInsufficient }

extension GameStatusX on GameStatus {
  bool get isOver => this != GameStatus.playing && this != GameStatus.check;

  /// Name used by the shared test data ("Checkmate", "DrawFiftyMove", ...).
  String get dataName => name[0].toUpperCase() + name.substring(1);
}

class HistoryEntry {
  final Board before; // position before the move
  final Move move;
  final String san;
  const HistoryEntry(this.before, this.move, this.san);
}

bool _insufficientMaterial(Board b) {
  var knights = 0, bishops = 0, light = 0, dark = 0;
  for (var s = 0; s < 64; s++) {
    switch (b.pieceAt(s).type) {
      case PieceType.none || PieceType.king:
        break;
      case PieceType.knight:
        knights++;
      case PieceType.bishop:
        bishops++;
        (fileOf(s) + rankOf(s)) % 2 == 0 ? dark++ : light++;
      default:
        return false; // pawn, rook or queen on the board
    }
  }
  if (knights + bishops <= 1) return true; // K v K, K+minor v K
  if (knights == 0) return light == 0 || dark == 0; // only same-coloured bishops
  return false;
}

/// A game in progress: position, history, undo and end-of-game detection.
class Game {
  final Board initial;
  Board _board;
  final List<HistoryEntry> _history = [];
  final List<String> _keys = [];
  List<Move> _legal = const [];
  GameStatus _status = GameStatus.playing;

  Game([Board? start])
      : initial = start ?? Board.start(),
        _board = (start ?? Board.start()).copy() {
    _keys.add(_positionKey(_board));
    _refresh();
  }

  /// A game from a FEN string, or null if the FEN is invalid.
  static Game? fromFen(String fen) {
    final b = Board.fromFen(fen);
    return b == null ? null : Game(b);
  }

  Board get board => _board;
  List<HistoryEntry> get history => List.unmodifiable(_history);
  int get moveCount => _history.length;
  List<Move> get legalMoves => _legal;
  GameStatus get status => _status;
  bool get inCheck => isInCheck(_board, _board.sideToMove);
  Move? get lastMove => _history.isEmpty ? null : _history.last.move;

  List<Move> legalMovesFrom(int from) => [for (final m in _legal) if (m.from == from) m];

  /// Plays a legal move; returns false (state unchanged) if it is not legal here.
  bool makeMove(int from, int to, [PieceType promo = PieceType.none]) {
    for (final m in _legal) {
      if (m.from == from && m.to == to && m.promo == promo) {
        _history.add(HistoryEntry(_board.copy(), m, moveToSan(_board, m)));
        _board.applyMove(m);
        _keys.add(_positionKey(_board));
        _refresh();
        return true;
      }
    }
    return false;
  }

  bool playMove(Move m) => makeMove(m.from, m.to, m.promo);

  /// Plays a move written like "e2e4" or "e7e8q".
  bool makeMoveUci(String uci) {
    if (uci.length < 4 || uci.length > 5) return false;
    final from = squareFromName(uci.substring(0, 2)), to = squareFromName(uci.substring(2, 4));
    if (from == noSquare || to == noSquare) return false;
    var promo = PieceType.none;
    if (uci.length == 5) {
      promo = switch (uci[4]) {
        'q' => PieceType.queen,
        'r' => PieceType.rook,
        'b' => PieceType.bishop,
        'n' => PieceType.knight,
        _ => PieceType.none,
      };
      if (promo == PieceType.none) return false;
    }
    return makeMove(from, to, promo);
  }

  /// Plays a move in standard algebraic notation such as "Nf3" or "O-O".
  bool makeMoveSan(String san) {
    final m = moveFromSan(_board, san);
    return m != null && playMove(m);
  }

  bool undo() {
    if (_history.isEmpty) return false;
    _board = _history.removeLast().before;
    _keys.removeLast();
    _refresh();
    return true;
  }

  /// "1-0", "0-1", "1/2-1/2", or "*" while the game is still going.
  String get resultToken => switch (_status) {
        GameStatus.checkmate => _board.sideToMove == PieceColor.white ? '0-1' : '1-0',
        GameStatus.stalemate ||
        GameStatus.drawFiftyMove ||
        GameStatus.drawRepetition ||
        GameStatus.drawInsufficient =>
          '1/2-1/2',
        _ => '*',
      };

  void _refresh() {
    _legal = generateLegal(_board);
    final check = isInCheck(_board, _board.sideToMove);
    if (_legal.isEmpty) {
      _status = check ? GameStatus.checkmate : GameStatus.stalemate;
    } else if (_insufficientMaterial(_board)) {
      _status = GameStatus.drawInsufficient;
    } else if (_keys.where((k) => k == _keys.last).length >= 3) {
      _status = GameStatus.drawRepetition;
    } else if (_board.halfmoveClock >= 100) {
      _status = GameStatus.drawFiftyMove;
    } else {
      _status = check ? GameStatus.check : GameStatus.playing;
    }
  }

  // Placement, side, castling rights, and en passant only if a capture onto it is legal.
  static String _positionKey(Board b) {
    final copy = b.copy();
    if (copy.epSquare != noSquare && !generateLegal(copy).any((m) => m.isEnPassant)) {
      copy.epSquare = noSquare;
    }
    copy.halfmoveClock = 0;
    copy.fullmoveNumber = 1;
    return copy.toFen();
  }
}
