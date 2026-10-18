#pragma once

#include <SDL.h>

#include <string>

namespace platform {

// "chess_2026-10-18_1530.pgn"
std::string defaultPgnName();
// "2026.10.18" as PGN wants it.
std::string pgnDate();

struct SaveResult {
    bool saved = false;
    bool cancelled = false;
    std::string path;   // where the file went
    std::string error;  // set when saving failed
};

// Saves `text` as a PGN file. On Windows this opens the standard Save dialog; elsewhere (or if the
// dialog is unavailable) it writes into the app's private documents folder and returns that path.
SaveResult savePgn(SDL_Window* window, const std::string& text);

}  // namespace platform
