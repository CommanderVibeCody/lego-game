#include "minifig.h"

#include "effects.h"
#include "render.h"

#include <cmath>

using namespace R;

namespace {
constexpr float kHipY = 0.82f;
const Color kBlack = {25, 25, 28, 255};
const Color kGrey = {120, 124, 130, 255};
}  // namespace

MinifigFrame DrawMinifig(const MinifigLook& L, const MinifigPose& P, Vector3 pos, float yaw, float flash) {
    auto F = [flash](Color c) { return flash > 0 ? Mix(c, WHITE, flash) : c; };

    MinifigFrame out;
    Matrix root = Mul(MatrixRotateY(yaw), MatrixTranslate(pos.x, pos.y, pos.z));
    out.root = root;
    float swing = sinf(P.walkPhase) * P.walkAmt;

    // Legs -------------------------------------------------------------------
    Matrix hip = Mul(MatrixTranslate(0, kHipY, 0), root);
    for (int side = -1; side <= 1; side += 2) {
        float a = P.seated ? -PI / 2 : swing * 0.65f * (side < 0 ? 1.0f : -1.0f);
        Matrix lm = Mul(MatrixRotateX(a), hip);
        Cube(Box(lm, V3(side * 0.19f, -0.40f, 0), V3(0.36f, 0.70f, 0.42f)), F(L.legs));
        Cube(Box(lm, V3(side * 0.19f, -0.765f, 0.035f), V3(0.36f, 0.11f, 0.49f)), F(Shade(L.legs, 0.92f)));
    }

    // Hips -------------------------------------------------------------------
    Cube(Box(root, V3(0, 0.87f, 0), V3(0.78f, 0.16f, 0.42f)), F(L.hips));
    Cube(Box(root, V3(0, 0.76f, 0), V3(0.10f, 0.22f, 0.40f)), F(L.hips));

    // Torso ------------------------------------------------------------------
    Matrix upper = Mul(MatrixRotateY(P.torsoYaw), root);
    Torso(Mul(Mul(MatrixScale(0.80f, 0.76f, 0.40f), MatrixTranslate(0, 0.95f, 0)), upper), F(L.torso));
    if (L.vest.a > 0) {
        for (int side = -1; side <= 1; side += 2) {
            Cube(Box(upper, V3(side * 0.19f, 1.33f, 0.2f), V3(0.15f, 0.72f, 0.03f)), F(L.vest));
            Cube(Box(upper, V3(side * 0.19f, 1.33f, -0.2f), V3(0.15f, 0.72f, 0.03f)), F(L.vest));
        }
    }
    if (L.belt.a > 0) {
        Cube(Box(upper, V3(0, 1.0f, 0), V3(0.79f, 0.08f, 0.42f)), F(L.belt));
        Cube(Box(upper, V3(0, 1.0f, 0.205f), V3(0.12f, 0.09f, 0.03f)), F(Color{200, 180, 90, 255}));
    }
    if (L.armorDetail) {
        Cube(Box(upper, V3(0, 1.48f, 0.205f), V3(0.34f, 0.06f, 0.03f)), F(kBlack));
        Cube(Box(upper, V3(-0.08f, 1.30f, 0.205f), V3(0.1f, 0.12f, 0.03f)), F(kGrey));
        Cube(Box(upper, V3(0.08f, 1.30f, 0.205f), V3(0.1f, 0.12f, 0.03f)), F(kBlack));
        Cube(Box(upper, V3(0, 1.0f, 0.205f), V3(0.6f, 0.07f, 0.03f)), F(kGrey));
        Cube(Box(root, V3(0, 0.87f, 0.215f), V3(0.5f, 0.06f, 0.02f)), F(kBlack));
    }

    // Neck + head ------------------------------------------------------------
    Cylinder(Cyl(upper, V3(0, 1.69f, 0), 0.12f, 0.1f), F(L.head));
    Matrix head = Mul(MatrixRotateY(P.headYaw), upper);
    Cylinder(Cyl(head, V3(0, 1.79f, 0), 0.22f, 0.36f), F(L.head));
    Cylinder(Cyl(head, V3(0, 1.77f, 0), 0.205f, 0.40f), F(L.head));  // rounded-looking edges
    Cylinder(Cyl(head, V3(0, 2.17f, 0), 0.12f, 0.08f), F(L.head));   // stud

    if (L.gear != HeadGear::Helmet) {
        float eye = L.bigEyes ? 0.055f : 0.035f;
        Sphere(Sph(head, V3(-0.08f, 2.01f, 0.205f), V3(eye, eye * 1.3f, 0.02f)), kBlack);
        Sphere(Sph(head, V3(0.08f, 2.01f, 0.205f), V3(eye, eye * 1.3f, 0.02f)), kBlack);
        Cube(Box(head, V3(0, 1.89f, 0.212f), V3(0.13f, 0.022f, 0.02f)), kBlack);
        Cube(Box(head, V3(-0.07f, 1.905f, 0.21f), V3(0.03f, 0.03f, 0.02f)), kBlack);
        Cube(Box(head, V3(0.07f, 1.905f, 0.21f), V3(0.03f, 0.03f, 0.02f)), kBlack);
    }
    if (L.antennae) {
        for (int side = -1; side <= 1; side += 2) {
            Cylinder(Cyl(head, V3(side * 0.12f, 2.12f, 0), 0.02f, 0.28f), F(L.head));
            Sphere(Sph(head, V3(side * 0.12f, 2.42f, 0), V3(0.05f, 0.05f, 0.05f)), F(Shade(L.head, 0.7f)));
        }
    }

    switch (L.gear) {
        case HeadGear::Hair:
            Sphere(Sph(head, V3(0, 2.09f, -0.01f), V3(0.255f, 0.16f, 0.255f)), F(L.gearColor));
            Cube(Box(head, V3(0, 1.94f, -0.12f), V3(0.48f, 0.36f, 0.22f)), F(L.gearColor));
            break;
        case HeadGear::Helmet: {
            const Color g = F(L.gearColor);
            Cylinder(Cyl(head, V3(0, 1.74f, 0), 0.29f, 0.3f), g);
            Sphere(Sph(head, V3(0, 2.04f, 0), V3(0.29f, 0.25f, 0.29f)), g);
            Cylinder(Cyl(head, V3(0, 1.72f, -0.02f), 0.31f, 0.08f), g);                // flared rim
            Cube(Box(head, V3(0, 2.1f, 0.02f), V3(0.08f, 0.2f, 0.58f)), F(Shade(L.gearColor, 0.93f)));  // crest ridge
            for (int s2 = -1; s2 <= 1; s2 += 2) {                                      // ear pieces
                Cylinder(Mul(Mul(Mul(MatrixTranslate(0, -0.5f, 0), MatrixScale(0.09f, 0.06f, 0.09f)), MatrixRotateZ(PI / 2)),
                             Mul(MatrixTranslate(s2 * 0.3f, 1.93f, 0.0f), head)), F(kGrey));
            }
            Cube(Box(head, V3(0, 2.11f, 0.24f), V3(0.36f, 0.04f, 0.06f)), F(kGrey));   // brow stripe
            Cube(Box(head, V3(-0.1f, 2.0f, 0.26f), V3(0.13f, 0.09f, 0.07f)), kBlack);  // eye lenses
            Cube(Box(head, V3(0.1f, 2.0f, 0.26f), V3(0.13f, 0.09f, 0.07f)), kBlack);
            Cube(Box(head, V3(0, 1.85f, 0.28f), V3(0.16f, 0.13f, 0.06f)), F(kGrey));   // breathing grille
            for (int k = -1; k <= 1; k++) Cube(Box(head, V3(k * 0.045f, 1.85f, 0.31f), V3(0.015f, 0.10f, 0.02f)), kBlack);
            Cube(Box(head, V3(0, 1.94f, 0.285f), V3(0.05f, 0.06f, 0.05f)), kBlack);
            for (int s2 = -1; s2 <= 1; s2 += 2)                                        // cheek "tears"
                Cube(Box(head, V3(s2 * 0.19f, 1.86f, 0.22f), V3(0.05f, 0.1f, 0.04f)), F(kGrey));
            break;
        }
        case HeadGear::Hood:
            Sphere(Sph(head, V3(0, 2.1f, -0.03f), V3(0.30f, 0.2f, 0.30f)), F(L.gearColor));
            Cube(Box(head, V3(0, 1.92f, -0.13f), V3(0.6f, 0.42f, 0.28f)), F(L.gearColor));
            Cylinder(Cyl(upper, V3(0, 1.62f, -0.04f), 0.34f, 0.12f), F(L.gearColor));
            break;
        case HeadGear::Cap:
            Sphere(Sph(head, V3(0, 2.1f, 0), V3(0.25f, 0.13f, 0.25f)), F(L.gearColor));
            Cube(Box(head, V3(0, 2.07f, 0.24f), V3(0.36f, 0.04f, 0.18f)), F(Shade(L.gearColor, 0.8f)));
            break;
        case HeadGear::None: break;
    }

    // Arms + C-hands -----------------------------------------------------------
    for (int side = -1; side <= 1; side += 2) {
        bool right = side < 0;  // the figure faces +Z, so its right arm is on -X
        float pitch = right ? P.rArmPitch - swing * 0.55f * P.rArmSwing : P.lArmPitch + swing * 0.55f * P.lArmSwing;
        float armYaw = right ? P.rArmYaw : 0.0f;
        float wrist = right ? P.rWristRoll : 0.0f;
        Matrix rot = Mul(Mul(MatrixRotateZ(side * 0.10f), MatrixRotateX(pitch)), MatrixRotateY(armYaw));
        Matrix arm = Mul(rot, Mul(MatrixTranslate(side * 0.36f, 1.58f, 0), upper));
        Cube(Box(arm, V3(side * 0.05f, -0.24f, 0), V3(0.2f, 0.48f, 0.24f)), F(L.arms));
        Sphere(Sph(arm, V3(side * 0.04f, -0.03f, 0), V3(0.12f, 0.12f, 0.13f)), F(L.arms));
        Matrix hand = Mul(Mul(MatrixRotateY(wrist), MatrixTranslate(side * 0.06f, -0.56f, 0.02f)), arm);
        Cylinder(Cyl(hand, V3(0, -0.01f, 0), 0.065f, 0.1f), F(L.hands));
        Matrix grip = Mul(MatrixTranslate(0, -0.08f, 0), hand);
        Cylinder(CylZ(grip, V3(0, 0, -0.08f), 0.10f, 0.16f), F(L.hands));
        Cylinder(CylZ(grip, V3(0, 0, -0.085f), 0.05f, 0.17f), Color{40, 30, 15, 255});  // the hole of the "C"
        if (right) out.rightHand = grip; else out.leftHand = grip;
    }
    return out;
}

void BreakMinifig(const MinifigLook& L, Vector3 pos, float yaw, Vector3 impulseDir, Effects& fx) {
    Matrix root = Mul(MatrixRotateY(yaw), MatrixTranslate(pos.x, pos.y, pos.z));
    auto spawn = [&](PieceShape shape, Vector3 size, Color c, Vector3 local) {
        Vector3 wp = Vector3Transform(local, root);
        Vector3 v = Vector3Add(Vector3Scale(impulseDir, Randf(2.0f, 5.0f)), Vector3Scale(RandomUnit(), Randf(1.5f, 3.5f)));
        v.y = Randf(4.0f, 8.5f);
        fx.SpawnPiece(shape, size, c, wp, v);
    };
    spawn(PieceShape::Cylinder, V3(0.22f, 0.40f, 0.22f), L.head, V3(0, 1.97f, 0));
    spawn(PieceShape::Torso, V3(0.8f, 0.76f, 0.4f), L.torso, V3(0, 1.33f, 0));
    spawn(PieceShape::Cube, V3(0.78f, 0.16f, 0.42f), L.hips, V3(0, 0.87f, 0));
    spawn(PieceShape::Cube, V3(0.36f, 0.78f, 0.45f), L.legs, V3(-0.19f, 0.42f, 0));
    spawn(PieceShape::Cube, V3(0.36f, 0.78f, 0.45f), L.legs, V3(0.19f, 0.42f, 0));
    spawn(PieceShape::Cube, V3(0.2f, 0.5f, 0.24f), L.arms, V3(-0.42f, 1.35f, 0));
    spawn(PieceShape::Cube, V3(0.2f, 0.5f, 0.24f), L.arms, V3(0.42f, 1.35f, 0));
    spawn(PieceShape::Cylinder, V3(0.1f, 0.16f, 0.1f), L.hands, V3(-0.45f, 1.0f, 0.1f));
    spawn(PieceShape::Cylinder, V3(0.1f, 0.16f, 0.1f), L.hands, V3(0.45f, 1.0f, 0.1f));
    if (L.gear == HeadGear::Helmet) spawn(PieceShape::Sphere, V3(0.29f, 0.27f, 0.29f), L.gearColor, V3(0, 2.1f, 0));
    else if (L.gear != HeadGear::None) spawn(PieceShape::Sphere, V3(0.26f, 0.16f, 0.26f), L.gearColor, V3(0, 2.15f, 0));
    for (int i = 0; i < 3; i++)  // a few extra small bricks for that satisfying crunch
        spawn(PieceShape::Cube, V3(0.18f, 0.12f, 0.18f), i == 0 ? L.torso : L.legs, V3(0, 1.0f + 0.3f * i, 0));
}
