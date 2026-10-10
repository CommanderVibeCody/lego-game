// The desert spaceport cantina: procedural geometry, lights and collision.
#pragma once

#include "raylib.h"

#include <vector>

struct Obstacle {
    enum Kind { Circle, Box, Ring } kind;
    Vector2 c;          // center (x, z)
    float r = 0;        // circle radius / ring outer radius
    float rIn = 0;      // ring inner radius
    Vector2 half = {};  // box half extents
    float top = 0;      // height of the top surface (you can stand on it)
};

struct Lamp {
    Vector3 pos;
    Color color;
    float intensity;
};

struct Seat {
    Vector3 pos;   // top center of the stool
    float yaw;     // facing towards the table
};

class Level {
public:
    // Cantina dimensions
    float R = 16.0f;          // room radius
    float wallH = 4.5f;
    float wallThick = 0.8f;
    float doorHalf = 1.85f;   // half width of the door gap (centered on +Z)
    float doorH = 3.3f;
    float domeScale = 0.5f;
    float worldLimit = 60.0f;

    Vector2 stageCenter = {-6.5f, -9.5f};
    Vector2 stageHalf = {2.4f, 1.5f};
    float stageTop = 0.45f;

    void Build();
    void Draw(float time) const;
    void ApplyLights() const;

    // Highest walkable surface below `maxY` at (x, z). 0 is the floor.
    float GroundHeight(float x, float z, float maxY) const;
    // Push a circle standing with its feet at `feet` out of walls and props.
    void ResolveCollision(Vector3& feet, float radius) const;
    // True if the point is inside solid geometry (walls, dome, props, floor).
    bool BlocksPoint(Vector3 p) const;
    bool LineOfSight(Vector3 a, Vector3 b) const;
    bool Inside(float x, float z) const;

    // Extra obstacles that change at runtime (breakable crates).
    void SetDynamicObstacles(const std::vector<Obstacle>& obs) const { dynamic_ = obs; }

    const std::vector<Seat>& Seats() const { return seats_; }
    const std::vector<Lamp>& Lamps() const { return lamps_; }

private:
    float DomeY(float r) const;
    bool InsideObstacle(const Obstacle& o, float x, float z) const;
    void PushOut(const Obstacle& o, Vector3& feet, float radius) const;

    std::vector<Obstacle> obstacles_;
    mutable std::vector<Obstacle> dynamic_;
    std::vector<Vector2> tables_;
    std::vector<Seat> seats_;
    std::vector<Lamp> lamps_;
    struct Building { Vector2 c; float r, h; Color color; };
    std::vector<Building> buildings_;
    std::vector<Vector2> vaporators_;
    std::vector<Color> bottleColors_;
};
