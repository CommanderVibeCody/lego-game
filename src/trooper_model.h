// Optional external trooper model (assets/models/trooper.glb, CC BY 4.0, see CREDITS.md).
// The model is made of separate meshes (legs, arms, hands, body, helmet), so the
// walk cycle and aiming are animated by rotating those meshes around hip and
// shoulder pivots. If the file is missing the game falls back to the procedural
// minifigure.
#pragma once

#include "minifig.h"
#include "raylib.h"

#include <string>
#include <vector>

class TrooperModel {
public:
    bool Load();  // searches next to the executable, two levels up (build/bin) and the working directory
    void Unload();
    bool Loaded() const { return loaded_; }
    const std::string& Path() const { return path_; }

    // Same contract as DrawMinifig(): feet at `pos`, facing +Z rotated by `yaw`.
    MinifigFrame Draw(const MinifigPose& pose, Vector3 pos, float yaw, float flash) const;

private:
    enum Part { Body, RightLeg, LeftLeg, RightArm, LeftArm };

    Model model_ = {};
    bool loaded_ = false;
    std::string path_;
    std::vector<int> part_;          // Part per mesh
    std::vector<Color> baseColor_;   // original diffuse color per material
    Matrix norm_ = {};               // model units -> minifig units (feet at y=0, centered)
    Vector3 hip_ = {};               // pivots in minifig units
    Vector3 shoulder_[2] = {};       // [0] right, [1] left
    Vector3 handCenter_[2] = {};
};
