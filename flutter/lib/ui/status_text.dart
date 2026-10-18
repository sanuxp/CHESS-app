import '../core/game.dart';
import '../core/types.dart';

/// "WHITE TO MOVE" while playing, "GAME OVER" once finished.
String statusHeadline(Game g) {
  if (g.status.isOver) return 'GAME OVER';
  return g.board.sideToMove == PieceColor.white ? 'WHITE TO MOVE' : 'BLACK TO MOVE';
}

/// Extra line: "CHECK!", "CHECKMATE - WHITE WINS", the draw reason, or empty.
String statusDetail(Game g) => switch (g.status) {
      GameStatus.check => 'CHECK!',
      GameStatus.checkmate =>
        g.board.sideToMove == PieceColor.white ? 'CHECKMATE - BLACK WINS' : 'CHECKMATE - WHITE WINS',
      GameStatus.stalemate => 'STALEMATE - DRAW',
      GameStatus.drawFiftyMove => 'DRAW BY 50 MOVE RULE',
      GameStatus.drawRepetition => 'DRAW BY REPETITION',
      GameStatus.drawInsufficient => 'DRAW - INSUFFICIENT MATERIAL',
      GameStatus.playing => '',
    };
