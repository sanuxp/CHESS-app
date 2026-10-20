import 'dart:async';

import 'classify.dart';
import 'uci_parser.dart';

/// A line-based connection to a UCI engine: a child process on desktop, a native library on phones,
/// a script in tests.
abstract class UciTransport {
  /// Lines the engine prints (without the newline). Must be a single-subscription or broadcast stream.
  Stream<String> get lines;
  void send(String line);
  Future<void> close();
}

class EngineException implements Exception {
  final String message;
  const EngineException(this.message);
  @override
  String toString() => message;
}

/// Talks UCI over a [UciTransport]: handshake, options and fixed-depth multi-line analysis.
class UciEngine {
  final UciTransport _transport;
  StreamSubscription<String>? _subscription;
  void Function(String)? _onLine;
  int _multiPv = 1;
  bool _busy = false;
  String name = '';

  UciEngine(this._transport);

  /// Performs the `uci` / `isready` handshake.
  Future<void> start({Duration timeout = const Duration(seconds: 20)}) async {
    _subscription = _transport.lines.listen((line) {
      if (line.startsWith('id name ')) name = line.substring(8).trim();
      _onLine?.call(line);
    });
    await _ask('uci', (l) => l.trim() == 'uciok', timeout);
    await _ask('isready', (l) => l.trim() == 'readyok', timeout);
  }

  Future<void> _ask(String command, bool Function(String) done, Duration timeout) {
    final completer = Completer<void>();
    _onLine = (l) {
      if (!completer.isCompleted && done(l)) completer.complete();
    };
    _transport.send(command);
    return completer.future.timeout(timeout, onTimeout: () {
      throw EngineException('The engine did not answer "$command" within ${timeout.inSeconds} s.');
    }).whenComplete(() => _onLine = null);
  }

  /// Best lines for [fen], best first, scores from the side to move's view. Empty if there is no legal move.
  Future<List<EngineLine>> analyze(
    String fen, {
    int depth = 14,
    int multiPv = 3,
    Duration timeout = const Duration(minutes: 2),
  }) async {
    if (_busy) throw const EngineException('The engine is already analysing a position.');
    _busy = true;
    try {
      if (multiPv != _multiPv) {
        setOption('MultiPV', multiPv);
        _multiPv = multiPv;
      }
      final best = <int, UciInfo>{}; // multipv index -> deepest info seen
      final done = Completer<String>();
      _onLine = (l) {
        final info = parseInfo(l);
        if (info != null) {
          final old = best[info.multipv];
          if (old == null || info.depth >= old.depth) best[info.multipv] = info;
          return;
        }
        final move = parseBestMove(l);
        if (move != null && !done.isCompleted) done.complete(move);
      };
      _transport.send('position fen $fen');
      _transport.send('go depth $depth');
      final bestMove = await done.future.timeout(timeout, onTimeout: () {
        _transport.send('stop');
        throw EngineException('The engine took longer than ${timeout.inSeconds} s for one position.');
      });
      if (bestMove == '(none)') return const [];
      final keys = best.keys.toList()..sort();
      return [for (final k in keys) EngineLine(best[k]!.pv.first, best[k]!.score, best[k]!.pv)];
    } finally {
      _onLine = null;
      _busy = false;
    }
  }

  /// Sets an engine option such as Threads or Hash (no answer is expected).
  void setOption(String name, Object value) => _transport.send('setoption name $name value $value');

  /// Asks a running search to stop (its `bestmove` still arrives).
  void stop() => _transport.send('stop');

  Future<void> dispose() async {
    _transport.send('quit');
    await _subscription?.cancel();
    await _transport.close();
  }
}
