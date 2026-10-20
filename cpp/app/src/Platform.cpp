#define NOMINMAX
#include "Platform.hpp"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <iterator>

#ifdef _WIN32
#include <SDL_syswm.h>
#include <windows.h>
#include <commdlg.h>
#endif

namespace platform {

namespace {

std::string formatNow(const char* fmt) {
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), fmt, std::localtime(&t));
    return buf;
}

bool writeFile(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

}  // namespace

OpenResult openPgn(SDL_Window* window) {
    OpenResult result;
#ifdef _WIN32
    char file[MAX_PATH] = {0};
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    if (window && SDL_GetWindowWMInfo(window, &wm)) ofn.hwndOwner = wm.info.win.window;
    ofn.lpstrFilter = "PGN game (*.pgn)\0*.pgn\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Open a game (PGN)";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetOpenFileNameA(&ofn)) return result;  // cancelled
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        result.error = std::string("could not read ") + file;
        return result;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    result.opened = true;
    result.text = std::move(content);
    std::string path = file;
    size_t slash = path.find_last_of("/\\");
    result.name = slash == std::string::npos ? path : path.substr(slash + 1);
#else
    (void)window;
    result.supported = false;
#endif
    return result;
}

std::string defaultPgnName() { return "chess_" + formatNow("%Y-%m-%d_%H%M") + ".pgn"; }
std::string pgnDate() { return formatNow("%Y.%m.%d"); }

SaveResult savePgn(SDL_Window* window, const std::string& text) {
    SaveResult result;
    std::string name = defaultPgnName();

#ifdef _WIN32
    char file[MAX_PATH] = {0};
    std::snprintf(file, sizeof(file), "%s", name.c_str());

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    if (window && SDL_GetWindowWMInfo(window, &wm)) ofn.hwndOwner = wm.info.win.window;
    ofn.lpstrFilter = "PGN game (*.pgn)\0*.pgn\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "pgn";
    ofn.lpstrTitle = "Save game as PGN";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetSaveFileNameA(&ofn)) {
        result.cancelled = true;
        return result;
    }
    result.path = file;
#else
    (void)window;
    char* pref = SDL_GetPrefPath("ChessLocal", "Chess");
    if (!pref) {
        result.error = "no writable folder";
        return result;
    }
    result.path = std::string(pref) + name;
    SDL_free(pref);
#endif

    if (writeFile(result.path, text)) result.saved = true;
    else result.error = "could not write " + result.path;
    return result;
}

}  // namespace platform
