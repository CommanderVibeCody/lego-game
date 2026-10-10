#include "effects.h"

#include "level.h"
#include "render.h"

#include <algorithm>
#include <cmath>

using namespace R;

namespace {
constexpr float kGravity = 24.0f;
constexpr size_t kMaxPieces = 450;
constexpr size_t kMaxStuds = 300;
constexpr size_t kMaxSparks = 600;

Color StudColor(int value) {
    if (value >= 10000) return Color{190, 70, 230, 255};  // purple
    if (value >= 1000) return Color{60, 130, 255, 255};   // blue
    if (value >= 100) return Color{250, 195, 40, 255};    // gold
    return Color{205, 210, 220, 255};                     // silver
}
}  // namespace

float Randf(float a, float b) { return a + (b - a) * (GetRandomValue(0, 100000) / 100000.0f); }

Vector3 RandomUnit() {
    for (int i = 0; i < 16; i++) {
        Vector3 v = {Randf(-1, 1), Randf(-1, 1), Randf(-1, 1)};
        float l = Vector3Length(v);
        if (l > 0.05f && l <= 1.0f) return Vector3Scale(v, 1.0f / l);
    }
    return Vector3{0, 1, 0};
}

void Effects::SpawnPiece(PieceShape shape, Vector3 size, Color color, Vector3 pos, Vector3 vel) {
    if (pieces_.size() >= kMaxPieces) pieces_.erase(pieces_.begin());
    Piece p;
    p.shape = shape;
    p.size = size;
    p.color = color;
    p.pos = pos;
    p.vel = vel;
    p.axis = RandomUnit();
    p.angle = Randf(0, 2 * PI);
    p.angVel = Randf(5, 14);
    p.life = Randf(4.5f, 6.5f);
    pieces_.push_back(p);
}

void Effects::SpawnStuds(Vector3 at, int totalValue, float spread) {
    const int denoms[] = {10000, 1000, 100, 10};
    int spawned = 0;
    while (totalValue >= 10 && spawned < 14) {
        int value = 10;
        for (int d : denoms) {
            if (totalValue >= d && (d == 10 || GetRandomValue(0, 2) > 0)) { value = d; break; }
        }
        totalValue -= value;
        if (studs_.size() >= kMaxStuds) studs_.erase(studs_.begin());
        Stud s;
        s.pos = at;
        float a = Randf(0, 2 * PI);
        float sp = Randf(1.5f, 4.0f) * spread;
        s.vel = {cosf(a) * sp, Randf(5.5f, 9.0f), sinf(a) * sp};
        s.value = value;
        s.age = 0;
        s.spin = Randf(0, 6);
        s.magnet = false;
        studs_.push_back(s);
        spawned++;
    }
}

void Effects::SpawnSparks(Vector3 at, Color color, int count, float speed) {
    for (int i = 0; i < count; i++) {
        if (sparks_.size() >= kMaxSparks) sparks_.erase(sparks_.begin());
        Spark s;
        s.pos = at;
        s.vel = Vector3Scale(RandomUnit(), Randf(0.3f, 1.0f) * speed);
        s.color = color;
        s.maxLife = s.life = Randf(0.2f, 0.5f);
        s.size = Randf(0.03f, 0.07f);
        sparks_.push_back(s);
    }
}

void Effects::Clear() {
    pieces_.clear();
    studs_.clear();
    sparks_.clear();
}

CollectResult Effects::Update(float dt, const Level& level, Vector3 playerPos, bool canCollect) {
    CollectResult result;

    for (auto& p : pieces_) {
        p.vel.y -= kGravity * dt;
        p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
        p.angle += p.angVel * dt;
        float he = 0.35f * std::min(p.size.y, std::max(p.size.x, p.size.z)) + 0.03f;
        if (p.shape == PieceShape::Cylinder) he = std::min(p.size.x, p.size.y * 0.5f) + 0.02f;
        float ground = level.GroundHeight(p.pos.x, p.pos.z, p.pos.y - he + 0.25f);
        if (p.pos.y - he < ground) {
            p.pos.y = ground + he;
            if (p.vel.y < 0) p.vel.y = -p.vel.y * 0.35f;
            if (p.vel.y < 1.0f) p.vel.y = 0;
            p.vel.x *= 0.72f;
            p.vel.z *= 0.72f;
            p.angVel *= 0.75f;
        }
        Vector3 feet = {p.pos.x, p.pos.y - he, p.pos.z};
        level.ResolveCollision(feet, he);
        p.pos.x = feet.x;
        p.pos.z = feet.z;
        p.life -= dt;
    }
    pieces_.erase(std::remove_if(pieces_.begin(), pieces_.end(), [](const Piece& p) { return p.life <= 0; }),
                  pieces_.end());

    Vector3 chest = {playerPos.x, playerPos.y + 1.0f, playerPos.z};
    for (auto& s : studs_) {
        s.age += dt;
        s.spin += dt * 4.0f;
        float dist = Vector3Distance(s.pos, chest);
        if (canCollect && s.age > 0.45f && dist < 3.2f) s.magnet = true;
        if (s.magnet && canCollect) {
            Vector3 dir = Vector3Normalize(Vector3Subtract(chest, s.pos));
            float speed = 6.0f + 14.0f / std::max(dist, 0.3f);
            s.vel = Vector3Lerp(s.vel, Vector3Scale(dir, speed), std::min(1.0f, dt * 10.0f));
            s.pos = Vector3Add(s.pos, Vector3Scale(s.vel, dt));
            if (Vector3Distance(s.pos, chest) < 0.6f) {
                result.value += s.value;
                result.count++;
                s.age = 1e9f;  // mark for removal
            }
            continue;
        }
        s.vel.y -= kGravity * dt;
        s.pos = Vector3Add(s.pos, Vector3Scale(s.vel, dt));
        float ground = level.GroundHeight(s.pos.x, s.pos.z, s.pos.y + 0.1f);
        if (s.pos.y - 0.17f < ground) {
            s.pos.y = ground + 0.17f;
            if (s.vel.y < 0) s.vel.y = -s.vel.y * 0.45f;
            if (s.vel.y < 1.2f) s.vel.y = 0;
            s.vel.x *= 0.8f;
            s.vel.z *= 0.8f;
        }
        Vector3 feet = {s.pos.x, s.pos.y - 0.17f, s.pos.z};
        level.ResolveCollision(feet, 0.15f);
        s.pos.x = feet.x;
        s.pos.z = feet.z;
    }
    studs_.erase(std::remove_if(studs_.begin(), studs_.end(), [](const Stud& s) { return s.age > 30.0f; }),
                 studs_.end());

    for (auto& s : sparks_) {
        s.vel.y -= 6.0f * dt;
        s.pos = Vector3Add(s.pos, Vector3Scale(s.vel, dt));
        s.life -= dt;
    }
    sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [](const Spark& s) { return s.life <= 0; }),
                  sparks_.end());
    return result;
}

void Effects::Draw(float time) const {
    for (const auto& p : pieces_) {
        float k = p.life < 0.5f ? p.life / 0.5f : 1.0f;
        Vector3 sz = Vector3Scale(p.size, k);
        Matrix rot = MatrixRotate(p.axis, p.angle);
        Matrix tr = MatrixTranslate(p.pos.x, p.pos.y, p.pos.z);
        switch (p.shape) {
            case PieceShape::Cube:
                Cube(Mul(Mul(MatrixScale(sz.x, sz.y, sz.z), rot), tr), p.color);
                // a stud on top of every brick
                Cylinder(Mul(Mul(MatrixScale(0.09f * k, 0.06f * k, 0.09f * k), MatrixTranslate(0, sz.y * 0.5f, 0)),
                             Mul(rot, tr)),
                         p.color);
                break;
            case PieceShape::Cylinder:
                Cylinder(Mul(Mul(Mul(MatrixTranslate(0, -0.5f, 0), MatrixScale(sz.x, sz.y, sz.z)), rot), tr), p.color);
                break;
            case PieceShape::Sphere:
                Sphere(Mul(Mul(MatrixScale(sz.x, sz.y, sz.z), rot), tr), p.color);
                break;
            case PieceShape::Torso:
                Torso(Mul(Mul(Mul(MatrixTranslate(0, -0.5f, 0), MatrixScale(sz.x, sz.y, sz.z)), rot), tr), p.color);
                break;
        }
    }

    for (const auto& s : studs_) {
        if (s.age > 26.0f && fmodf(time * 8.0f, 2.0f) < 1.0f) continue;  // blink before vanishing
        Color c = StudColor(s.value);
        float bob = (s.vel.y == 0 && !s.magnet) ? sinf(time * 3.0f + s.spin) * 0.05f : 0.0f;
        Vector3 p = {s.pos.x, s.pos.y + bob, s.pos.z};
        float scale = s.value >= 1000 ? 1.25f : 1.0f;
        // A coin-like stud standing upright and spinning around Y.
        Matrix frame = Mul(MatrixRotateY(s.spin), MatrixTranslate(p.x, p.y, p.z));
        Cylinder(Mul(Mul(Mul(MatrixTranslate(0, -0.5f, 0), MatrixScale(0.16f * scale, 0.07f, 0.16f * scale)),
                         MatrixRotateX(PI / 2)), frame), c);
        Cylinder(Mul(Mul(Mul(MatrixTranslate(0, 0.5f, 0), MatrixScale(0.1f * scale, 0.04f, 0.1f * scale)),
                         MatrixRotateX(PI / 2)), frame), Shade(c, 1.1f));
        HaloSphere(Sph(MatrixIdentity(), p, V3(0.22f * scale, 0.22f * scale, 0.22f * scale)), WithAlpha(c, 0.07f));
    }

    for (const auto& s : sparks_) {
        float k = s.life / s.maxLife;
        float r = s.size * (0.5f + k);
        GlowSphere(Sph(MatrixIdentity(), s.pos, V3(r, r, r)), Mix(s.color, WHITE, 0.5f * k));
        HaloSphere(Sph(MatrixIdentity(), s.pos, V3(r * 2.2f, r * 2.2f, r * 2.2f)), WithAlpha(s.color, 0.25f * k));
    }
}
