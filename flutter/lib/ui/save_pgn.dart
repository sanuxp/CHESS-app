import 'dart:convert';

import 'package:file_selector/file_selector.dart';
import 'package:flutter/foundation.dart';
import 'package:share_plus/share_plus.dart';

/// "chess_2026-10-18_1530.pgn"
String defaultPgnName([DateTime? now]) {
  final t = now ?? DateTime.now();
  String two(int n) => n.toString().padLeft(2, '0');
  return 'chess_${t.year}-${two(t.month)}-${two(t.day)}_${two(t.hour)}${two(t.minute)}.pgn';
}

/// "2026.10.18" as PGN wants it.
String pgnDate([DateTime? now]) {
  final t = now ?? DateTime.now();
  String two(int n) => n.toString().padLeft(2, '0');
  return '${t.year}.${two(t.month)}.${two(t.day)}';
}

/// Saves [pgn] the way each platform does it: a save dialog on desktop, a download in the browser,
/// the share sheet on Android and iOS. Returns a short message for the toast.
Future<String> savePgnFile(String pgn) async {
  final name = defaultPgnName();
  final file = XFile.fromData(Uint8List.fromList(utf8.encode(pgn)), mimeType: 'application/x-chess-pgn', name: name);
  try {
    if (kIsWeb) {
      await file.saveTo(name);
      return 'DOWNLOADED $name';
    }
    switch (defaultTargetPlatform) {
      case TargetPlatform.android:
      case TargetPlatform.iOS:
        await SharePlus.instance.share(ShareParams(files: [file], fileNameOverrides: [name], subject: name));
        return 'SHARED $name';
      default:
        final where = await getSaveLocation(
          suggestedName: name,
          acceptedTypeGroups: const [XTypeGroup(label: 'PGN game', extensions: ['pgn'])],
        );
        if (where == null) return 'SAVE CANCELLED';
        await file.saveTo(where.path);
        final slash = where.path.lastIndexOf(RegExp(r'[\\/]'));
        return 'SAVED ${slash < 0 ? where.path : where.path.substring(slash + 1)}';
    }
  } catch (e) {
    return 'COULD NOT SAVE: $e';
  }
}
