#include "Theme.hpp"

#include <cstring>
#include <fstream>
#include <sstream>

namespace {

constexpr SDL_Color hex(unsigned v) {
    return SDL_Color{static_cast<Uint8>(v >> 16), static_cast<Uint8>(v >> 8), static_cast<Uint8>(v), 255};
}

std::string settingsPath() {
    std::string path;
    if (char* dir = SDL_GetPrefPath("Chess", "Chess")) {
        path = std::string(dir) + "settings.ini";
        SDL_free(dir);
    }
    return path;
}

template <typename Arr>
int indexOf(const Arr& arr, const std::string& name, int fallback) {
    for (size_t i = 0; i < arr.size(); ++i) {
        if (name == arr[i].name) return static_cast<int>(i);
    }
    return fallback;
}

}  // namespace

const std::array<BoardTheme, kBoardThemeCount>& boardThemes() {
    static const std::array<BoardTheme, kBoardThemeCount> t{{
        {"Brown", hex(0xF0D9B5), hex(0xB58863)},
        {"Green", hex(0xEEEED2), hex(0x769656)},
        {"Blue", hex(0xDEE3E6), hex(0x8CA2AD)},
        {"Purple", hex(0xEFEFEF), hex(0x8C77B5)},
        {"Slate", hex(0xDCDCDC), hex(0x828282)},
        {"Ice", hex(0xE8F0F7), hex(0x7898BA)},
        {"Coral", hex(0xF5E1D7), hex(0xC46E5F)},
        {"Walnut", hex(0xC8AA82), hex(0x6E4B32)},
    }};
    return t;
}

const std::array<PieceSet, kPieceSetCount>& pieceSets() {
    static const std::array<PieceSet, kPieceSetCount> t{{
        {"Engraved", PieceMode::Hatch, hex(0xFAF8F2), hex(0x1E1C1C), hex(0x161414), hex(0x1A1A1E), hex(0xD6D6DC), hex(0x08080A)},
        {"Classic", PieceMode::Flat, hex(0xF8F6EE), hex(0xF8F6EE), hex(0x26221F), hex(0x4A4C56), hex(0x4A4C56), hex(0x0E0E12)},
        {"Shaded", PieceMode::Gradient, hex(0xFFFFFF), hex(0xB9B4A8), hex(0x26221F), hex(0x5A5C68), hex(0x101014), hex(0x08080A)},
        {"Wood", PieceMode::Gradient, hex(0xEBC98F), hex(0xB98A4E), hex(0x4A2E14), hex(0x7A4A2A), hex(0x3A2012), hex(0x1E0F06)},
        {"Royal", PieceMode::Gradient, hex(0xFBF1D0), hex(0xD2A94A), hex(0x6B5314), hex(0x3A4F8F), hex(0x141F44), hex(0x0A1230)},
    }};
    return t;
}

const std::array<UiTheme, kUiThemeCount>& uiThemes() {
    static const std::array<UiTheme, kUiThemeCount> t{{
        {"Dark", hex(0x1E1E22), hex(0x2C2C34), hex(0xEBEBF0), hex(0x9696A5), hex(0x465270), hex(0x5C6E96), hex(0x34343C), hex(0xEBEBF0), hex(0x6E6E7D)},
        {"Midnight", hex(0x0B1020), hex(0x141C33), hex(0xE3E9FA), hex(0x8091B8), hex(0x2B4A8C), hex(0x3C62B3), hex(0x1B2640), hex(0xE3E9FA), hex(0x5C6B8F)},
        {"Light", hex(0xECEBE8), hex(0xFFFFFF), hex(0x22252B), hex(0x6A6F7A), hex(0xD5DAE5), hex(0xC0C9DB), hex(0xEBEDF1), hex(0x22252B), hex(0xA0A5AF)},
        {"Sepia", hex(0x2B2118), hex(0x3A2D20), hex(0xF1E3CC), hex(0xB69F80), hex(0x7A5A38), hex(0x946F45), hex(0x4A3A2A), hex(0xF6EBD8), hex(0x8A7358)},
    }};
    return t;
}

Settings Settings::load() {
    Settings s;
    std::string path = settingsPath();
    if (path.empty()) return s;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), val = line.substr(eq + 1);
        while (!val.empty() && (val.back() == '\r' || val.back() == ' ')) val.pop_back();
        if (key == "board") s.board = indexOf(boardThemes(), val, 0);
        else if (key == "pieces") s.pieces = indexOf(pieceSets(), val, 0);
        else if (key == "interface") s.ui = indexOf(uiThemes(), val, 0);
        else if (key == "coordinates") s.coordinates = val != "off";
        else if (key == "hints") s.hints = val != "off";
        else if (key == "animations") s.animations = val != "off";
    }
    return s;
}

void Settings::save() const {
    std::string path = settingsPath();
    if (path.empty()) return;
    std::ofstream out(path);
    out << "board=" << boardTheme().name << "\n"
        << "pieces=" << pieceSet().name << "\n"
        << "interface=" << uiTheme().name << "\n"
        << "coordinates=" << (coordinates ? "on" : "off") << "\n"
        << "hints=" << (hints ? "on" : "off") << "\n"
        << "animations=" << (animations ? "on" : "off") << "\n";
}
