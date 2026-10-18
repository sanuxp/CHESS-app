#include "PieceArt.hpp"

#include "Theme.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using chess::Color;
using chess::PieceType;

namespace {

constexpr int kHi = 512;   // supersampled working resolution
constexpr int kOut = 256;  // texture size

struct Pt {
    float x, y;
};

// Shapes live in a 100x100 coordinate space, y pointing down.
struct Prim {
    enum Kind { Circle, Ellipse, Rect, Poly, Line } kind;
    float a = 0, b = 0, c = 0, d = 0, e = 0;  // circle: cx,cy,r | ellipse: cx,cy,rx,ry | rect: x0,y0,x1,y1 | line: x0,y0,x1,y1,width
    std::vector<Pt> pts;
};

Prim circle(float cx, float cy, float r) { return Prim{Prim::Circle, cx, cy, r, 0, 0, {}}; }
Prim ellipse(float cx, float cy, float rx, float ry) { return Prim{Prim::Ellipse, cx, cy, rx, ry, 0, {}}; }
Prim rect(float x0, float y0, float x1, float y1) { return Prim{Prim::Rect, x0, y0, x1, y1, 0, {}}; }
Prim poly(std::vector<Pt> pts) { return Prim{Prim::Poly, 0, 0, 0, 0, 0, std::move(pts)}; }
Prim line(float x0, float y0, float x1, float y1, float w) { return Prim{Prim::Line, x0, y0, x1, y1, w, {}}; }

struct Shape {
    std::vector<Prim> fill;  // body
    std::vector<Prim> cut;   // removed from body (notches)
    std::vector<Prim> ink;   // detail strokes drawn on the body
};

// Closed Catmull-Rom smoothing of a polygon outline.
std::vector<Pt> smooth(const std::vector<Pt>& p, int steps = 6) {
    std::vector<Pt> out;
    const int n = static_cast<int>(p.size());
    for (int i = 0; i < n; ++i) {
        const Pt &p0 = p[(i + n - 1) % n], &p1 = p[i], &p2 = p[(i + 1) % n], &p3 = p[(i + 2) % n];
        for (int s = 0; s < steps; ++s) {
            float t = static_cast<float>(s) / steps, t2 = t * t, t3 = t2 * t;
            auto f = [&](float a, float b, float c, float d) {
                return 0.5f * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
            };
            out.push_back({f(p0.x, p1.x, p2.x, p3.x), f(p0.y, p1.y, p2.y, p3.y)});
        }
    }
    return out;
}

// Stepped round base shared by all pieces.
void addBase(Shape& s) {
    s.fill.push_back(ellipse(50, 77, 23, 3.5f));
    s.fill.push_back(rect(30, 79, 70, 85));
    s.fill.push_back(ellipse(50, 79, 28, 4));
    s.fill.push_back(ellipse(50, 86, 31, 5));
    s.ink.push_back(line(23, 80.5f, 77, 80.5f, 0.9f));
}

Shape shapeFor(PieceType t) {
    Shape s;
    switch (t) {
        case PieceType::Pawn:
            s.fill = {circle(50, 24, 11), ellipse(50, 39, 14, 3.5f),
                      poly(smooth({{44, 40}, {56, 40}, {58, 52}, {63, 64}, {70, 74}, {30, 74}, {37, 64}, {42, 52}}, 5))};
            s.ink = {line(36, 36.5f, 64, 36.5f, 0.9f)};
            addBase(s);
            break;
        case PieceType::Rook:
            s.fill = {rect(28, 12, 72, 28), rect(34, 28, 66, 33), poly({{37, 33}, {63, 33}, {65, 62}, {70, 72}, {30, 72}, {35, 62}}),
                      rect(27, 69, 73, 75)};
            s.cut = {rect(40, 12, 46, 21), rect(54, 12, 60, 21)};
            s.ink = {line(28, 28, 72, 28, 0.9f), line(34, 33, 66, 33, 0.9f), line(27, 69, 73, 69, 0.9f)};
            addBase(s);
            break;
        case PieceType::Bishop:
            s.fill = {circle(50, 11, 5), ellipse(50, 33, 13, 19), ellipse(50, 53, 16, 3.5f),
                      poly(smooth({{45, 54}, {55, 54}, {58, 63}, {65, 72}, {35, 72}, {42, 63}}, 5))};
            s.ink = {line(46, 38, 58, 22, 2.4f), line(36, 50, 64, 50, 0.9f)};
            addBase(s);
            break;
        case PieceType::Knight:
            s.fill = {poly(smooth({{28, 78}, {30, 64}, {37, 53}, {43, 44}, {35, 43}, {29, 35}, {31, 25}, {39, 16}, {43, 8},
                                   {50, 14}, {58, 15}, {68, 25}, {77, 40}, {81, 49}, {77, 55}, {69, 54}, {61, 51}, {58, 60},
                                   {66, 70}, {70, 78}},
                                  5))};
            s.ink = {circle(55, 28, 2.2f),    circle(78, 47, 1.3f),        line(38, 26, 46, 30, 1.2f), line(34, 34, 45, 38, 1.2f),
                     line(35, 44, 46, 48, 1.2f), line(37, 53, 47, 56, 1.2f)};
            addBase(s);
            break;
        case PieceType::Queen:
            s.fill = {circle(22, 22, 4.5f), circle(36, 15, 4.5f), circle(50, 11, 5), circle(64, 15, 4.5f), circle(78, 22, 4.5f),
                      poly({{22, 24}, {32, 50}, {36, 26}, {44, 48}, {50, 20}, {56, 48}, {64, 26}, {68, 50}, {78, 24}, {72, 62},
                            {66, 70}, {34, 70}, {28, 62}}),
                      ellipse(50, 56, 20, 3.5f)};
            s.ink = {line(30, 53, 70, 53, 0.9f)};
            addBase(s);
            break;
        case PieceType::King:
            s.fill = {rect(46.5f, 3, 53.5f, 24), rect(40, 8, 60, 14.5f), ellipse(50, 37, 17, 13),
                      poly(smooth({{35, 42}, {65, 42}, {68, 54}, {66, 66}, {70, 72}, {30, 72}, {34, 66}, {32, 54}}, 5)),
                      ellipse(50, 56, 20, 3.5f)};
            s.ink = {line(32, 53, 68, 53, 0.9f)};
            addBase(s);
            break;
        default: break;
    }
    return s;
}

bool pointInPoly(const std::vector<Pt>& p, float x, float y) {
    bool inside = false;
    for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) {
        if (((p[i].y > y) != (p[j].y > y)) && (x < (p[j].x - p[i].x) * (y - p[i].y) / (p[j].y - p[i].y) + p[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

float distToSegment(float px, float py, float x0, float y0, float x1, float y1) {
    float dx = x1 - x0, dy = y1 - y0;
    float len2 = dx * dx + dy * dy;
    float t = len2 > 0 ? std::clamp(((px - x0) * dx + (py - y0) * dy) / len2, 0.0f, 1.0f) : 0.0f;
    float cx = x0 + t * dx, cy = y0 + t * dy;
    return std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
}

bool inside(const Prim& p, float x, float y) {
    switch (p.kind) {
        case Prim::Circle: return (x - p.a) * (x - p.a) + (y - p.b) * (y - p.b) <= p.c * p.c;
        case Prim::Ellipse: {
            float nx = (x - p.a) / p.c, ny = (y - p.b) / p.d;
            return nx * nx + ny * ny <= 1;
        }
        case Prim::Rect: return x >= p.a && x <= p.c && y >= p.b && y <= p.d;
        case Prim::Poly: return pointInPoly(p.pts, x, y);
        case Prim::Line: return distToSegment(x, y, p.a, p.b, p.c, p.d) <= p.e / 2;
    }
    return false;
}

void bounds(const Prim& p, float& x0, float& y0, float& x1, float& y1) {
    switch (p.kind) {
        case Prim::Circle: x0 = p.a - p.c; x1 = p.a + p.c; y0 = p.b - p.c; y1 = p.b + p.c; break;
        case Prim::Ellipse: x0 = p.a - p.c; x1 = p.a + p.c; y0 = p.b - p.d; y1 = p.b + p.d; break;
        case Prim::Rect: x0 = p.a; y0 = p.b; x1 = p.c; y1 = p.d; break;
        case Prim::Line:
            x0 = std::min(p.a, p.c) - p.e; x1 = std::max(p.a, p.c) + p.e;
            y0 = std::min(p.b, p.d) - p.e; y1 = std::max(p.b, p.d) + p.e;
            break;
        case Prim::Poly:
            x0 = y0 = 1e9f; x1 = y1 = -1e9f;
            for (const Pt& q : p.pts) {
                x0 = std::min(x0, q.x); x1 = std::max(x1, q.x);
                y0 = std::min(y0, q.y); y1 = std::max(y1, q.y);
            }
            break;
    }
}

void paint(std::vector<uint8_t>& mask, const Prim& p, uint8_t value) {
    const float k = kHi / 100.0f;
    float ux0, uy0, ux1, uy1;
    bounds(p, ux0, uy0, ux1, uy1);
    int x0 = std::max(0, static_cast<int>(std::floor(ux0 * k)) - 1), x1 = std::min(kHi - 1, static_cast<int>(std::ceil(ux1 * k)) + 1);
    int y0 = std::max(0, static_cast<int>(std::floor(uy0 * k)) - 1), y1 = std::min(kHi - 1, static_cast<int>(std::ceil(uy1 * k)) + 1);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (inside(p, (x + 0.5f) / k, (y + 0.5f) / k)) mask[y * kHi + x] = value;
        }
    }
}

struct Rgb {
    uint8_t r, g, b;
};

// How one colour side of a piece set is painted. Hatch: engraved horizontal lines whose weight follows a
// light source from the upper left. Gradient: base (left, lit) fading to line (right, shadow).
struct Style {
    PieceMode mode;
    Rgb base, line, outline, ink;
    bool lightLines;  // Hatch only: bright lines on a dark body
};

Rgb toRgb(SDL_Color c) { return Rgb{c.r, c.g, c.b}; }

Style styleFor(const PieceSet& set, bool white) {
    Rgb base = toRgb(white ? set.whiteBase : set.blackBase);
    Rgb line = toRgb(white ? set.whiteLine : set.blackLine);
    Rgb outline = toRgb(white ? set.whiteOutline : set.blackOutline);
    // Detail strokes (eye, collar rings): outline colour on light bodies, line colour on dark bodies.
    bool dark = static_cast<int>(base.r) + base.g + base.b < 3 * 128;
    Rgb ink = set.mode == PieceMode::Hatch ? (dark ? line : outline) : (dark ? Rgb{210, 210, 215} : outline);
    return Style{set.mode, base, line, outline, ink, set.mode == PieceMode::Hatch && dark};
}

float hatchCoverage(const Style& st, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    if (st.lightLines) return 0.06f + 0.5f * std::pow(1 - t, 1.4f);
    return 0.06f + 0.55f * std::pow(t, 2.4f) + 0.18f * std::pow(1 - t, 5.0f);
}

Rgb mix(Rgb a, Rgb b, float t) {
    return Rgb{static_cast<uint8_t>(a.r + (b.r - a.r) * t), static_cast<uint8_t>(a.g + (b.g - a.g) * t),
               static_cast<uint8_t>(a.b + (b.b - a.b) * t)};
}

SDL_Texture* renderPiece(SDL_Renderer* renderer, const Shape& shape, const Style& style) {
    std::vector<uint8_t> fill(kHi * kHi, 0), ink(kHi * kHi, 0), outline(kHi * kHi, 0);
    for (const Prim& p : shape.fill) paint(fill, p, 1);
    for (const Prim& p : shape.cut) paint(fill, p, 0);
    for (const Prim& p : shape.ink) paint(ink, p, 1);

    // Per-row horizontal extent of the body, used to place the shading across it.
    std::vector<int> rowMin(kHi, kHi), rowMax(kHi, -1);
    for (int y = 0; y < kHi; ++y) {
        for (int x = 0; x < kHi; ++x) {
            if (!fill[y * kHi + x]) continue;
            rowMin[y] = std::min(rowMin[y], x);
            rowMax[y] = std::max(rowMax[y], x);
        }
    }
    const float kPeriod = 3.4f;  // hatch spacing in 100-unit space
    auto bodyColor = [&](int x, int y) {
        float w = static_cast<float>(rowMax[y] - rowMin[y] + 1);
        float t = (x - rowMin[y]) / w;
        if (style.mode == PieceMode::Flat) return style.base;
        if (style.mode == PieceMode::Gradient) return mix(style.base, style.line, std::clamp(t, 0.0f, 1.0f));
        float phase = std::fmod((y + 0.5f) * 100.0f / kHi, kPeriod) / kPeriod;
        return phase < hatchCoverage(style, t) ? style.line : style.base;
    };

    // Outline: every empty pixel within `radius` of the body.
    const int radius = static_cast<int>(kHi * 0.024f);
    for (int y = 0; y < kHi; ++y) {
        for (int x = 0; x < kHi; ++x) {
            if (!fill[y * kHi + x]) continue;
            bool edge = x == 0 || y == 0 || x == kHi - 1 || y == kHi - 1 || !fill[y * kHi + x - 1] ||
                        !fill[y * kHi + x + 1] || !fill[(y - 1) * kHi + x] || !fill[(y + 1) * kHi + x];
            if (!edge) continue;
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (dx * dx + dy * dy > radius * radius) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= kHi || ny >= kHi) continue;
                    outline[ny * kHi + nx] = 1;
                }
            }
        }
    }

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, kOut, kOut, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) return nullptr;
    const int ss = kHi / kOut;
    auto* pixels = static_cast<uint32_t*>(surf->pixels);
    for (int y = 0; y < kOut; ++y) {
        for (int x = 0; x < kOut; ++x) {
            int a = 0, r = 0, g = 0, b = 0;
            for (int sy = 0; sy < ss; ++sy) {
                for (int sx = 0; sx < ss; ++sx) {
                    int i = (y * ss + sy) * kHi + (x * ss + sx);
                    Rgb c;
                    if (fill[i]) c = ink[i] ? style.ink : bodyColor(x * ss + sx, y * ss + sy);
                    else if (outline[i]) c = style.outline;
                    else continue;
                    a += 255;
                    r += c.r * 255;
                    g += c.g * 255;
                    b += c.b * 255;
                }
            }
            uint32_t px = 0;
            if (a > 0) {
                int alpha = a / (ss * ss);
                px = (static_cast<uint32_t>(alpha) << 24) | (static_cast<uint32_t>(r / a) << 16) |
                     (static_cast<uint32_t>(g / a) << 8) | static_cast<uint32_t>(b / a);
            }
            pixels[y * (surf->pitch / 4) + x] = px;
        }
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

}  // namespace

PieceArt::~PieceArt() { release(); }

void PieceArt::release() {
    for (SDL_Texture*& t : textures_) {
        if (t) SDL_DestroyTexture(t);
        t = nullptr;
    }
}

bool PieceArt::init(SDL_Renderer* renderer, const PieceSet& set) {
    release();
    const Style white = styleFor(set, true), black = styleFor(set, false);
    for (int t = 1; t <= 6; ++t) {
        Shape shape = shapeFor(static_cast<PieceType>(t));
        textures_[t - 1] = renderPiece(renderer, shape, white);
        textures_[6 + t - 1] = renderPiece(renderer, shape, black);
    }
    return std::all_of(textures_.begin(), textures_.end(), [](SDL_Texture* t) { return t != nullptr; });
}

SDL_Texture* PieceArt::get(chess::Piece p) const {
    if (p.isNone()) return nullptr;
    int index = (p.color == Color::White ? 0 : 6) + static_cast<int>(p.type) - 1;
    return textures_[index];
}
