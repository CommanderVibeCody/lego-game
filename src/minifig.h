// Procedural brick-toy minifigure: cylinder head with stud, trapezoid torso,
// blocky arms with C-shaped hands, hips and legs. No external assets.
#pragma once

#include "raylib.h"

class Effects;

enum class HeadGear { None, Hair, Helmet, Hood, Cap };

struct MinifigLook {
    Color head = {250, 205, 40, 255};
    Color torso = {200, 200, 200, 255};
    Color arms = {200, 200, 200, 255};
    Color hands = {250, 205, 40, 255};
    Color hips = {90, 90, 90, 255};
    Color legs = {90, 90, 90, 255};
    HeadGear gear = HeadGear::None;
    Color gearColor = {110, 70, 40, 255};
    Color vest = {0, 0, 0, 0};   // alpha 0 = no vest
    Color belt = {0, 0, 0, 0};   // alpha 0 = no belt
    bool armorDetail = false;    // trooper-style chest plates
    bool antennae = false;       // alien patrons
    bool bigEyes = false;
};

struct MinifigPose {
    float walkPhase = 0;   // radians, advances while walking
    float walkAmt = 0;     // 0..1 blend of the walk cycle
    float rArmPitch = 0;   // negative = forward / up
    float rArmYaw = 0;
    float rWristRoll = 0;
    float rArmSwing = 1;   // how much the walk cycle swings the right arm
    float lArmPitch = 0;
    float lArmSwing = 1;
    float torsoYaw = 0;
    float headYaw = 0;
    bool seated = false;
};

struct MinifigFrame {
    Matrix root;
    Matrix rightHand;  // grip frame: +Z through the C-hand, -Y along the arm
    Matrix leftHand;
};

// Height of a minifigure in world units (feet to top of stud).
constexpr float kMinifigHeight = 2.25f;

MinifigFrame DrawMinifig(const MinifigLook& look, const MinifigPose& pose, Vector3 feetPos, float yaw,
                         float flash = 0.0f);

// Smash a minifigure into loose bricks flying away from `impulseDir`.
void BreakMinifig(const MinifigLook& look, Vector3 feetPos, float yaw, Vector3 impulseDir, Effects& fx);
