// Brick debris, collectible studs and glowing sparks.
#pragma once

#include "raylib.h"

#include <vector>

class Level;

enum class PieceShape { Cube, Cylinder, Sphere, Torso };

struct Piece {
    PieceShape shape;
    Vector3 size;   // cube: extents, cylinder: (radius, height, radius), sphere: radii
    Color color;
    Vector3 pos, vel;
    Vector3 axis;
    float angle, angVel;
    float life;
};

struct Stud {
    Vector3 pos, vel;
    int value;
    float age;
    float spin;
    bool magnet;
};

struct Spark {
    Vector3 pos, vel;
    Color color;
    float life, maxLife, size;
};

struct CollectResult {
    int value = 0;
    int count = 0;
};

class Effects {
public:
    void SpawnPiece(PieceShape shape, Vector3 size, Color color, Vector3 pos, Vector3 vel);
    void SpawnStuds(Vector3 at, int totalValue, float spread = 1.0f);
    void SpawnSparks(Vector3 at, Color color, int count, float speed);
    void Clear();

    // Physics + collection. Returns studs picked up by the player this frame.
    CollectResult Update(float dt, const Level& level, Vector3 playerPos, bool canCollect);
    void Draw(float time) const;

    size_t StudCount() const { return studs_.size(); }

private:
    std::vector<Piece> pieces_;
    std::vector<Stud> studs_;
    std::vector<Spark> sparks_;
};

float Randf(float a, float b);
Vector3 RandomUnit();
