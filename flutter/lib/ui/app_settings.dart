import 'dart:ui' show Brightness, Color;

import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

// Customization data and state, see docs/THEMES.md. Names, colours and defaults are fixed by that spec.

/// Light and dark square colours.
class BoardTheme {
  final String name;
  final Color light, dark;
  const BoardTheme(this.name, this.light, this.dark);
}

const boardThemes = <BoardTheme>[
  BoardTheme('Brown', Color(0xFFF0D9B5), Color(0xFFB58863)),
  BoardTheme('Green', Color(0xFFEEEED2), Color(0xFF769656)),
  BoardTheme('Blue', Color(0xFFDEE3E6), Color(0xFF8CA2AD)),
  BoardTheme('Purple', Color(0xFFEFEFEF), Color(0xFF8C77B5)),
  BoardTheme('Slate', Color(0xFFDCDCDC), Color(0xFF828282)),
  BoardTheme('Ice', Color(0xFFE8F0F7), Color(0xFF7898BA)),
  BoardTheme('Coral', Color(0xFFF5E1D7), Color(0xFFC46E5F)),
  BoardTheme('Walnut', Color(0xFFC8AA82), Color(0xFF6E4B32)),
];

/// How a piece body is drawn.
enum PieceMode {
  /// Horizontal hatch lines, heavier towards the right edge: body colour A, line colour B.
  hatch,

  /// One flat colour A.
  flat,

  /// Horizontal gradient from A (left) to B (right).
  gradient,
}

class PieceSet {
  final String name;
  final PieceMode mode;
  final Color whiteA, whiteB, blackA, blackB;
  final Color whiteOutline, blackOutline;
  final Color whiteInk, blackInk; // detail strokes (bishop cut, king band, knight eye...)
  const PieceSet(
    this.name,
    this.mode, {
    required this.whiteA,
    this.whiteB = const Color(0x00000000),
    required this.blackA,
    this.blackB = const Color(0x00000000),
    required this.whiteOutline,
    required this.blackOutline,
    required this.whiteInk,
    required this.blackInk,
  });
}

const pieceSets = <PieceSet>[
  PieceSet('Engraved', PieceMode.hatch,
      whiteA: Color(0xFFFAF8F2),
      whiteB: Color(0xFF1E1C1C),
      blackA: Color(0xFF1A1A1E),
      blackB: Color(0xFFD6D6DC),
      whiteOutline: Color(0xFF161414),
      blackOutline: Color(0xFF08080A),
      whiteInk: Color(0xFF161414),
      blackInk: Color(0xFFD6D6DC)),
  PieceSet('Classic', PieceMode.flat,
      whiteA: Color(0xFFF8F6EE),
      blackA: Color(0xFF4A4C56),
      whiteOutline: Color(0xFF26221F),
      blackOutline: Color(0xFF0E0E12),
      whiteInk: Color(0xFF463F3A),
      blackInk: Color(0xFFC8C8D2)),
  PieceSet('Shaded', PieceMode.gradient,
      whiteA: Color(0xFFFFFFFF),
      whiteB: Color(0xFFB9B4A8),
      blackA: Color(0xFF5A5C68),
      blackB: Color(0xFF101014),
      whiteOutline: Color(0xFF26221F),
      blackOutline: Color(0xFF08080A),
      whiteInk: Color(0xFF6A655A),
      blackInk: Color(0xFFC8C8D2)),
  PieceSet('Wood', PieceMode.gradient,
      whiteA: Color(0xFFEBC98F),
      whiteB: Color(0xFFB98A4E),
      blackA: Color(0xFF7A4A2A),
      blackB: Color(0xFF3A2012),
      whiteOutline: Color(0xFF4A2E14),
      blackOutline: Color(0xFF1E0F06),
      whiteInk: Color(0xFF4A2E14),
      blackInk: Color(0xFFD9B98A)),
  PieceSet('Royal', PieceMode.gradient,
      whiteA: Color(0xFFFBF1D0),
      whiteB: Color(0xFFD2A94A),
      blackA: Color(0xFF3A4F8F),
      blackB: Color(0xFF141F44),
      whiteOutline: Color(0xFF6B5314),
      blackOutline: Color(0xFF0A1230),
      whiteInk: Color(0xFF6B5314),
      blackInk: Color(0xFFAAB8E8)),
];

/// Interface colours.
class UiPalette {
  final String name;
  final Brightness brightness;
  final Color background, panel, text, muted, button, buttonHover, buttonDisabled, buttonText;
  const UiPalette(
    this.name,
    this.brightness, {
    required this.background,
    required this.panel,
    required this.text,
    required this.muted,
    required this.button,
    required this.buttonHover,
    required this.buttonDisabled,
    required this.buttonText,
  });
}

const uiPalettes = <UiPalette>[
  UiPalette('Dark', Brightness.dark,
      background: Color(0xFF1E1E22),
      panel: Color(0xFF2C2C34),
      text: Color(0xFFEBEBF0),
      muted: Color(0xFF9696A5),
      button: Color(0xFF465270),
      buttonHover: Color(0xFF5C6E96),
      buttonDisabled: Color(0xFF34343C),
      buttonText: Color(0xFFEBEBF0)),
  UiPalette('Midnight', Brightness.dark,
      background: Color(0xFF0B1020),
      panel: Color(0xFF141C33),
      text: Color(0xFFE3E9FA),
      muted: Color(0xFF8091B8),
      button: Color(0xFF2B4A8C),
      buttonHover: Color(0xFF3C62B3),
      buttonDisabled: Color(0xFF1B2640),
      buttonText: Color(0xFFE3E9FA)),
  UiPalette('Light', Brightness.light,
      background: Color(0xFFECEBE8),
      panel: Color(0xFFFFFFFF),
      text: Color(0xFF22252B),
      muted: Color(0xFF6A6F7A),
      button: Color(0xFFD5DAE5),
      buttonHover: Color(0xFFC0C9DB),
      buttonDisabled: Color(0xFFEBEDF1),
      buttonText: Color(0xFF22252B)),
  UiPalette('Sepia', Brightness.dark,
      background: Color(0xFF2B2118),
      panel: Color(0xFF3A2D20),
      text: Color(0xFFF1E3CC),
      muted: Color(0xFFB69F80),
      button: Color(0xFF7A5A38),
      buttonHover: Color(0xFF946F45),
      buttonDisabled: Color(0xFF4A3A2A),
      buttonText: Color(0xFFF6EBD8)),
];

/// Fixed colours (not themeable).
const lastMoveTint = Color.fromRGBO(205, 210, 106, 0.47);
const selectionTint = Color.fromRGBO(246, 246, 105, 0.59);
const checkDisc = Color.fromRGBO(220, 50, 50, 0.59);
const hintDots = Color.fromRGBO(40, 40, 40, 0.4);

/// The six user settings. Changes notify listeners and are saved with shared_preferences.
class AppSettings extends ChangeNotifier {
  /// [persist] false keeps everything in memory (tests).
  AppSettings({this.persist = true});

  final bool persist;

  int boardIndex = 0;
  int pieceIndex = 0;
  int paletteIndex = 0;
  bool coordinates = true;
  bool hints = true;
  bool animations = true;

  BoardTheme get board => boardThemes[boardIndex];
  PieceSet get pieces => pieceSets[pieceIndex];
  UiPalette get palette => uiPalettes[paletteIndex];

  static int _indexOf(List<String> names, String? name) {
    final i = name == null ? -1 : names.indexOf(name);
    return i < 0 ? 0 : i; // unknown names fall back to the default (index 0)
  }

  /// Reads the saved settings; on any failure the defaults stay.
  Future<void> load() async {
    if (!persist) return;
    try {
      final prefs = await SharedPreferences.getInstance();
      boardIndex = _indexOf([for (final b in boardThemes) b.name], prefs.getString('board'));
      pieceIndex = _indexOf([for (final p in pieceSets) p.name], prefs.getString('pieces'));
      paletteIndex = _indexOf([for (final p in uiPalettes) p.name], prefs.getString('interface'));
      coordinates = prefs.getBool('coordinates') ?? true;
      hints = prefs.getBool('hints') ?? true;
      animations = prefs.getBool('animations') ?? true;
      notifyListeners();
    } catch (_) {}
  }

  Future<void> _save() async {
    if (!persist) return;
    try {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('board', board.name);
      await prefs.setString('pieces', pieces.name);
      await prefs.setString('interface', palette.name);
      await prefs.setBool('coordinates', coordinates);
      await prefs.setBool('hints', hints);
      await prefs.setBool('animations', animations);
    } catch (_) {}
  }

  void _changed() {
    notifyListeners();
    _save();
  }

  static int _step(int i, int count, int delta) => (i + delta + count) % count;

  void cycleBoard(int delta) {
    boardIndex = _step(boardIndex, boardThemes.length, delta);
    _changed();
  }

  void cyclePieces(int delta) {
    pieceIndex = _step(pieceIndex, pieceSets.length, delta);
    _changed();
  }

  void cyclePalette(int delta) {
    paletteIndex = _step(paletteIndex, uiPalettes.length, delta);
    _changed();
  }

  void setCoordinates(bool v) {
    coordinates = v;
    _changed();
  }

  void setHints(bool v) {
    hints = v;
    _changed();
  }

  void setAnimations(bool v) {
    animations = v;
    _changed();
  }

  void reset() {
    boardIndex = pieceIndex = paletteIndex = 0;
    coordinates = hints = animations = true;
    _changed();
  }
}
