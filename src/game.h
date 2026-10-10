#pragma once

struct GameOptions {
    int width = 1280;
    int height = 720;
    const char* screenshotPath = nullptr;  // if set: run a scripted scene, save a screenshot and quit
    int screenshotFrame = 150;
    bool proceduralTroopers = false;       // ignore assets/models/trooper.glb
};

int RunGame(const GameOptions& options);
