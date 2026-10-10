#include "weapons.h"

#include "render.h"

#include <cmath>

using namespace R;

namespace {
const Color kColors[kSwordColorCount] = {
    {70, 150, 255, 255}, {60, 235, 90, 255}, {255, 50, 50, 255}, {190, 80, 255, 255}, {255, 220, 60, 255}};
const char* kNames[kSwordColorCount] = {"Blue", "Green", "Red", "Purple", "Yellow"};
constexpr float kBladeLength = 1.35f;
constexpr float kHiltTop = 0.2f;
}  // namespace

Color SwordColor(int index) { return kColors[((index % kSwordColorCount) + kSwordColorCount) % kSwordColorCount]; }
const char* SwordColorName(int index) {
    return kNames[((index % kSwordColorCount) + kSwordColorCount) % kSwordColorCount];
}

void DrawLaserSword(const Matrix& hand, Color blade, float ignite, float time) {
    const Color metal = {185, 188, 195, 255};
    const Color dark = {30, 30, 34, 255};
    // Hilt runs along the hand's +Z axis.
    Cylinder(CylZ(hand, V3(0, 0, -0.2f), 0.055f, 0.4f), metal);
    for (int i = 0; i < 4; i++) Cylinder(CylZ(hand, V3(0, 0, -0.14f + i * 0.06f), 0.06f, 0.025f), dark);
    Cylinder(CylZ(hand, V3(0, 0, 0.13f), 0.07f, 0.07f), metal);              // emitter
    Cube(Box(hand, V3(0, 0.07f, 0.0f), V3(0.03f, 0.04f, 0.08f)), dark);       // activation button
    Cylinder(CylZ(hand, V3(0, 0, -0.24f), 0.045f, 0.04f), dark);              // pommel
    if (ignite <= 0.01f) return;

    float len = kBladeLength * ignite;
    float flicker = 1.0f + 0.06f * sinf(time * 47.0f) + 0.04f * sinf(time * 31.0f);
    GlowCylinder(CylZ(hand, V3(0, 0, kHiltTop), 0.032f, len), Mix(blade, WHITE, 0.82f));
    Sphere(Sph(hand, V3(0, 0, kHiltTop + len), V3(0.032f, 0.032f, 0.032f)), Mix(blade, WHITE, 0.82f));
    HaloCylinder(CylZ(hand, V3(0, 0, kHiltTop - 0.02f), 0.07f * flicker, len + 0.06f), WithAlpha(blade, 0.85f));
    HaloCylinder(CylZ(hand, V3(0, 0, kHiltTop - 0.04f), 0.13f * flicker, len + 0.1f), WithAlpha(blade, 0.30f));
    HaloSphere(Sph(hand, V3(0, 0, kHiltTop + len), V3(0.12f, 0.12f, 0.12f)), WithAlpha(blade, 0.4f));
}

void SwordSegment(const Matrix& hand, float ignite, Vector3* base, Vector3* tip) {
    *base = Vector3Transform(V3(0, 0, kHiltTop), hand);
    *tip = Vector3Transform(V3(0, 0, kHiltTop + kBladeLength * ignite), hand);
}

// The blaster barrel points along the arm (-Y of the hand frame); "up" on the gun is +Z.
void DrawBlaster(const Matrix& hand, bool heavy) {
    const Color body = {35, 35, 40, 255};
    const Color metal = {110, 112, 120, 255};
    float len = heavy ? 0.62f : 0.5f;
    Cube(Box(hand, V3(0, 0.0f, -0.02f), V3(0.08f, 0.11f, 0.22f)), body);                 // grip in the hand
    Cube(Box(hand, V3(0, -len * 0.45f, 0.13f), V3(0.11f, len, 0.13f)), body);           // body
    Cylinder(CylNegY(hand, V3(0, -len * 0.9f, 0.13f), 0.04f, 0.18f), metal);            // barrel
    Cylinder(CylNegY(hand, V3(0, -len * 0.9f - 0.18f, 0.13f), 0.05f, 0.04f), body);     // muzzle
    Cylinder(CylNegY(hand, V3(0, -0.05f, 0.23f), 0.035f, 0.25f), metal);                // scope
    if (heavy) Cube(Box(hand, V3(0, 0.2f, 0.12f), V3(0.08f, 0.22f, 0.1f)), body);       // stock
}

Vector3 BlasterMuzzle(const Matrix& hand) { return Vector3Transform(V3(0, -0.8f, 0.13f), hand); }

void DrawDrink(const Matrix& hand, Color drink) {
    Matrix up = Mul(MatrixRotateX(PI / 2), hand);  // glass stands along the hand axis
    Cylinder(Cyl(up, V3(0, -0.08f, 0), 0.085f, 0.26f), Color{200, 220, 230, 255});
    GlowCylinder(Cyl(up, V3(0, 0.12f, 0), 0.07f, 0.07f), drink);
}

void DrawHorn(const Matrix& hand) {
    const Color brass = {220, 175, 70, 255};
    Cylinder(CylZ(hand, V3(0, 0, -0.35f), 0.04f, 0.75f), brass);
    Cylinder(CylZ(hand, V3(0, 0, 0.35f), 0.15f, 0.12f), brass);
    Cylinder(CylZ(hand, V3(0, 0, 0.3f), 0.09f, 0.08f), brass);
}

void DrawBolt(const Bolt& b) {
    Vector3 dir = Vector3Normalize(b.vel);
    Vector3 tail = Vector3Subtract(b.pos, Vector3Scale(dir, 0.8f));
    GlowCylinder(CylBetween(tail, b.pos, 0.035f), Mix(b.color, WHITE, 0.7f));
    HaloCylinder(CylBetween(Vector3Subtract(tail, Vector3Scale(dir, 0.1f)), Vector3Add(b.pos, Vector3Scale(dir, 0.1f)), 0.09f),
                 WithAlpha(b.color, 0.75f));
    HaloSphere(Sph(MatrixIdentity(), b.pos, V3(0.18f, 0.18f, 0.18f)), WithAlpha(b.color, 0.15f));
}
