#include "trooper_model.h"

#include "render.h"
#include "rlgl.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

using namespace R;

namespace {
constexpr float kTargetHeight = 2.3f;  // matches the procedural trooper incl. helmet

const char* FindModelFile() {
    const char* dir = GetApplicationDirectory();
    const char* candidates[] = {
        TextFormat("%sassets/models/trooper.glb", dir),
        TextFormat("%s../../assets/models/trooper.glb", dir),  // build/bin -> repo root
        TextFormat("%s../assets/models/trooper.glb", dir),
        "assets/models/trooper.glb",
    };
    static std::string found;
    for (const char* c : candidates) {
        if (FileExists(c)) {
            found = c;
            return found.c_str();
        }
    }
    return nullptr;
}

Vector3 Center(const BoundingBox& b) { return Vector3Scale(Vector3Add(b.min, b.max), 0.5f); }
}  // namespace

bool TrooperModel::Load() {
    const char* file = FindModelFile();
    if (!file) {
        TraceLog(LOG_WARNING, "TROOPER: assets/models/trooper.glb not found, using procedural troopers");
        return false;
    }
    model_ = LoadModel(file);
    if (model_.meshCount == 0) {
        TraceLog(LOG_WARNING, "TROOPER: failed to load %s, using procedural troopers", file);
        return false;
    }
    path_ = file;

    // Classify meshes by their bounding boxes (model units, y up, facing +Z).
    BoundingBox all = GetModelBoundingBox(model_);
    float H = all.max.y - all.min.y;
    std::vector<BoundingBox> boxes(model_.meshCount);
    part_.assign(model_.meshCount, Body);
    BoundingBox legs = {{FLT_MAX, FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX, -FLT_MAX}};
    BoundingBox arm[2] = {legs, legs};
    BoundingBox hand[2] = {legs, legs};
    int legCount = 0, armCount = 0;
    for (int i = 0; i < model_.meshCount; i++) {
        BoundingBox b = boxes[i] = GetMeshBoundingBox(model_.meshes[i]);
        Vector3 c = Center(b);
        int side = c.x < 0 ? 0 : 1;  // the figure faces +Z, so its right side is -X
        if (b.max.y < all.min.y + 0.4f * H && fabsf(c.x) > 0.04f * H) {
            part_[i] = side == 0 ? RightLeg : LeftLeg;
            legs.min = Vector3Min(legs.min, b.min);
            legs.max = Vector3Max(legs.max, b.max);
            legCount++;
        } else if (fabsf(c.x) > 0.15f * H && c.y > all.min.y + 0.25f * H && c.y < all.min.y + 0.8f * H) {
            part_[i] = side == 0 ? RightArm : LeftArm;
            // The upper limb part is the one closer to the body; the outer one is the hand.
            armCount++;
        }
    }
    // Split arm meshes into arm (closest to the body) and hand (furthest out / lowest).
    for (int s = 0; s < 2; s++) {
        int want = s == 0 ? RightArm : LeftArm;
        float bestInner = FLT_MAX;
        int armIdx = -1;
        for (int i = 0; i < model_.meshCount; i++)
            if (part_[i] == want && fabsf(Center(boxes[i]).x) < bestInner) { bestInner = fabsf(Center(boxes[i]).x); armIdx = i; }
        for (int i = 0; i < model_.meshCount; i++) {
            if (part_[i] != want) continue;
            BoundingBox& target = (i == armIdx) ? arm[s] : hand[s];
            target.min = Vector3Min(target.min, boxes[i].min);
            target.max = Vector3Max(target.max, boxes[i].max);
        }
        if (hand[s].min.x == FLT_MAX) hand[s] = arm[s];
    }
    if (legCount < 2 || armCount < 2) {
        TraceLog(LOG_WARNING, "TROOPER: unexpected mesh layout (%d leg / %d arm meshes), limbs will not animate",
                 legCount, armCount);
    }

    // Normalize: feet on y=0, centered between the legs, scaled to minifig height.
    float s = kTargetHeight / H;
    Vector3 off = legCount > 0 ? Vector3{(legs.min.x + legs.max.x) * 0.5f, all.min.y, (legs.min.z + legs.max.z) * 0.5f}
                               : Vector3{Center(all).x, all.min.y, Center(all).z};
    norm_ = MatrixMultiply(MatrixTranslate(-off.x, -off.y, -off.z), MatrixScale(s, s, s));
    auto toFig = [&](Vector3 p) { return Vector3Scale(Vector3Subtract(p, off), s); };

    hip_ = toFig({0, legs.max.y - 0.1f * (legs.max.y - legs.min.y), (legs.min.z + legs.max.z) * 0.5f});
    for (int i = 0; i < 2; i++) {
        float h = arm[i].max.y - arm[i].min.y;
        Vector3 c = Center(arm[i]);
        shoulder_[i] = toFig({c.x, arm[i].max.y - 0.2f * h, c.z});
        handCenter_[i] = toFig(Center(hand[i]));
    }

    // Use the game's lighting shader for every material and remember the colors (for hit flashes).
    baseColor_.resize(model_.materialCount);
    for (int i = 0; i < model_.materialCount; i++) {
        model_.materials[i].shader = R::LitShader();
        baseColor_[i] = model_.materials[i].maps[MATERIAL_MAP_DIFFUSE].color;
    }
    loaded_ = true;
    TraceLog(LOG_INFO, "TROOPER: loaded %s (%d meshes, %d materials, scale %.4f)", file, model_.meshCount,
             model_.materialCount, s);
    return true;
}

void TrooperModel::Unload() {
    if (!loaded_) return;
    // The shader belongs to the renderer; detach it so UnloadModel does not free it.
    for (int i = 0; i < model_.materialCount; i++) model_.materials[i].shader = Shader{rlGetShaderIdDefault(), nullptr};
    UnloadModel(model_);
    loaded_ = false;
}

MinifigFrame TrooperModel::Draw(const MinifigPose& P, Vector3 pos, float yaw, float flash) const {
    MinifigFrame out;
    Matrix root = Mul(MatrixRotateY(yaw), MatrixTranslate(pos.x, pos.y, pos.z));
    out.root = root;
    float swing = sinf(P.walkPhase) * P.walkAmt;

    Matrix legM[2], armM[2];
    for (int side = 0; side < 2; side++) {
        float legA = P.seated ? -PI / 2 : swing * 0.65f * (side == 0 ? 1.0f : -1.0f);
        legM[side] = Mul(Mul(MatrixTranslate(-hip_.x, -hip_.y, -hip_.z), MatrixRotateX(legA)),
                         Mul(MatrixTranslate(hip_.x, hip_.y, hip_.z), root));
        bool right = side == 0;
        float pitch = right ? P.rArmPitch - swing * 0.55f * P.rArmSwing : P.lArmPitch + swing * 0.55f * P.lArmSwing;
        float armYaw = right ? P.rArmYaw : 0.0f;
        Vector3 sh = shoulder_[side];
        Matrix rot = Mul(MatrixRotateX(pitch), MatrixRotateY(armYaw));
        armM[side] = Mul(Mul(MatrixTranslate(-sh.x, -sh.y, -sh.z), rot), Mul(MatrixTranslate(sh.x, sh.y, sh.z), root));
        // Grip frame like the procedural figure: -Y along the arm, +Z through the hand.
        Vector3 hc = Vector3Subtract(handCenter_[side], sh);
        Matrix grip = Mul(Mul(Mul(MatrixTranslate(hc.x, hc.y, hc.z), MatrixRotateY(right ? P.rWristRoll : 0.0f)), rot),
                          Mul(MatrixTranslate(sh.x, sh.y, sh.z), root));
        if (right) out.rightHand = grip; else out.leftHand = grip;
    }

    for (int i = 0; i < model_.meshCount; i++) {
        Matrix m;
        switch (part_[i]) {
            case RightLeg: m = legM[0]; break;
            case LeftLeg: m = legM[1]; break;
            case RightArm: m = armM[0]; break;
            case LeftArm: m = armM[1]; break;
            default: m = root; break;
        }
        int mat = model_.meshMaterial[i];
        Material& material = model_.materials[mat];
        material.maps[MATERIAL_MAP_DIFFUSE].color = flash > 0 ? Mix(baseColor_[mat], WHITE, flash) : baseColor_[mat];
        DrawMesh(model_.meshes[i], material, Mul(norm_, m));
    }
    return out;
}
