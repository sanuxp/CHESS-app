import 'package:flutter/material.dart';

import '../review/classify.dart';

/// Colour and glyph shown for a move label (badge on the board, marks in the move list).
class LabelStyle {
  final Color color;
  final String glyph; // text mark such as "!!"; also the fallback when there is no icon
  final IconData? icon; // symbols that fonts may lack (star, check, book...) are drawn as icons
  const LabelStyle(this.color, this.glyph, {this.icon});
}

LabelStyle labelStyle(MoveLabel l) => switch (l) {
      MoveLabel.brilliant => const LabelStyle(Color(0xFF1BAAA6), '!!'),
      MoveLabel.great => const LabelStyle(Color(0xFF5C8BB0), '!'),
      MoveLabel.best => const LabelStyle(Color(0xFF81B64C), '*', icon: Icons.star),
      MoveLabel.excellent => const LabelStyle(Color(0xFF96BC4B), '+', icon: Icons.thumb_up),
      MoveLabel.good => const LabelStyle(Color(0xFFA3B18A), 'ok', icon: Icons.check),
      MoveLabel.book => const LabelStyle(Color(0xFFA88865), 'book', icon: Icons.menu_book),
      MoveLabel.forced => const LabelStyle(Color(0xFF8A8A8A), '->', icon: Icons.arrow_forward),
      MoveLabel.inaccuracy => const LabelStyle(Color(0xFFF7C631), '?!'),
      MoveLabel.mistake => const LabelStyle(Color(0xFFE58F2A), '?'),
      MoveLabel.miss => const LabelStyle(Color(0xFFE05A4F), 'x', icon: Icons.close),
      MoveLabel.blunder => const LabelStyle(Color(0xFFCA3431), '??'),
    };

/// One-line explanation used under the move list.
String labelMeaning(MoveLabel l) => switch (l) {
      MoveLabel.brilliant => 'a brilliant sacrifice',
      MoveLabel.great => 'a great find, the only good move',
      MoveLabel.best => 'the best move',
      MoveLabel.excellent => 'an excellent move',
      MoveLabel.good => 'a good move',
      MoveLabel.book => 'an opening book move',
      MoveLabel.forced => 'the only legal move',
      MoveLabel.inaccuracy => 'an inaccuracy',
      MoveLabel.miss => 'a missed chance',
      MoveLabel.mistake => 'a mistake',
      MoveLabel.blunder => 'a blunder',
    };
