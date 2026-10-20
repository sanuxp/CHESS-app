import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'uci_engine.dart';

String engineSearchHint() =>
    'Stockfish was not found. Run tools\\get_stockfish.ps1 (it installs engine\\stockfish.exe), '
    'put a Stockfish program in an "engine" folder next to the app, or choose one in the review settings.';

String get _exeName => Platform.isWindows ? 'stockfish.exe' : 'stockfish';

/// Looks for Stockfish: the user's chosen path, then `engine/` next to the app and in its parent folders
/// (so a development build finds the repository's `engine/`), then the `CHESS_STOCKFISH` variable, then PATH.
Future<String?> findEnginePath({String? customPath}) async {
  if (customPath != null && customPath.isNotEmpty && await File(customPath).exists()) return customPath;

  var dir = File(Platform.resolvedExecutable).parent;
  for (var i = 0; i < 10; i++) {
    final candidate = File('${dir.path}${Platform.pathSeparator}engine${Platform.pathSeparator}$_exeName');
    if (await candidate.exists()) return candidate.path;
    final parent = dir.parent;
    if (parent.path == dir.path) break;
    dir = parent;
  }
  // `flutter test` / `flutter run` start from the package folder, one level below the repository root.
  for (final rel in ['engine', '../engine']) {
    final candidate = File('$rel${Platform.pathSeparator}$_exeName');
    if (await candidate.exists()) return candidate.absolute.path;
  }

  final env = Platform.environment['CHESS_STOCKFISH'];
  if (env != null && await File(env).exists()) return env;

  final finder = Platform.isWindows ? 'where' : 'which';
  try {
    final r = await Process.run(finder, [Platform.isWindows ? 'stockfish.exe' : 'stockfish']);
    if (r.exitCode == 0) {
      final first = (r.stdout as String).split(RegExp(r'\r?\n')).first.trim();
      if (first.isNotEmpty) return first;
    }
  } catch (_) {}
  return null;
}

/// One search thread. More threads should help on paper, but multi-threaded Stockfish was about 30x slower than
/// one thread on the machine this was developed on, so it is opt-in (review settings).
int defaultEngineThreads() => 1;

class ProcessTransport implements UciTransport {
  final Process _process;
  final StreamController<String> _lines = StreamController<String>.broadcast();
  late final StreamSubscription<String> _sub;

  ProcessTransport._(this._process) {
    _sub = _process.stdout.transform(utf8.decoder).transform(const LineSplitter()).listen(_lines.add, onDone: _lines.close);
    _process.stderr.drain<void>();
  }

  static Future<ProcessTransport> start(String path) async => ProcessTransport._(await Process.start(path, const []));

  @override
  Stream<String> get lines => _lines.stream;

  @override
  void send(String line) => _process.stdin.writeln(line);

  @override
  Future<void> close() async {
    await _sub.cancel();
    _process.kill();
  }
}

/// Starts Stockfish as a child process; null when it cannot be found.
Future<UciTransport?> openEngineTransport({String? customPath}) async {
  final path = await findEnginePath(customPath: customPath);
  if (path == null) return null;
  return ProcessTransport.start(path);
}
