// Toy laser sword and blaster, plus blaster bolts.
#pragma once

#include "raylib.h"

constexpr int kSwordColorCount = 5;
Color SwordColor(int index);
const char* SwordColorName(int index);

// Hand frame comes from MinifigFrame::rightHand. `ignite` is 0..1 (blade length).
void DrawLaserSword(const Matrix& hand, Color blade, float ignite, float time);
void SwordSegment(const Matrix& hand, float ignite, Vector3* base, Vector3* tip);

void DrawBlaster(const Matrix& hand, bool heavy);
Vector3 BlasterMuzzle(const Matrix& hand);

void DrawDrink(const Matrix& hand, Color drink);
void DrawHorn(const Matrix& hand);

struct Bolt {
    Vector3 pos;
    Vector3 vel;
    Color color;
    bool hostile;   // fired by enemies (hurts the player)
    float life;
    bool alive;
};

void DrawBolt(const Bolt& b);
