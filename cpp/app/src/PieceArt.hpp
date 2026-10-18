#pragma once

#include <SDL.h>

#include <array>

#include "Theme.hpp"
#include "chess/Types.hpp"

// Draws the twelve chess pieces procedurally (no image files) into textures once at start-up.
class PieceArt {
    public:
        ~PieceArt();
        // (Re)draws all twelve textures in the given piece set.
    bool init(SDL_Renderer* renderer, const PieceSet& set);
        SDL_Texture* get(chess::Piece p) const;

    private:
    void release();
        std::array<SDL_Texture*, 12> textures_{};
};
