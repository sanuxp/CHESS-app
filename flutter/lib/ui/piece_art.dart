import 'dart:math' as math;
import 'dart:ui';

import '../core/types.dart';
import 'app_settings.dart';

// Pieces are vector shapes in a 100x100 box (y down), the same geometry the C++ app uses
// (cpp/app/src/PieceArt.cpp). A body is the union of its parts minus the notches; it is stroked in the
// outline colour first, then filled according to the piece set (hatch, flat or gradient).

Path _rect(double x0, double y0, double x1, double y1) => Path()..addRect(Rect.fromLTRB(x0, y0, x1, y1));
Path _circle(double cx, double cy, double r) => Path()..addOval(Rect.fromCircle(center: Offset(cx, cy), radius: r));
Path _ellipse(double cx, double cy, double rx, double ry) =>
    Path()..addOval(Rect.fromCenter(center: Offset(cx, cy), width: 2 * rx, height: 2 * ry));
Path _poly(List<double> xy) {
  final p = Path()..moveTo(xy[0], xy[1]);
  for (var i = 2; i < xy.length; i += 2) {
    p.lineTo(xy[i], xy[i + 1]);
  }
  return p..close();
}

/// A thin bar of the given width between two points (detail strokes).
Path _line(double x0, double y0, double x1, double y1, double w) {
  final dx = x1 - x0, dy = y1 - y0;
  final len = math.sqrt(dx * dx + dy * dy);
  final nx = -dy / len * w / 2, ny = dx / len * w / 2;
  return _poly([x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny]);
}

/// Closed Catmull-Rom smoothing of a polygon outline.
Path _smooth(List<double> xy, [int steps = 5]) {
  final n = xy.length ~/ 2;
  double x(int i) => xy[(i % n) * 2];
  double y(int i) => xy[(i % n) * 2 + 1];
  double f(double a, double b, double c, double d, double t) {
    final t2 = t * t, t3 = t2 * t;
    return 0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
  }

  final out = <double>[];
  for (var i = 0; i < n; i++) {
    for (var s = 0; s < steps; s++) {
      final t = s / steps;
      out.add(f(x(i - 1 + n), x(i), x(i + 1), x(i + 2), t));
      out.add(f(y(i - 1 + n), y(i), y(i + 1), y(i + 2), t));
    }
  }
  return _poly(out);
}

class _Shape {
  final List<Path> fill; // body parts
  final List<Path> cut; // notches removed from the body
  final List<Path> ink; // detail strokes drawn on the body
  const _Shape(this.fill, {this.cut = const [], this.ink = const []});
}

/// Stepped round base shared by all pieces.
List<Path> _baseFill() => [_ellipse(50, 77, 23, 3.5), _rect(30, 79, 70, 85), _ellipse(50, 79, 28, 4), _ellipse(50, 86, 31, 5)];
Path _baseInk() => _line(23, 80.5, 77, 80.5, 0.9);

_Shape _shapeFor(PieceType t) {
  switch (t) {
    case PieceType.pawn:
      return _Shape([
        _circle(50, 24, 11),
        _ellipse(50, 39, 14, 3.5),
        _smooth([44, 40, 56, 40, 58, 52, 63, 64, 70, 74, 30, 74, 37, 64, 42, 52]),
        ..._baseFill(),
      ], ink: [
        _line(36, 36.5, 64, 36.5, 0.9),
        _baseInk(),
      ]);
    case PieceType.rook:
      return _Shape([
        _rect(28, 12, 72, 28),
        _rect(34, 28, 66, 33),
        _poly([37, 33, 63, 33, 65, 62, 70, 72, 30, 72, 35, 62]),
        _rect(27, 69, 73, 75),
        ..._baseFill(),
      ], cut: [
        _rect(40, 12, 46, 21),
        _rect(54, 12, 60, 21),
      ], ink: [
        _line(28, 28, 72, 28, 0.9),
        _line(34, 33, 66, 33, 0.9),
        _line(27, 69, 73, 69, 0.9),
        _baseInk(),
      ]);
    case PieceType.bishop:
      return _Shape([
        _circle(50, 11, 5),
        _ellipse(50, 33, 13, 19),
        _ellipse(50, 53, 16, 3.5),
        _smooth([45, 54, 55, 54, 58, 63, 65, 72, 35, 72, 42, 63]),
        ..._baseFill(),
      ], ink: [
        _line(46, 38, 58, 22, 2.4),
        _line(36, 50, 64, 50, 0.9),
        _baseInk(),
      ]);
    case PieceType.knight:
      return _Shape([
        _smooth([
          28, 78, 30, 64, 37, 53, 43, 44, 35, 43, 29, 35, 31, 25, 39, 16, 43, 8, 50, 14, //
          58, 15, 68, 25, 77, 40, 81, 49, 77, 55, 69, 54, 61, 51, 58, 60, 66, 70, 70, 78,
        ]),
        ..._baseFill(),
      ], ink: [
        _circle(55, 28, 2.2),
        _circle(78, 47, 1.3),
        _line(38, 26, 46, 30, 1.2),
        _line(34, 34, 45, 38, 1.2),
        _line(35, 44, 46, 48, 1.2),
        _line(37, 53, 47, 56, 1.2),
        _baseInk(),
      ]);
    case PieceType.queen:
      return _Shape([
        _circle(22, 22, 4.5),
        _circle(36, 15, 4.5),
        _circle(50, 11, 5),
        _circle(64, 15, 4.5),
        _circle(78, 22, 4.5),
        _poly([22, 24, 32, 50, 36, 26, 44, 48, 50, 20, 56, 48, 64, 26, 68, 50, 78, 24, 72, 62, 66, 70, 34, 70, 28, 62]),
        _ellipse(50, 56, 20, 3.5),
        ..._baseFill(),
      ], ink: [
        _line(30, 53, 70, 53, 0.9),
        _baseInk(),
      ]);
    case PieceType.king:
      return _Shape([
        _rect(46.5, 3, 53.5, 24),
        _rect(40, 8, 60, 14.5),
        _ellipse(50, 37, 17, 13),
        _smooth([35, 42, 65, 42, 68, 54, 66, 66, 70, 72, 30, 72, 34, 66, 32, 54]),
        _ellipse(50, 56, 20, 3.5),
        ..._baseFill(),
      ], ink: [
        _line(32, 53, 68, 53, 0.9),
        _baseInk(),
      ]);
    default:
      return const _Shape([]);
  }
}

Path _union(List<Path> parts) {
  var result = Path();
  for (final p in parts) {
    result = Path.combine(PathOperation.union, result, p);
  }
  return result;
}

/// Everything about one silhouette that does not depend on the colours; built once per piece type.
class _Built {
  final Path body;
  final Path ink; // already limited to the body
  final Rect bounds;
  final Path hatchDark; // hatch for light bodies: heavy towards the right
  final Path hatchLight; // hatch for dark bodies: heavy towards the left
  _Built(this.body, this.ink, this.bounds, this.hatchDark, this.hatchLight);

  static const _period = 3.4; // hatch spacing in 100-unit space
  static const _rows = 100;

  static double _coverage(bool lightLines, double t) {
    t = t.clamp(0.0, 1.0).toDouble();
    if (lightLines) return 0.06 + 0.5 * math.pow(1 - t, 1.4);
    return 0.06 + 0.55 * math.pow(t, 2.4) + 0.18 * math.pow(1 - t, 5.0);
  }

  factory _Built.from(_Shape s) {
    var body = _union(s.fill);
    if (s.cut.isNotEmpty) body = Path.combine(PathOperation.difference, body, _union(s.cut));
    final ink = Path.combine(PathOperation.intersect, body, _union(s.ink));
    final bounds = body.getBounds();

    // Per-row horizontal extent of the body, from its outline, used to place the shading across it.
    final rowMin = List<double>.filled(_rows, 1e9), rowMax = List<double>.filled(_rows, -1e9);
    for (final m in body.computeMetrics()) {
      for (var d = 0.0; d <= m.length; d += 0.4) {
        final pos = m.getTangentForOffset(d)?.position;
        if (pos == null) continue;
        final r = pos.dy.floor().clamp(0, _rows - 1).toInt();
        rowMin[r] = math.min(rowMin[r], pos.dx);
        rowMax[r] = math.max(rowMax[r], pos.dx);
      }
    }

    Path hatch(bool lightLines) {
      final lines = Path();
      const steps = 12;
      for (var y0 = (bounds.top / _period).floor() * _period; y0 < bounds.bottom; y0 += _period) {
        final r = (y0 + _period / 2).floor().clamp(0, _rows - 1).toInt();
        final x0 = rowMin[r] - 1, x1 = rowMax[r] + 1;
        if (x1 - x0 < 2.5) continue;
        final bottom = <Offset>[];
        lines.moveTo(x0, y0);
        for (var i = 0; i <= steps; i++) {
          final t = i / steps;
          final x = x0 + (x1 - x0) * t;
          lines.lineTo(x, y0);
          bottom.add(Offset(x, y0 + _coverage(lightLines, t) * _period));
        }
        for (final p in bottom.reversed) {
          lines.lineTo(p.dx, p.dy);
        }
        lines.close();
      }
      return Path.combine(PathOperation.intersect, body, lines);
    }

    return _Built(body, ink, bounds, hatch(false), hatch(true));
  }
}

final Map<PieceType, _Built> _cache = {};

_Built? _built(PieceType t) {
  final cached = _cache[t];
  if (cached != null) return cached;
  final shape = _shapeFor(t);
  if (shape.fill.isEmpty) return null;
  return _cache[t] = _Built.from(shape);
}

/// Draws [piece] filling [cell] (with a small margin) in the rendering style of [set].
/// [opacity] fades the whole piece.
void paintPiece(Canvas canvas, Piece piece, Rect cell, PieceSet set, {double opacity = 1}) {
  final built = _built(piece.type);
  if (built == null || opacity <= 0) return;
  final white = piece.color == PieceColor.white;
  final outline = Paint()
    ..color = white ? set.whiteOutline : set.blackOutline
    ..style = PaintingStyle.stroke
    ..strokeWidth = 4.8
    ..strokeJoin = StrokeJoin.round;
  final a = white ? set.whiteA : set.blackA, b = white ? set.whiteB : set.blackB;

  final inset = cell.width / 16;
  final box = cell.deflate(inset);
  final scale = math.min(box.width, box.height) / 100;

  canvas.save();
  if (opacity < 1) {
    canvas.saveLayer(cell, Paint()..color = Color.fromRGBO(255, 255, 255, opacity));
  }
  canvas.translate(box.left, box.top);
  canvas.scale(scale);
  canvas.drawPath(built.body, outline);
  switch (set.mode) {
    case PieceMode.flat:
      canvas.drawPath(built.body, Paint()..color = a);
    case PieceMode.gradient:
      canvas.drawPath(
        built.body,
        Paint()..shader = Gradient.linear(Offset(built.bounds.left, 0), Offset(built.bounds.right, 0), [a, b]),
      );
    case PieceMode.hatch:
      canvas.drawPath(built.body, Paint()..color = a);
      canvas.drawPath(white ? built.hatchDark : built.hatchLight, Paint()..color = b);
  }
  canvas.drawPath(built.ink, Paint()..color = white ? set.whiteInk : set.blackInk);
  if (opacity < 1) canvas.restore();
  canvas.restore();
}
