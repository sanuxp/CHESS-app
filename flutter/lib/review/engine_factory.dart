// Chooses how to reach an engine on this platform. Desktop starts a child process (engine_factory_io.dart);
// platforms without one get a stub that reports "no engine" until a native implementation is added.
export 'engine_factory_stub.dart' if (dart.library.io) 'engine_factory_io.dart';
