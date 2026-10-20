import 'dart:async';

import 'package:chess_app/review/uci_engine.dart';
import 'package:chess_app/review/uci_parser.dart';

/// Replies like an engine from a table of canned search results.
class FakeTransport implements UciTransport {
  final _out = StreamController<String>.broadcast();
  final List<String> sent = [];
  final Map<String, List<String>> script; // FEN -> info lines (bestmove is added)
  String _fen = '';

  FakeTransport(this.script);

  @override
  Stream<String> get lines => _out.stream;

  @override
  void send(String line) {
    sent.add(line);
    Future(() {
      if (line == 'uci') {
        _out
          ..add('id name FakeFish 1')
          ..add('uciok');
      } else if (line == 'isready') {
        _out.add('readyok');
      } else if (line.startsWith('position fen ')) {
        _fen = line.substring('position fen '.length);
      } else if (line.startsWith('go ')) {
        final infos = script[_fen] ?? const [];
        for (final i in infos) {
          _out.add(i);
        }
        _out.add('info depth 1 currmove e2e4 currmovenumber 1');
        _out.add(infos.isEmpty ? 'bestmove (none)' : 'bestmove ${parseInfo(infos.first)!.pv.first} ponder a2a3');
      }
    });
  }

  @override
  Future<void> close() async => _out.close();
}

/// An engine that never says anything.
class SilentTransport implements UciTransport {
  @override
  Stream<String> get lines => const Stream.empty();
  @override
  void send(String line) {}
  @override
  Future<void> close() async {}
}
