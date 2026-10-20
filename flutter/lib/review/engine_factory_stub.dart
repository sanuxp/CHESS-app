import 'uci_engine.dart';

/// Where the engine program is expected, for messages.
String engineSearchHint() => 'No engine is available on this platform yet.';

/// The Stockfish program we would run, or null.
Future<String?> findEnginePath({String? customPath}) async => null;

/// Search threads to give the engine.
int defaultEngineThreads() => 1;

/// Opens a connection to an engine, or null when none can be started here.
Future<UciTransport?> openEngineTransport({String? customPath}) async => null;
