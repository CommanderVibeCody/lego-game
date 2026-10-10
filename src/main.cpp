// Brick Wars: Cantina Brawl
// A small third-person brick-toy action game built with raylib.
#include "game.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static void PrintUsage() {
    std::printf(
        "Usage: lego-game [options]\n"
        "  --width N            window width  (default 1280)\n"
        "  --height N           window height (default 720)\n"
        "  --screenshot FILE    run a short scripted scene, save a PNG and exit\n"
        "  --frames N           frame at which the screenshot is taken (default 150)\n");
}

int main(int argc, char** argv) {
    GameOptions options;
    for (int i = 1; i < argc; i++) {
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", name);
                std::exit(1);
            }
            return argv[++i];
        };
        if (!std::strcmp(argv[i], "--width")) options.width = std::atoi(next("--width"));
        else if (!std::strcmp(argv[i], "--height")) options.height = std::atoi(next("--height"));
        else if (!std::strcmp(argv[i], "--screenshot")) options.screenshotPath = next("--screenshot");
        else if (!std::strcmp(argv[i], "--frames")) options.screenshotFrame = std::atoi(next("--frames"));
        else if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h")) { PrintUsage(); return 0; }
        else { std::fprintf(stderr, "unknown option: %s\n", argv[i]); PrintUsage(); return 1; }
    }
    return RunGame(options);
}
