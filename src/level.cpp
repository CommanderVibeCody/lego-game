#include "level.h"

#include "render.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>

using namespace R;

namespace {
const Color kSand = {219, 186, 128, 255};
const Color kSandDark = {196, 160, 104, 255};
const Color kWall = {214, 184, 138, 255};
const Color kWallTrim = {180, 148, 104, 255};
const Color kFloor = {156, 122, 88, 255};
const Color kFloorInner = {136, 104, 76, 255};
const Color kMetal = {112, 116, 126, 255};
const Color kMetalDark = {70, 72, 80, 255};
const Color kWood = {128, 90, 60, 255};
const Color kNeon = {90, 225, 255, 255};
constexpr int kWallSegments = 48;
constexpr float kDeg = PI / 180.0f;
}  // namespace

void Level::Build() {
    obstacles_.clear();
    tables_.clear();
    seats_.clear();
    lamps_.clear();
    buildings_.clear();
    vaporators_.clear();

    // Central round bar with a drinks tower in the middle.
    obstacles_.push_back({Obstacle::Ring, {0, 0}, 3.9f, 3.0f, {}, 1.1f});
    obstacles_.push_back({Obstacle::Circle, {0, 0}, 0.95f, 0, {}, 3.4f});

    // Round tables with stools.
    const float tableAngles[] = {50, 100, 145, 265, 310};
    const Color lampColors[] = {{255, 140, 50, 255}, {80, 210, 255, 255}, {255, 80, 190, 255},
                                {255, 190, 80, 255}, {120, 255, 140, 255}};
    int li = 0;
    for (float a : tableAngles) {
        Vector2 c = {sinf(a * kDeg) * 10.0f, cosf(a * kDeg) * 10.0f};
        tables_.push_back(c);
        obstacles_.push_back({Obstacle::Circle, c, 1.0f, 0, {}, 1.05f});
        for (int s = 0; s < 3; s++) {
            float sa = a * kDeg + PI / 3 + s * (2 * PI / 3);
            Vector2 p = {c.x + sinf(sa) * 1.65f, c.y + cosf(sa) * 1.65f};
            obstacles_.push_back({Obstacle::Circle, p, 0.32f, 0, {}, 0.6f});
            seats_.push_back({{p.x, 0.6f, p.y}, atan2f(c.x - p.x, c.y - p.y)});
        }
        Color lc = lampColors[li++ % 5];
        lamps_.push_back({{c.x, 3.4f, c.y}, lc, 1.3f});
    }
    lamps_.push_back({{3.4f, 3.6f, 0}, Color{255, 170, 90, 255}, 1.5f});
    lamps_.push_back({{-3.4f, 3.6f, 0}, Color{90, 200, 255, 255}, 1.5f});
    lamps_.push_back({{stageCenter.x, 3.4f, stageCenter.y + 1.2f}, Color{255, 90, 160, 255}, 1.8f});

    // Band stage
    obstacles_.push_back({Obstacle::Box, stageCenter, 0, 0, stageHalf, stageTop});

    // Outside: adobe huts and moisture-collector towers.
    buildings_ = {{{-26, 14}, 4.0f, 3.5f, kWall}, {{24, 22}, 5.0f, 4.0f, kSandDark}, {{28, -16}, 3.5f, 3.0f, kWall},
                  {{-22, -26}, 5.5f, 4.0f, kSandDark}, {{4, -34}, 4.0f, 3.0f, kWall}};
    for (auto& b : buildings_) obstacles_.push_back({Obstacle::Circle, b.c, b.r, 0, {}, 100.0f});
    vaporators_ = {{9, 27}, {-11, 31}, {31, 5}, {-34, -4}};
    for (auto& v : vaporators_) obstacles_.push_back({Obstacle::Circle, v, 0.5f, 0, {}, 100.0f});

    bottleColors_.clear();
    const Color bottle[] = {{60, 200, 90, 255}, {200, 60, 60, 255}, {70, 120, 230, 255}, {230, 180, 60, 255},
                            {170, 80, 220, 255}, {90, 220, 210, 255}};
    for (int i = 0; i < 40; i++) bottleColors_.push_back(bottle[(i * 7 + i / 3) % 6]);
}

void Level::ApplyLights() const {
    R::ClearLights();
    for (const auto& l : lamps_)
        R::AddLight(l.pos, V3(l.color.r / 255.0f * l.intensity, l.color.g / 255.0f * l.intensity,
                              l.color.b / 255.0f * l.intensity));
}

float Level::DomeY(float r) const {
    float t = Clamp(r / R, 0.0f, 1.0f);
    return wallH + domeScale * R * sqrtf(1.0f - t * t);
}

bool Level::Inside(float x, float z) const { return sqrtf(x * x + z * z) < R; }

void Level::Draw(float time) const {
    Matrix I = MatrixIdentity();

    // Desert floor and dunes
    Cube(Box(I, V3(0, -0.5f, 0), V3(260, 1, 260)), kSand);
    const Vector3 dunes[] = {{-45, -1.2f, 30}, {50, -1.0f, -25}, {12, -1.5f, -62}, {-55, -1.0f, -45}, {55, -1.2f, 45}};
    for (int i = 0; i < 5; i++)
        Sphere(Sph(I, dunes[i], V3(14.0f + i * 2, 3.0f, 9.0f + i)), i % 2 ? kSand : kSandDark);

    // Cantina floor
    Cylinder(Cyl(I, V3(0, -0.01f, 0), R + 0.4f, 0.03f), kFloor);
    Cylinder(Cyl(I, V3(0, -0.005f, 0), 7.0f, 0.03f), kFloorInner);
    // Entrance ramp / mat
    Cube(Box(I, V3(0, 0.0f, R + 1.6f), V3(3.4f, 0.04f, 3.6f)), kSandDark);

    // Round wall made of segments; the two segments facing +Z form the doorway.
    float segW = 2 * PI * R / kWallSegments + 0.12f;
    for (int i = 0; i < kWallSegments; i++) {
        float a = (i + 0.5f) * 2 * PI / kWallSegments;
        Matrix seg = Mul(MatrixRotateY(a), MatrixTranslate(sinf(a) * R, 0, cosf(a) * R));
        bool door = (i == 0 || i == kWallSegments - 1);
        if (door) {
            Cube(Box(seg, V3(0, (doorH + wallH) * 0.5f, 0), V3(segW, wallH - doorH, wallThick)), kWall);
        } else {
            Cube(Box(seg, V3(0, wallH * 0.5f, 0), V3(segW, wallH, wallThick)), kWall);
            if (i % 4 == 2) {  // small round windows with a warm glow
                GlowCylinder(Mul(Mul(Mul(MatrixTranslate(0, -0.5f, 0), MatrixScale(0.35f, wallThick + 0.04f, 0.35f)),
                                     Mul(MatrixRotateX(PI / 2), MatrixTranslate(0, 2.8f, 0))), seg),
                             Color{60, 45, 35, 255});
            }
        }
        Cube(Box(seg, V3(0, wallH + 0.1f, 0), V3(segW, 0.2f, wallThick + 0.25f)), kWallTrim);
        Cube(Box(seg, V3(0, 0.25f, -wallThick * 0.5f - 0.05f), V3(segW, 0.5f, 0.1f)), kWallTrim);  // inner skirting
    }
    // Door frame
    for (int side = -1; side <= 1; side += 2) {
        Cube(Box(I, V3(side * (doorHalf + 0.3f), doorH * 0.5f + 0.1f, R + 0.05f), V3(0.6f, doorH + 0.2f, 1.3f)), kWallTrim);
    }
    Cube(Box(I, V3(0, doorH + 0.15f, R + 0.05f), V3(doorHalf * 2 + 1.2f, 0.35f, 1.35f)), kWallTrim);
    GlowCube(Box(I, V3(0, doorH - 0.05f, R + 0.72f), V3(doorHalf * 2 - 0.4f, 0.08f, 0.04f)), Color{255, 160, 70, 255});

    // Dome (two-sided: sunlit outside, lamp-lit inside)
    rlDisableBackfaceCulling();
    SetShellMode(true);
    Dome(Mul(MatrixScale(R + 0.3f, R * domeScale, R + 0.3f), MatrixTranslate(0, wallH + 0.15f, 0)), kWall);
    SetShellMode(false);
    rlEnableBackfaceCulling();
    // Dome skylight ring
    Cylinder(Cyl(I, V3(0, DomeY(0) - 0.25f, 0), 2.6f, 0.5f), kWallTrim);
    // Vents on the dome
    for (int i = 0; i < 6; i++) {
        float a = i * PI / 3 + 0.3f;
        float rr = R * 0.62f;
        Cylinder(Cyl(I, V3(sinf(a) * rr, DomeY(rr) - 0.4f, cosf(a) * rr), 0.6f, 1.1f), kWallTrim);
    }

    // Central bar ---------------------------------------------------------------
    const int barSegs = 36;
    float barR = 3.45f, barW = 0.9f;
    for (int i = 0; i < barSegs; i++) {
        float a = (i + 0.5f) * 2 * PI / barSegs;
        Matrix seg = Mul(MatrixRotateY(a), MatrixTranslate(sinf(a) * barR, 0, cosf(a) * barR));
        float w = 2 * PI * barR / barSegs + 0.06f;
        Cube(Box(seg, V3(0, 0.5f, 0), V3(w, 1.0f, barW)), kWood);
        Cube(Box(seg, V3(0, 1.05f, 0), V3(w * 1.12f, 0.1f, barW + 0.25f)), kMetal);
        GlowCube(Box(seg, V3(0, 0.82f, barW * 0.5f + 0.01f), V3(w, 0.05f, 0.02f)), kNeon);
        Cube(Box(seg, V3(0, 0.08f, barW * 0.5f + 0.1f), V3(w, 0.16f, 0.2f)), kMetalDark);  // foot rail
    }
    // Drinks tower
    Cylinder(Cyl(I, V3(0, 0, 0), 0.75f, 3.4f), kMetalDark);
    Cylinder(Cyl(I, V3(0, 3.4f, 0), 1.2f, 0.15f), kMetal);
    for (int tier = 0; tier < 2; tier++) {
        float y = 1.25f + tier * 0.7f;
        Cylinder(Cyl(I, V3(0, y, 0), 0.98f, 0.05f), kMetal);
        for (int b = 0; b < 16; b++) {
            float a = b * 2 * PI / 16 + tier * 0.2f;
            Vector3 p = {sinf(a) * 0.86f, y + 0.05f, cosf(a) * 0.86f};
            Color c = bottleColors_[(b + tier * 16) % bottleColors_.size()];
            Cylinder(Cyl(I, p, 0.065f, 0.26f), c);
            Cylinder(Cyl(I, V3(p.x, p.y + 0.26f, p.z), 0.025f, 0.1f), c);
        }
    }
    GlowCylinder(Cyl(I, V3(0, 2.75f, 0), 0.77f, 0.06f), kNeon);

    // Tables & stools -------------------------------------------------------------
    for (size_t t = 0; t < tables_.size(); t++) {
        Vector2 c = tables_[t];
        Cylinder(Cyl(I, V3(c.x, 0, c.y), 0.5f, 0.06f), kMetalDark);
        Cylinder(Cyl(I, V3(c.x, 0, c.y), 0.14f, 0.95f), kMetal);
        Cylinder(Cyl(I, V3(c.x, 0.95f, c.y), 1.0f, 0.1f), kMetal);
        Cylinder(Cyl(I, V3(c.x, 0.93f, c.y), 1.04f, 0.06f), kMetalDark);
        // glasses with glowing drinks
        for (int g = 0; g < 2; g++) {
            float a = (float)t * 1.7f + g * 2.4f;
            Vector3 p = {c.x + sinf(a) * 0.5f, 1.05f, c.y + cosf(a) * 0.5f};
            Cylinder(Cyl(I, p, 0.08f, 0.22f), Color{200, 220, 230, 255});
            GlowCylinder(Cyl(I, V3(p.x, p.y + 0.12f, p.z), 0.07f, 0.06f), bottleColors_[(t * 2 + g) % 6]);
        }
        const Lamp& l = lamps_[t];
        Cylinder(Cyl(I, V3(l.pos.x, l.pos.y + 0.2f, l.pos.z), 0.025f, DomeY(10.0f) - l.pos.y), kMetalDark);
        Cylinder(Cyl(I, V3(l.pos.x, l.pos.y + 0.05f, l.pos.z), 0.4f, 0.2f), kMetalDark);
    }
    for (const auto& s : seats_) {
        Cylinder(Cyl(I, V3(s.pos.x, 0, s.pos.z), 0.07f, 0.52f), kMetal);
        Cylinder(Cyl(I, V3(s.pos.x, 0.0f, s.pos.z), 0.22f, 0.04f), kMetalDark);
        Cylinder(Cyl(I, V3(s.pos.x, 0.5f, s.pos.z), 0.32f, 0.1f), kWood);
    }

    // Stage
    Cube(Box(I, V3(stageCenter.x, stageTop * 0.5f, stageCenter.y), V3(stageHalf.x * 2, stageTop, stageHalf.y * 2)), kWood);
    GlowCube(Box(I, V3(stageCenter.x, stageTop - 0.05f, stageCenter.y + stageHalf.y + 0.01f), V3(stageHalf.x * 2, 0.05f, 0.02f)),
             Color{255, 90, 160, 255});
    Cube(Box(I, V3(stageCenter.x, 1.8f, stageCenter.y - stageHalf.y + 0.1f), V3(stageHalf.x * 2, 3.0f, 0.2f)), kWallTrim);

    // Lamps (glowing bulbs + halos)
    for (const auto& l : lamps_) {
        float flick = 1.0f + 0.05f * sinf(time * 13.0f + l.pos.x);
        GlowSphere(Sph(I, l.pos, V3(0.2f, 0.2f, 0.2f)), Mix(l.color, WHITE, 0.6f));
        const float radii[] = {0.32f, 0.5f, 0.75f};
        const float alphas[] = {0.30f, 0.12f, 0.05f};
        for (int k = 0; k < 3; k++) {
            float r = radii[k] * flick;
            HaloSphere(Sph(I, l.pos, V3(r, r, r)), WithAlpha(l.color, alphas[k]));
        }
    }

    // Outside props -----------------------------------------------------------------
    for (const auto& b : buildings_) {
        Cylinder(Cyl(I, V3(b.c.x, 0, b.c.y), b.r, b.h), b.color);
        Dome(Mul(MatrixScale(b.r, b.r * 0.6f, b.r), MatrixTranslate(b.c.x, b.h, b.c.y)), b.color);
        Cylinder(Cyl(I, V3(b.c.x, b.h - 0.1f, b.c.y), b.r + 0.15f, 0.25f), kWallTrim);
        // dark doorway facing the cantina
        float a = atan2f(-b.c.x, -b.c.y);
        Matrix f = Mul(MatrixRotateY(a), MatrixTranslate(b.c.x + sinf(a) * b.r, 0, b.c.y + cosf(a) * b.r));
        Cube(Box(f, V3(0, 1.2f, 0), V3(1.4f, 2.4f, 0.3f)), Color{50, 40, 35, 255});
    }
    for (const auto& v : vaporators_) {
        Cylinder(Cyl(I, V3(v.x, 0, v.y), 0.22f, 5.0f), kMetal);
        for (int r = 0; r < 3; r++) Cylinder(Cyl(I, V3(v.x, 1.4f + r * 1.4f, v.y), 0.5f - r * 0.08f, 0.18f), kMetalDark);
        Sphere(Sph(I, V3(v.x, 5.0f, v.y), V3(0.25f, 0.25f, 0.25f)), kMetal);
        GlowSphere(Sph(I, V3(v.x, 5.3f, v.y), V3(0.07f, 0.07f, 0.07f)), Color{255, 60, 60, 255});
    }

    // Twin suns in the sky
    GlowSphere(Sph(I, V3(-160, 120, -230), V3(9, 9, 9)), Color{255, 248, 220, 255});
    HaloSphere(Sph(I, V3(-160, 120, -230), V3(22, 22, 22)), Color{255, 230, 170, 60});
    GlowSphere(Sph(I, V3(-110, 95, -250), V3(6, 6, 6)), Color{255, 220, 170, 255});
    HaloSphere(Sph(I, V3(-110, 95, -250), V3(16, 16, 16)), Color{255, 200, 140, 50});
}

// --- Collision ------------------------------------------------------------------

bool Level::InsideObstacle(const Obstacle& o, float x, float z) const {
    float dx = x - o.c.x, dz = z - o.c.y;
    switch (o.kind) {
        case Obstacle::Circle: return dx * dx + dz * dz < o.r * o.r;
        case Obstacle::Box: return fabsf(dx) < o.half.x && fabsf(dz) < o.half.y;
        case Obstacle::Ring: {
            float d = sqrtf(dx * dx + dz * dz);
            return d > o.rIn && d < o.r;
        }
    }
    return false;
}

float Level::GroundHeight(float x, float z, float maxY) const {
    float g = 0.0f;
    auto test = [&](const Obstacle& o) {
        if (o.top <= maxY && o.top > g && InsideObstacle(o, x, z)) g = o.top;
    };
    for (const auto& o : obstacles_) test(o);
    for (const auto& o : dynamic_) test(o);
    return g;
}

void Level::PushOut(const Obstacle& o, Vector3& feet, float radius) const {
    if (feet.y >= o.top - 0.05f) return;  // standing on / above it
    float dx = feet.x - o.c.x, dz = feet.z - o.c.y;
    switch (o.kind) {
        case Obstacle::Circle: {
            float d = sqrtf(dx * dx + dz * dz), minD = o.r + radius;
            if (d < minD) {
                if (d < 1e-4f) { dx = 1; dz = 0; d = 1; }
                feet.x = o.c.x + dx / d * minD;
                feet.z = o.c.y + dz / d * minD;
            }
            break;
        }
        case Obstacle::Ring: {
            float d = sqrtf(dx * dx + dz * dz);
            if (d > o.rIn - radius && d < o.r + radius) {
                if (d < 1e-4f) { dx = 1; dz = 0; d = 1; }
                float target = (d < (o.rIn + o.r) * 0.5f) ? o.rIn - radius : o.r + radius;
                feet.x = o.c.x + dx / d * target;
                feet.z = o.c.y + dz / d * target;
            }
            break;
        }
        case Obstacle::Box: {
            float qx = Clamp(feet.x, o.c.x - o.half.x, o.c.x + o.half.x);
            float qz = Clamp(feet.z, o.c.y - o.half.y, o.c.y + o.half.y);
            float ex = feet.x - qx, ez = feet.z - qz;
            float d = sqrtf(ex * ex + ez * ez);
            if (d > 1e-4f) {
                if (d < radius) {
                    feet.x = qx + ex / d * radius;
                    feet.z = qz + ez / d * radius;
                }
            } else {  // center inside the box: leave through the closest side
                float px = o.half.x - fabsf(dx), pz = o.half.y - fabsf(dz);
                if (px < pz) feet.x = o.c.x + (dx < 0 ? -1 : 1) * (o.half.x + radius);
                else feet.z = o.c.y + (dz < 0 ? -1 : 1) * (o.half.y + radius);
            }
            break;
        }
    }
}

void Level::ResolveCollision(Vector3& feet, float radius) const {
    // Round cantina wall with a doorway on +Z.
    float r = sqrtf(feet.x * feet.x + feet.z * feet.z);
    bool inDoor = feet.z > 0 && fabsf(feet.x) < doorHalf - radius * 0.6f && feet.y < doorH;
    float inner = R - wallThick * 0.5f - radius, outer = R + wallThick * 0.5f + radius;
    if (!inDoor && feet.y < wallH + 0.2f && r > inner && r < outer && r > 1e-4f) {
        float target = (r < R) ? inner : outer;
        feet.x *= target / r;
        feet.z *= target / r;
    }
    // Door frame pillars
    for (int side = -1; side <= 1; side += 2) {
        Obstacle pillar{Obstacle::Box, {side * (doorHalf + 0.3f), R + 0.05f}, 0, 0, {0.3f, 0.65f}, doorH + 0.2f};
        PushOut(pillar, feet, radius);
    }
    for (const auto& o : obstacles_) PushOut(o, feet, radius);
    for (const auto& o : dynamic_) PushOut(o, feet, radius);

    float lr = sqrtf(feet.x * feet.x + feet.z * feet.z);
    if (lr > worldLimit) {
        feet.x *= worldLimit / lr;
        feet.z *= worldLimit / lr;
    }
}

bool Level::BlocksPoint(Vector3 p) const {
    if (p.y < 0.05f) return true;
    float r = sqrtf(p.x * p.x + p.z * p.z);
    if (r > R - wallThick * 0.5f - 0.1f && r < R + wallThick * 0.5f + 0.1f && p.y < wallH + 0.3f) {
        bool inDoor = p.z > 0 && fabsf(p.x) < doorHalf && p.y < doorH;
        if (!inDoor) return true;
    }
    if (r < R + 0.3f && p.y > wallH && p.y > DomeY(r) - 0.25f && p.y < DomeY(r) + 0.6f) return true;
    for (const auto& o : obstacles_)
        if (p.y < o.top && InsideObstacle(o, p.x, p.z)) return true;
    for (const auto& o : dynamic_)
        if (p.y < o.top && InsideObstacle(o, p.x, p.z)) return true;
    return false;
}

bool Level::LineOfSight(Vector3 a, Vector3 b) const {
    float len = Vector3Distance(a, b);
    int steps = std::max(1, (int)(len / 0.4f));
    for (int i = 1; i < steps; i++) {
        Vector3 p = Vector3Lerp(a, b, (float)i / steps);
        if (BlocksPoint(p)) return false;
    }
    return true;
}
