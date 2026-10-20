#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "App.hpp"

int main(int argc, char** argv) {
    App app;

    // Hidden developer mode: render one frame to a BMP.
    //   chess --screenshot out.bmp [--size 1000x720] [--moves "e2e4 e7e5"] [--select e2]
    //         [--animate 0.5] [--undo] [--promotion] [--flip]
    //         [--board N] [--pieces N] [--ui N] [--settings]
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--selftest") == 0) {
            return app.init() && app.selfTest() ? 0 : 1;
        }
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            ShotOptions opt;
            opt.outPath = argv[++i];
            for (++i; i < argc; ++i) {
                if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
                    std::sscanf(argv[++i], "%dx%d", &opt.width, &opt.height);
                } else if (std::strcmp(argv[i], "--moves") == 0 && i + 1 < argc) {
                    opt.moves = argv[++i];
                } else if (std::strcmp(argv[i], "--select") == 0 && i + 1 < argc) {
                    opt.select = argv[++i];
                } else if (std::strcmp(argv[i], "--animate") == 0 && i + 1 < argc) {
                    opt.animate = std::atof(argv[++i]);
                } else if (std::strcmp(argv[i], "--undo") == 0) {
                    opt.undoLast = true;
                } else if (std::strcmp(argv[i], "--promotion") == 0) {
                    opt.promotion = true;
                } else if (std::strcmp(argv[i], "--board") == 0 && i + 1 < argc) {
                    opt.board = std::atoi(argv[++i]);
                } else if (std::strcmp(argv[i], "--pieces") == 0 && i + 1 < argc) {
                    opt.pieces = std::atoi(argv[++i]);
                } else if (std::strcmp(argv[i], "--ui") == 0 && i + 1 < argc) {
                    opt.ui = std::atoi(argv[++i]);
                } else if (std::strcmp(argv[i], "--settings") == 0) {
                    opt.settings = true;
                } else if (std::strcmp(argv[i], "--review") == 0 && i + 1 < argc) {
                    opt.reviewSan = argv[++i];
                } else if (std::strcmp(argv[i], "--review-setup") == 0) {
                    opt.reviewSetup = true;
                } else if (std::strcmp(argv[i], "--depth") == 0 && i + 1 < argc) {
                    opt.reviewDepth = std::atoi(argv[++i]);
                } else if (std::strcmp(argv[i], "--cursor") == 0 && i + 1 < argc) {
                    opt.cursor = std::atoi(argv[++i]);
                } else if (std::strcmp(argv[i], "--flip") == 0) {
                    opt.flip = true;
                }
            }
            return app.screenshot(opt) ? 0 : 1;
        }
    }

    if (!app.init()) return 1;
    app.run();
    return 0;
}
