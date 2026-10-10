#include "game.h"

#include "audio.h"
#include "effects.h"
#include "level.h"
#include "minifig.h"
#include "render.h"
#include "trooper_model.h"
#include "weapons.h"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace R;

namespace {

constexpr float kGravity = 26.0f;
constexpr float kMoveSpeed = 6.0f;
constexpr float kJumpVel = 9.5f;
constexpr float kFigRadius = 0.42f;
constexpr int kMaxHearts = 4;
constexpr float kSwingTime = 0.38f;
constexpr float kFixedDt = 1.0f / 60.0f;
const Color kSky = {228, 210, 180, 255};
const Color kSkin = {250, 205, 40, 255};
const Color kEnemyBolt = {255, 60, 40, 255};
const Color kPlayerBolt = {255, 120, 40, 255};

float WrapAngle(float a) {
    while (a > PI) a -= 2 * PI;
    while (a < -PI) a += 2 * PI;
    return a;
}
float ApproachAngle(float cur, float target, float step) {
    float d = WrapAngle(target - cur);
    if (fabsf(d) <= step) return target;
    return cur + (d > 0 ? step : -step);
}
float Approach(float cur, float target, float step) {
    if (cur < target) return std::min(cur + step, target);
    return std::max(cur - step, target);
}
float YawTo(Vector3 from, Vector3 to) { return atan2f(to.x - from.x, to.z - from.z); }
float HDist(Vector3 a, Vector3 b) { return sqrtf((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z)); }
float Smooth(float t) { t = Clamp(t, 0.0f, 1.0f); return t * t * (3 - 2 * t); }

enum class Weapon { Sword, Blaster };
enum class NpcKind { Trooper, Patron, Seated, Musician, Bartender };

struct Player {
    Vector3 pos = {0, 0, 9};
    float velY = 0;
    float yaw = PI;
    bool onGround = true;
    int jumpsLeft = 2;
    int hearts = kMaxHearts;
    float invuln = 0, regen = 0, hurtFlash = 0;
    Weapon weapon = Weapon::Sword;
    int swordColor = 0;
    float ignite = 0;
    float attackT = -1;
    int swingStyle = 0;
    int swingId = 0;
    bool blocking = false;
    float fireCd = 0;
    float aimHold = 0;
    float walkPhase = 0, walkAmt = 0;
    bool alive = true;
    float respawnT = 0;
    Matrix hand = MatrixIdentity();
    bool hasHand = false;
};

struct Npc {
    NpcKind kind = NpcKind::Patron;
    MinifigLook look;
    Vector3 pos = {}, home = {};
    float yaw = 0, homeYaw = 0, velY = 0;
    bool alive = true;
    int hp = 1;
    float walkPhase = 0, walkAmt = 0;
    float thinkT = 0;
    Vector2 target = {};
    float shootT = 2.0f;
    float flash = 0;
    float respawnT = 0;
    float seed = 0;
    float losT = 0;
    bool los = false;
    int lastSwing = -1;
    Color drink = {};
    Matrix hand = MatrixIdentity();
    bool hasHand = false;
};

struct Crate {
    Vector3 pos;
    Color color;
    bool alive = true;
    float respawnT = 0;
    int lastSwing = -1;
};

struct OrbitCam {
    float yaw = 0, pitch = 0.38f, dist = 7.5f, curDist = 7.5f;
    Vector3 target = {0, 1.7f, 9};
    Camera3D cam = {};
};

MinifigLook TrooperLook() {
    MinifigLook L;
    const Color white = {236, 236, 240, 255};
    L.torso = L.arms = L.hips = L.legs = white;
    L.hands = {30, 30, 34, 255};
    L.gear = HeadGear::Helmet;
    L.gearColor = {242, 242, 245, 255};
    L.armorDetail = true;
    return L;
}

MinifigLook HeroLook() {
    MinifigLook L;
    L.torso = L.arms = {228, 218, 195, 255};
    L.hands = kSkin;
    L.hips = {120, 84, 54, 255};
    L.legs = {200, 184, 150, 255};
    L.gear = HeadGear::Hair;
    L.gearColor = {160, 110, 55, 255};
    L.belt = {110, 74, 44, 255};
    return L;
}

MinifigLook PatronLook(int i) {
    const Color heads[] = {kSkin, {110, 190, 90, 255}, {90, 150, 225, 255}, kSkin, {230, 120, 150, 255}, {190, 160, 120, 255}};
    const Color torsos[] = {{150, 60, 50, 255}, {60, 80, 130, 255}, {90, 110, 70, 255}, {70, 60, 55, 255},
                            {170, 140, 90, 255}, {120, 70, 130, 255}, {40, 40, 45, 255}};
    const Color legs[] = {{60, 50, 45, 255}, {40, 40, 50, 255}, {110, 90, 70, 255}, {80, 60, 40, 255}};
    const Color gear[] = {{100, 70, 40, 255}, {60, 50, 45, 255}, {150, 120, 80, 255}, {30, 30, 30, 255}};
    MinifigLook L;
    L.head = heads[i % 6];
    L.hands = (i % 6 == 1 || i % 6 == 2 || i % 6 == 4) ? L.head : kSkin;
    L.torso = torsos[(i * 3) % 7];
    L.arms = (i % 2) ? L.torso : torsos[(i * 3 + 2) % 7];
    L.hips = L.legs = legs[i % 4];
    switch (i % 5) {
        case 0: L.gear = HeadGear::Hood; break;
        case 1: L.gear = HeadGear::None; break;
        case 2: L.gear = HeadGear::Cap; break;
        case 3: L.gear = HeadGear::Hair; break;
        default: L.gear = HeadGear::None; break;
    }
    L.gearColor = gear[(i * 7) % 4];
    L.antennae = (i % 6 == 2);
    L.bigEyes = (i % 6 == 1 || i % 6 == 4);
    if (i % 3 == 0) L.vest = {50, 40, 35, 255};
    if (i % 4 == 1) L.belt = {60, 45, 30, 255};
    return L;
}

void DrawHeart(float x, float y, float s, Color c) {
    DrawCircleV(Vector2{x - s * 0.5f, y}, s * 0.56f, c);
    DrawCircleV(Vector2{x + s * 0.5f, y}, s * 0.56f, c);
    DrawTriangle(Vector2{x - s * 1.04f, y + s * 0.18f}, Vector2{x, y + s * 1.35f}, Vector2{x + s * 1.04f, y + s * 0.18f}, c);
}

void DrawStudIcon(float x, float y, float r) {
    DrawCircleV(Vector2{x, y}, r, Color{120, 124, 135, 255});
    DrawCircleV(Vector2{x, y}, r * 0.86f, Color{210, 214, 222, 255});
    DrawCircleV(Vector2{x - r * 0.15f, y - r * 0.15f}, r * 0.5f, Color{235, 238, 244, 255});
}

void DrawTextShadow(const char* text, int x, int y, int size, Color c) {
    DrawText(text, x + 2, y + 2, size, Color{0, 0, 0, (unsigned char)(c.a * 0.6f)});
    DrawText(text, x, y, size, c);
}

class Game {
public:
    explicit Game(const GameOptions& o) : opt_(o) {}
    int Run();

private:
    void Init();
    void SetupDemoScene();
    void Update(float dt);
    void UpdatePlayer(float dt);
    void UpdateCamera(float dt);
    void UpdateNpcs(float dt);
    void UpdateBolts(float dt);
    void UpdateCrates(float dt);
    void UpdateWaves(float dt);
    void Draw();
    void DrawHud();

    void SpawnWave();
    void HitNpc(Npc& n, int dmg, Vector3 dir);
    void HurtPlayer(Vector3 dir);
    void BreakCrate(Crate& c, Vector3 dir);
    void FireBolt(Vector3 from, Vector3 dir, bool hostile, Color c, float speed);
    void Say(const std::string& text) { message_ = text; messageT_ = 3.0f; }
    MinifigPose PlayerPose() const;
    bool CursorLocked() const { return cursorLocked_; }

    GameOptions opt_;
    Level level_;
    Effects fx_;
    TrooperModel trooperModel_;
    Player player_;
    OrbitCam cam_;
    std::vector<Npc> npcs_;
    std::vector<Bolt> bolts_;
    std::vector<Crate> crates_;
    int studs_ = 0;
    float studPop_ = 0;
    int wave_ = 0;
    float waveT_ = 3.0f;
    int kills_ = 0;
    float time_ = 0;
    int frame_ = 0;
    std::string message_;
    float messageT_ = 0;
    bool showHelp_ = true;
    bool cursorLocked_ = true;
    bool demo_ = false;
};

void Game::Init() {
    level_.Build();
    R::SetSun(V3(-0.45f, -1.0f, -0.35f), V3(0.95f, 0.88f, 0.76f), V3(0.50f, 0.48f, 0.46f), V3(0.56f, 0.54f, 0.54f));
    R::SetFog(kSky, 0.011f);
    R::SetRoom(level_.R);
    level_.ApplyLights();

    // Seated patrons on some of the stools
    const auto& seats = level_.Seats();
    int idx = 0;
    for (size_t i = 0; i < seats.size(); i++) {
        if (i % 3 == 2) continue;
        Npc n;
        n.kind = NpcKind::Seated;
        n.look = PatronLook(idx++);
        Vector3 fwd = {sinf(seats[i].yaw), 0, cosf(seats[i].yaw)};
        n.home = n.pos = Vector3Add(V3(seats[i].pos.x, seats[i].pos.y - 0.80f, seats[i].pos.z), Vector3Scale(fwd, -0.08f));
        n.homeYaw = n.yaw = seats[i].yaw;
        n.seed = Randf(0, 10);
        const Color drinks[] = {{80, 230, 120, 255}, {255, 120, 60, 255}, {90, 160, 255, 255}};
        n.drink = drinks[i % 3];
        npcs_.push_back(n);
    }
    // Wandering patrons
    for (int i = 0; i < 4; i++) {
        Npc n;
        n.kind = NpcKind::Patron;
        n.look = PatronLook(idx++ + 3);
        float a = Randf(0, 2 * PI);
        n.home = n.pos = V3(sinf(a) * 7.0f, 0, cosf(a) * 7.0f);
        n.seed = Randf(0, 10);
        npcs_.push_back(n);
    }
    // The band on the stage
    for (int i = 0; i < 3; i++) {
        Npc n;
        n.kind = NpcKind::Musician;
        MinifigLook L = PatronLook(1);
        L.head = Color{(unsigned char)(170 + i * 20), 120, (unsigned char)(90 + i * 30), 255};
        L.hands = L.head;
        L.gear = HeadGear::None;
        L.bigEyes = true;
        L.torso = L.arms = Color{30, 30, 35, 255};
        L.vest = {0, 0, 0, 0};
        n.look = L;
        n.home = n.pos = V3(level_.stageCenter.x - 1.4f + i * 1.4f, level_.stageTop, level_.stageCenter.y);
        n.homeYaw = n.yaw = 0.15f;
        n.seed = i * 1.3f;
        npcs_.push_back(n);
    }
    // Bartender behind the round bar
    {
        Npc n;
        n.kind = NpcKind::Bartender;
        n.look = PatronLook(5);
        n.look.gear = HeadGear::None;
        n.look.vest = {200, 200, 205, 255};
        n.home = n.pos = V3(0, 0, 2.2f);
        npcs_.push_back(n);
    }

    // Breakable brick crates
    const Color crateColors[] = {{200, 40, 40, 255}, {40, 90, 200, 255}, {240, 190, 30, 255}, {50, 150, 60, 255}, {150, 150, 155, 255}};
    const Vector3 cratePos[] = {{3.6f, 0, 19.5f}, {-4.2f, 0, 20.5f}, {-4.9f, 0, 21.4f}, {6.5f, 0, 24.0f},
                                {13.0f, 0, 6.0f}, {-13.5f, 0, 4.5f}, {8.0f, 0, -12.0f}, {-1.5f, 0, -13.8f}};
    int ci = 0;
    for (auto p : cratePos) crates_.push_back({p, crateColors[ci++ % 5]});
    crates_.push_back({V3(-4.55f, 0.9f, 20.95f), crateColors[2]});  // stacked on top

    Say("Welcome to the desert spaceport cantina!");
}

void Game::SetupDemoScene() {
    demo_ = true;
    cursorLocked_ = false;
    showHelp_ = true;
    player_.pos = V3(1.0f, 0, 9.0f);
    player_.yaw = PI + 0.15f;
    player_.ignite = 1.0f;
    player_.swordColor = 0;
    cam_.yaw = 0.62f;
    cam_.pitch = 0.30f;
    cam_.dist = cam_.curDist = 7.0f;
    cam_.target = V3(player_.pos.x, 1.7f, player_.pos.z);
    wave_ = 1;
    waveT_ = 999;
    const Vector3 spots[] = {{-1.2f, 0, 6.3f}, {2.6f, 0, 5.4f}, {-4.0f, 0, 7.5f}, {5.3f, 0, 7.8f}, {0.8f, 0, 4.9f}};
    for (auto s : spots) {
        Npc n;
        n.kind = NpcKind::Trooper;
        n.look = TrooperLook();
        n.hp = 2;
        n.pos = n.home = s;
        n.yaw = YawTo(s, player_.pos);
        n.seed = Randf(0, 10);
        n.shootT = Randf(0.3f, 1.5f);
        npcs_.push_back(n);
    }
}

void Game::SpawnWave() {
    wave_++;
    int count = std::min(2 + wave_, 9);
    for (int i = 0; i < count; i++) {
        Npc n;
        n.kind = NpcKind::Trooper;
        n.look = TrooperLook();
        n.hp = 2;
        n.pos = n.home = V3(Randf(-3.0f, 3.0f), 0, level_.R + 5.0f + i * 1.3f);
        n.yaw = PI;
        n.seed = Randf(0, 10);
        n.shootT = Randf(1.5f, 3.0f);
        npcs_.push_back(n);
    }
    Say(TextFormat("Wave %d: %d troopers are storming the cantina!", wave_, count));
}

void Game::FireBolt(Vector3 from, Vector3 dir, bool hostile, Color c, float speed) {
    Bolt b;
    b.pos = from;
    b.vel = Vector3Scale(Vector3Normalize(dir), speed);
    b.color = c;
    b.hostile = hostile;
    b.life = 2.5f;
    b.alive = true;
    bolts_.push_back(b);
}

void Game::HitNpc(Npc& n, int dmg, Vector3 dir) {
    if (!n.alive) return;
    n.hp -= dmg;
    n.flash = 0.25f;
    Vector3 chest = V3(n.pos.x, n.pos.y + 1.3f, n.pos.z);
    fx_.SpawnSparks(chest, Color{255, 200, 120, 255}, 10, 5.0f);
    if (n.hp > 0) {
        n.pos = Vector3Add(n.pos, Vector3Scale(V3(dir.x, 0, dir.z), 0.3f));
        return;
    }
    n.alive = false;
    BreakMinifig(n.look, n.pos, n.yaw, dir, fx_);
    Audio::Play(Sfx::Break, Randf(0.9f, 1.15f));
    if (n.kind == NpcKind::Trooper) {
        kills_++;
        fx_.SpawnStuds(chest, GetRandomValue(5, 15) * 10);
        n.respawnT = -1;  // troopers do not come back; new waves do
    } else {
        fx_.SpawnStuds(chest, GetRandomValue(2, 6) * 10);
        n.respawnT = 25.0f;
    }
}

void Game::HurtPlayer(Vector3 dir) {
    if (!player_.alive || player_.invuln > 0 || demo_) return;
    player_.hearts--;
    player_.invuln = 1.2f;
    player_.hurtFlash = 0.35f;
    player_.regen = 0;
    Audio::Play(Sfx::Hurt);
    fx_.SpawnSparks(V3(player_.pos.x, player_.pos.y + 1.3f, player_.pos.z), Color{255, 120, 80, 255}, 12, 5.0f);
    if (player_.hearts <= 0) {
        player_.alive = false;
        player_.respawnT = 2.5f;
        BreakMinifig(HeroLook(), player_.pos, player_.yaw, dir, fx_);
        Audio::Play(Sfx::Break, 0.85f);
        int lost = std::min(studs_, std::max(studs_ / 3, 0));
        lost -= lost % 10;
        studs_ -= lost;
        if (lost > 0) fx_.SpawnStuds(V3(player_.pos.x, player_.pos.y + 1.0f, player_.pos.z), lost, 1.6f);
        Say("You fell apart! Rebuilding...");
    }
}

void Game::BreakCrate(Crate& c, Vector3 dir) {
    if (!c.alive) return;
    c.alive = false;
    c.respawnT = 30.0f;
    for (int i = 0; i < 6; i++) {
        Vector3 p = Vector3Add(c.pos, V3(Randf(-0.25f, 0.25f), 0.25f + (i / 2) * 0.3f, Randf(-0.25f, 0.25f)));
        Vector3 v = Vector3Add(Vector3Scale(dir, Randf(1.0f, 3.0f)), Vector3Scale(RandomUnit(), 2.5f));
        v.y = Randf(4.0f, 7.0f);
        fx_.SpawnPiece(PieceShape::Cube, V3(0.42f, 0.28f, 0.42f), c.color, p, v);
    }
    fx_.SpawnStuds(Vector3Add(c.pos, V3(0, 0.6f, 0)), GetRandomValue(4, 12) * 10);
    Audio::Play(Sfx::Break, Randf(1.0f, 1.25f));
}

MinifigPose Game::PlayerPose() const {
    const Player& p = player_;
    MinifigPose pose;
    pose.walkPhase = p.walkPhase;
    pose.walkAmt = p.walkAmt;
    if (p.weapon == Weapon::Sword) {
        pose.rArmPitch = -0.55f;
        pose.rArmSwing = 0.25f;
        if (p.attackT >= 0) {
            float t = p.attackT / kSwingTime;
            float wind = Smooth(t / 0.25f);                 // 0..1 during wind-up
            float strike = Smooth((t - 0.25f) / 0.4f);      // 0..1 during the strike
            float back = Smooth((t - 0.7f) / 0.3f);         // 0..1 while recovering
            float startPitch, endPitch, startYaw, endYaw, roll;
            switch (p.swingStyle) {
                case 0:  // overhead chop
                    startPitch = -2.9f; endPitch = -0.35f; startYaw = 0.35f; endYaw = -0.25f; roll = 0.0f;
                    break;
                case 1:  // horizontal slash right -> left
                    startPitch = -1.45f; endPitch = -1.35f; startYaw = -1.1f; endYaw = 1.0f; roll = -PI / 2;
                    break;
                default:  // backhand slash left -> right
                    startPitch = -1.5f; endPitch = -1.3f; startYaw = 1.0f; endYaw = -1.1f; roll = PI / 2;
                    break;
            }
            float pitch = Lerp(Lerp(-0.55f, startPitch, wind), endPitch, strike);
            float twist = Lerp(Lerp(0.0f, startYaw, wind), endYaw, strike);
            pose.rArmPitch = Lerp(pitch, -0.55f, back);
            pose.torsoYaw = Lerp(twist * 0.6f, 0.0f, back);
            pose.rArmYaw = Lerp(twist * 0.4f, 0.0f, back);
            pose.rWristRoll = Lerp(roll * std::max(wind, 0.0f), 0.0f, back);
            pose.rArmSwing = 0;
        } else if (p.blocking) {
            pose.rArmPitch = -1.25f;
            pose.rWristRoll = PI / 2;
            pose.rArmSwing = 0;
            pose.lArmPitch = -0.6f;
        }
    } else {
        if (p.aimHold > 0) {
            pose.rArmPitch = -PI / 2 + Clamp(cam_.pitch * 0.45f, -0.3f, 0.5f);
            pose.rArmSwing = 0;
            pose.lArmPitch = -1.1f;
            pose.lArmSwing = 0;
        } else {
            pose.rArmPitch = -0.35f;
            pose.rArmSwing = 0.4f;
        }
    }
    return pose;
}

void Game::UpdatePlayer(float dt) {
    Player& p = player_;
    p.invuln = std::max(0.0f, p.invuln - dt);
    p.hurtFlash = std::max(0.0f, p.hurtFlash - dt);

    if (!p.alive) {
        p.respawnT -= dt;
        if (p.respawnT <= 0) {
            p = Player{};
            p.weapon = player_.weapon;
            p.invuln = 2.0f;
            p.pos = V3(0, 0, 9);
            p.yaw = PI;
            Audio::Play(Sfx::Ignite);
            fx_.SpawnSparks(V3(0, 1.2f, 9), Color{255, 230, 120, 255}, 30, 4.0f);
        }
        return;
    }

    // Regenerate one heart every few seconds without damage.
    if (p.hearts < kMaxHearts) {
        p.regen += dt;
        if (p.regen > 7.0f) { p.hearts++; p.regen = 0; }
    }

    // --- Weapon switching -----------------------------------------------------
    bool wasSword = p.weapon == Weapon::Sword;
    if (!demo_) {
        if (IsKeyPressed(KEY_ONE)) p.weapon = Weapon::Sword;
        if (IsKeyPressed(KEY_TWO)) p.weapon = Weapon::Blaster;
        if (IsKeyPressed(KEY_Q)) p.weapon = (p.weapon == Weapon::Sword) ? Weapon::Blaster : Weapon::Sword;
        if (IsKeyPressed(KEY_F) && p.weapon == Weapon::Sword) {
            p.swordColor = (p.swordColor + 1) % kSwordColorCount;
            Audio::Play(Sfx::Ignite, 1.2f, 0.6f);
        }
    }
    if (!wasSword && p.weapon == Weapon::Sword) { p.ignite = 0; Audio::Play(Sfx::Ignite); }
    if (p.weapon != Weapon::Sword) { p.attackT = -1; p.ignite = 0; }

    // --- Movement relative to the camera ---------------------------------------
    Vector2 in = {0, 0};
    if (!demo_) {
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) in.y += 1;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) in.y -= 1;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) in.x += 1;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) in.x -= 1;
    }
    Vector2 fwd = {-sinf(cam_.yaw), -cosf(cam_.yaw)};
    Vector2 right = {-fwd.y, fwd.x};
    Vector2 move = {fwd.x * in.y + right.x * in.x, fwd.y * in.y + right.y * in.x};
    float mlen = sqrtf(move.x * move.x + move.y * move.y);
    bool moving = mlen > 0.01f;
    float speed = kMoveSpeed * ((IsKeyDown(KEY_LEFT_SHIFT) && !demo_) ? 1.45f : 1.0f);
    if (p.attackT >= 0 && p.onGround) speed *= 0.55f;
    if (moving) {
        move.x /= mlen;
        move.y /= mlen;
        p.pos.x += move.x * speed * dt;
        p.pos.z += move.y * speed * dt;
    }

    float camFacing = cam_.yaw + PI;
    if (p.aimHold > 0) p.yaw = ApproachAngle(p.yaw, camFacing, 20.0f * dt);
    else if (moving) p.yaw = ApproachAngle(p.yaw, atan2f(move.x, move.y), 12.0f * dt);

    p.walkAmt = Approach(p.walkAmt, (moving && p.onGround) ? 1.0f : 0.0f, dt * 6.0f);
    if (moving && p.onGround) p.walkPhase += dt * 11.0f * (speed / kMoveSpeed);

    // --- Jumping & gravity -------------------------------------------------------
    if (!demo_ && IsKeyPressed(KEY_SPACE) && p.jumpsLeft > 0) {
        p.velY = (p.jumpsLeft == 2) ? kJumpVel : kJumpVel * 0.9f;
        p.jumpsLeft--;
        p.onGround = false;
        Audio::Play(Sfx::Jump, p.jumpsLeft == 0 ? 1.25f : 1.0f, 0.6f);
    }
    p.velY -= kGravity * dt;
    p.pos.y += p.velY * dt;
    level_.ResolveCollision(p.pos, kFigRadius);
    for (const auto& n : npcs_) {  // do not walk through other minifigs
        if (!n.alive || fabsf(n.pos.y - p.pos.y) > 1.5f) continue;
        float d = HDist(p.pos, n.pos);
        if (d < 0.8f && d > 1e-4f) {
            p.pos.x = n.pos.x + (p.pos.x - n.pos.x) / d * 0.8f;
            p.pos.z = n.pos.z + (p.pos.z - n.pos.z) / d * 0.8f;
        }
    }
    float ground = level_.GroundHeight(p.pos.x, p.pos.z, p.pos.y + 0.3f);
    if (p.pos.y <= ground && p.velY <= 0) {
        if (!p.onGround && p.velY < -12.0f) fx_.SpawnSparks(V3(p.pos.x, ground + 0.1f, p.pos.z), Color{230, 200, 150, 255}, 6, 2.0f);
        p.pos.y = ground;
        p.velY = 0;
        p.onGround = true;
        p.jumpsLeft = 2;
    } else if (p.pos.y > ground + 0.05f && p.onGround) {
        p.onGround = false;  // walked off an edge
        p.jumpsLeft = 1;
    }

    // --- Attacks -------------------------------------------------------------------
    bool fire = !demo_ && CursorLocked() && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    bool firePressed = !demo_ && CursorLocked() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    p.aimHold = std::max(0.0f, p.aimHold - dt);
    p.fireCd = std::max(0.0f, p.fireCd - dt);

    if (p.weapon == Weapon::Sword) {
        p.ignite = Approach(p.ignite, 1.0f, dt * 3.5f);
        p.blocking = !demo_ && CursorLocked() && IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && p.attackT < 0;
        if (firePressed && (p.attackT < 0 || p.attackT > kSwingTime * 0.7f)) {
            p.attackT = 0;
            p.swingStyle = (p.swingStyle + 1) % 3;
            p.swingId++;
            Audio::Play(Sfx::Swing, Randf(0.9f, 1.1f));
            // Lock on to the closest enemy in front of us, brawler style.
            float best = 3.8f;
            for (const auto& n : npcs_) {
                if (!n.alive || n.kind == NpcKind::Seated || n.kind == NpcKind::Musician) continue;
                float d = HDist(p.pos, n.pos);
                if (d < best && fabsf(WrapAngle(YawTo(p.pos, n.pos) - p.yaw)) < 1.6f) { best = d; p.yaw = YawTo(p.pos, n.pos); }
            }
        }
        if (p.attackT >= 0) {
            p.attackT += dt;
            float t = p.attackT / kSwingTime;
            if (t > 0.25f && t < 0.75f) {
                for (auto& n : npcs_) {
                    if (!n.alive || n.lastSwing == p.swingId) continue;
                    float d = HDist(p.pos, n.pos);
                    float da = fabsf(WrapAngle(YawTo(p.pos, n.pos) - p.yaw));
                    if (d < 2.4f && da < 1.4f && fabsf(n.pos.y - p.pos.y) < 1.8f) {
                        n.lastSwing = p.swingId;
                        Vector3 dir = Vector3Normalize(V3(n.pos.x - p.pos.x, 0.0f, n.pos.z - p.pos.z));
                        fx_.SpawnSparks(V3(n.pos.x, n.pos.y + 1.4f, n.pos.z), SwordColor(p.swordColor), 14, 6.0f);
                        HitNpc(n, 3, dir);
                    }
                }
                for (auto& c : crates_) {
                    if (!c.alive || c.lastSwing == p.swingId) continue;
                    float d = HDist(p.pos, c.pos);
                    float da = fabsf(WrapAngle(YawTo(p.pos, c.pos) - p.yaw));
                    if (d < 2.3f && da < 1.5f && fabsf(c.pos.y - p.pos.y) < 1.6f) {
                        c.lastSwing = p.swingId;
                        BreakCrate(c, Vector3Normalize(V3(c.pos.x - p.pos.x, 0.0f, c.pos.z - p.pos.z)));
                    }
                }
            }
            if (p.attackT > kSwingTime) p.attackT = -1;
        }
    } else if (fire && p.fireCd <= 0) {
        p.fireCd = 0.2f;
        p.aimHold = 0.7f;
        p.yaw = camFacing;
        Vector3 camFwd = Vector3Normalize(Vector3Subtract(cam_.cam.target, cam_.cam.position));
        Vector3 aim = Vector3Add(cam_.cam.position, Vector3Scale(camFwd, 40.0f));
        float bestDot = cosf(9.0f * DEG2RAD);
        for (const auto& n : npcs_) {  // gentle auto-aim
            if (!n.alive) continue;
            Vector3 chest = V3(n.pos.x, n.pos.y + 1.3f, n.pos.z);
            Vector3 to = Vector3Normalize(Vector3Subtract(chest, cam_.cam.position));
            float dot = Vector3DotProduct(to, camFwd);
            if (dot > bestDot && Vector3Distance(chest, p.pos) < 30.0f) { bestDot = dot; aim = chest; }
        }
        Vector3 muzzle = p.hasHand ? BlasterMuzzle(p.hand)
                                   : Vector3Add(p.pos, V3(sinf(p.yaw) * 0.8f, 1.4f, cosf(p.yaw) * 0.8f));
        FireBolt(muzzle, Vector3Subtract(aim, muzzle), false, kPlayerBolt, 34.0f);
        fx_.SpawnSparks(muzzle, kPlayerBolt, 4, 2.0f);
        Audio::Play(Sfx::Blaster, Randf(0.95f, 1.05f), 0.7f);
    }
}

void Game::UpdateCamera(float dt) {
    if (!demo_) {
        if (IsKeyPressed(KEY_TAB)) {
            cursorLocked_ = !cursorLocked_;
            if (cursorLocked_) DisableCursor(); else EnableCursor();
        }
        if (!cursorLocked_ && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            cursorLocked_ = true;
            DisableCursor();
        }
        if (cursorLocked_) {
            Vector2 md = GetMouseDelta();
            cam_.yaw -= md.x * 0.0032f;
            cam_.pitch = Clamp(cam_.pitch + md.y * 0.0032f, -0.25f, 1.25f);
        }
        cam_.dist = Clamp(cam_.dist - GetMouseWheelMove() * 0.8f, 3.5f, 13.0f);
    }

    Vector3 goal = V3(player_.pos.x, player_.pos.y + 1.7f, player_.pos.z);
    cam_.target = Vector3Lerp(cam_.target, goal, std::min(1.0f, dt * 12.0f));
    Vector3 dir = {sinf(cam_.yaw) * cosf(cam_.pitch), sinf(cam_.pitch), cosf(cam_.yaw) * cosf(cam_.pitch)};
    float allowed = cam_.dist;
    for (float d = 0.4f; d <= cam_.dist; d += 0.15f) {
        if (level_.BlocksPoint(Vector3Add(cam_.target, Vector3Scale(dir, d)))) {
            allowed = std::max(0.8f, d - 0.35f);
            break;
        }
    }
    cam_.curDist = (allowed < cam_.curDist) ? allowed : Lerp(cam_.curDist, allowed, std::min(1.0f, dt * 4.0f));
    cam_.cam.position = Vector3Add(cam_.target, Vector3Scale(dir, cam_.curDist));
    cam_.cam.target = cam_.target;
    cam_.cam.up = V3(0, 1, 0);
    cam_.cam.fovy = 55.0f;
    cam_.cam.projection = CAMERA_PERSPECTIVE;
}

void Game::UpdateNpcs(float dt) {
    const Player& p = player_;
    Vector3 playerChest = V3(p.pos.x, p.pos.y + 1.3f, p.pos.z);

    for (auto& n : npcs_) {
        n.flash = std::max(0.0f, n.flash - dt);
        if (!n.alive) {
            if (n.respawnT > 0) {
                n.respawnT -= dt;
                if (n.respawnT <= 0) {
                    n.alive = true;
                    n.hp = 1;
                    n.pos = n.home;
                    n.yaw = n.homeYaw;
                    fx_.SpawnSparks(V3(n.pos.x, n.pos.y + 1.2f, n.pos.z), Color{255, 230, 120, 255}, 20, 3.0f);
                }
            }
            continue;
        }

        Vector2 move = {0, 0};
        float speed = 0;
        switch (n.kind) {
            case NpcKind::Trooper: {
                float dist = HDist(n.pos, p.pos);
                n.losT -= dt;
                if (n.losT <= 0) {
                    n.losT = 0.2f;
                    n.los = p.alive && dist < 24.0f && level_.LineOfSight(V3(n.pos.x, n.pos.y + 1.5f, n.pos.z), playerChest);
                }
                float desiredYaw = n.yaw;
                speed = 3.0f;
                if (n.los) {
                    desiredYaw = YawTo(n.pos, p.pos);
                    Vector2 to = {(p.pos.x - n.pos.x) / std::max(dist, 0.01f), (p.pos.z - n.pos.z) / std::max(dist, 0.01f)};
                    float advance = dist > 8.0f ? 1.0f : (dist < 4.5f ? -0.8f : 0.0f);
                    float strafe = sinf(time_ * 0.8f + n.seed) > 0 ? 0.55f : -0.55f;
                    move = {to.x * advance - to.y * strafe, to.y * advance + to.x * strafe};
                    if (demo_) move = {0, 0};
                    n.shootT -= dt;
                    if (n.shootT <= 0 && dist < 20.0f && fabsf(WrapAngle(desiredYaw - n.yaw)) < 0.3f) {
                        n.shootT = Randf(1.1f, 2.4f);
                        Vector3 muzzle = n.hasHand ? BlasterMuzzle(n.hand) : V3(n.pos.x, n.pos.y + 1.4f, n.pos.z);
                        Vector3 aim = Vector3Add(playerChest, V3(Randf(-0.6f, 0.6f), Randf(-0.4f, 0.4f), Randf(-0.6f, 0.6f)));
                        FireBolt(muzzle, Vector3Subtract(aim, muzzle), true, kEnemyBolt, 19.0f);
                        Audio::Play(Sfx::EnemyBlaster, Randf(0.9f, 1.1f), Clamp(1.2f - dist / 25.0f, 0.2f, 0.8f));
                    }
                } else {
                    // Navigate: go through the doorway if the player is on the other side of the wall.
                    Vector2 goal = {p.pos.x, p.pos.z};
                    bool nIn = level_.Inside(n.pos.x, n.pos.z), pIn = level_.Inside(p.pos.x, p.pos.z);
                    if (!p.alive) goal = {n.home.x, n.home.z - 6.0f};
                    else if (nIn != pIn) {
                        Vector2 outside = {0, level_.R + 2.5f}, inside = {0, level_.R - 2.5f};
                        bool nearDoor = fabsf(n.pos.x) < 1.4f && fabsf(n.pos.z - level_.R) < 3.0f;
                        if (nIn) goal = nearDoor ? outside : inside;
                        else goal = nearDoor ? inside : outside;
                    }
                    Vector2 d = {goal.x - n.pos.x, goal.y - n.pos.z};
                    float l = sqrtf(d.x * d.x + d.y * d.y);
                    if (l > 0.5f) {
                        move = {d.x / l, d.y / l};
                        desiredYaw = atan2f(move.x, move.y);
                    }
                    speed = 3.6f;
                }
                n.yaw = ApproachAngle(n.yaw, desiredYaw, 6.0f * dt);
                break;
            }
            case NpcKind::Patron: {
                n.thinkT -= dt;
                Vector2 d = {n.target.x - n.pos.x, n.target.y - n.pos.z};
                float l = sqrtf(d.x * d.x + d.y * d.y);
                if (n.thinkT <= 0 || l < 0.4f) {
                    for (int tries = 0; tries < 10; tries++) {
                        float a = Randf(0, 2 * PI), r = Randf(5.0f, 13.0f);
                        Vector3 c = V3(sinf(a) * r, 0.5f, cosf(a) * r);
                        if (!level_.BlocksPoint(c)) { n.target = {c.x, c.z}; break; }
                    }
                    n.thinkT = Randf(4.0f, 9.0f);
                } else if (n.thinkT < 6.0f) {  // walk for a while, then stand around
                    move = {d.x / l, d.y / l};
                    speed = 1.5f;
                    n.yaw = ApproachAngle(n.yaw, atan2f(move.x, move.y), 4.0f * dt);
                }
                break;
            }
            case NpcKind::Bartender: {
                float a = time_ * 0.25f;
                Vector3 goal = V3(sinf(a) * 2.2f, 0, cosf(a) * 2.2f);
                n.pos.x = goal.x;
                n.pos.z = goal.z;
                n.yaw = a;  // faces outwards towards the customers
                break;
            }
            case NpcKind::Musician:
                n.pos.y = n.home.y + fabsf(sinf(time_ * 4.0f + n.seed)) * 0.08f;
                break;
            case NpcKind::Seated:
                break;
        }

        if (speed > 0 && (move.x != 0 || move.y != 0)) {
            n.pos.x += move.x * speed * dt;
            n.pos.z += move.y * speed * dt;
            n.walkAmt = Approach(n.walkAmt, 1.0f, dt * 5.0f);
            n.walkPhase += dt * 11.0f * speed / kMoveSpeed;
        } else {
            n.walkAmt = Approach(n.walkAmt, 0.0f, dt * 5.0f);
        }

        if (n.kind == NpcKind::Trooper || n.kind == NpcKind::Patron) {
            n.velY -= kGravity * dt;
            n.pos.y += n.velY * dt;
            float g = level_.GroundHeight(n.pos.x, n.pos.z, n.pos.y + 0.3f);
            if (n.pos.y <= g) { n.pos.y = g; n.velY = 0; }
            level_.ResolveCollision(n.pos, kFigRadius);
            for (auto& o : npcs_) {  // keep a little personal space
                if (&o == &n || !o.alive) continue;
                float d = HDist(n.pos, o.pos);
                if (d < 0.85f && d > 1e-4f && fabsf(o.pos.y - n.pos.y) < 1.5f) {
                    n.pos.x = o.pos.x + (n.pos.x - o.pos.x) / d * 0.85f;
                    n.pos.z = o.pos.z + (n.pos.z - o.pos.z) / d * 0.85f;
                }
            }
            if (p.alive) {
                float d = HDist(n.pos, p.pos);
                if (d < 0.8f && d > 1e-4f && fabsf(p.pos.y - n.pos.y) < 1.5f) {
                    n.pos.x = p.pos.x + (n.pos.x - p.pos.x) / d * 0.8f;
                    n.pos.z = p.pos.z + (n.pos.z - p.pos.z) / d * 0.8f;
                }
            }
        }
    }
    // Remove troopers that are gone for good.
    npcs_.erase(std::remove_if(npcs_.begin(), npcs_.end(), [](const Npc& n) { return !n.alive && n.respawnT < 0; }),
                npcs_.end());
}

void Game::UpdateBolts(float dt) {
    Player& p = player_;
    Vector3 chest = V3(p.pos.x, p.pos.y + 1.3f, p.pos.z);
    bool canDeflect = p.alive && p.weapon == Weapon::Sword && p.ignite > 0.9f && (p.attackT >= 0 || p.blocking);

    for (auto& b : bolts_) {
        if (!b.alive) continue;
        Vector3 prev = b.pos;
        b.pos = Vector3Add(b.pos, Vector3Scale(b.vel, dt));
        b.life -= dt;
        if (b.life <= 0) { b.alive = false; continue; }

        // Sample along the segment so fast bolts don't tunnel through thin things.
        bool hit = false;
        for (int s = 1; s <= 3 && !hit; s++) {
            Vector3 q = Vector3Lerp(prev, b.pos, s / 3.0f);
            if (level_.BlocksPoint(q)) {
                fx_.SpawnSparks(q, b.color, 8, 3.0f);
                hit = true;
                break;
            }
            if (b.hostile) {
                if (canDeflect && Vector3Distance(q, chest) < 1.6f) {
                    // Reflect towards the nearest trooper (or straight back).
                    Vector3 dir = Vector3Negate(b.vel);
                    float best = 30.0f;
                    for (const auto& n : npcs_) {
                        if (!n.alive || n.kind != NpcKind::Trooper) continue;
                        float d = Vector3Distance(n.pos, p.pos);
                        if (d < best) { best = d; dir = Vector3Subtract(V3(n.pos.x, n.pos.y + 1.3f, n.pos.z), q); }
                    }
                    b.vel = Vector3Scale(Vector3Normalize(dir), 30.0f);
                    b.hostile = false;
                    b.color = SwordColor(p.swordColor);
                    b.life = 2.0f;
                    b.pos = q;
                    fx_.SpawnSparks(q, b.color, 12, 5.0f);
                    Audio::Play(Sfx::Deflect, Randf(0.9f, 1.2f), 0.8f);
                    break;
                }
                if (p.alive && HDist(q, p.pos) < 0.45f && q.y > p.pos.y + 0.1f && q.y < p.pos.y + 2.3f) {
                    HurtPlayer(Vector3Normalize(V3(b.vel.x, 0, b.vel.z)));
                    hit = true;
                }
            } else {
                for (auto& n : npcs_) {
                    if (!n.alive) continue;
                    if (HDist(q, n.pos) < 0.45f && q.y > n.pos.y + 0.1f && q.y < n.pos.y + 2.3f) {
                        HitNpc(n, 1, Vector3Normalize(V3(b.vel.x, 0, b.vel.z)));
                        hit = true;
                        break;
                    }
                }
                for (auto& c : crates_) {
                    if (hit || !c.alive) continue;
                    if (HDist(q, c.pos) < 0.55f && q.y > c.pos.y && q.y < c.pos.y + 0.95f) {
                        BreakCrate(c, Vector3Normalize(V3(b.vel.x, 0, b.vel.z)));
                        hit = true;
                    }
                }
            }
        }
        if (hit) b.alive = false;
    }
    bolts_.erase(std::remove_if(bolts_.begin(), bolts_.end(), [](const Bolt& b) { return !b.alive; }), bolts_.end());
}

void Game::UpdateCrates(float dt) {
    std::vector<Obstacle> dyn;
    for (auto& c : crates_) {
        if (!c.alive) {
            c.respawnT -= dt;
            if (c.respawnT <= 0 && HDist(c.pos, player_.pos) > 2.0f) {
                c.alive = true;
                fx_.SpawnSparks(Vector3Add(c.pos, V3(0, 0.5f, 0)), Color{255, 230, 120, 255}, 12, 2.5f);
            }
            continue;
        }
        // A crate stacked on another one falls if the lower one is gone.
        if (c.pos.y > 0.01f) {
            bool supported = false;
            for (const auto& o : crates_)
                if (&o != &c && o.alive && HDist(o.pos, c.pos) < 0.6f && o.pos.y < c.pos.y - 0.5f) supported = true;
            if (!supported) c.pos.y = std::max(0.0f, c.pos.y - dt * 6.0f);
        }
        dyn.push_back({Obstacle::Box, {c.pos.x, c.pos.z}, 0, 0, {0.45f, 0.45f}, c.pos.y + 0.9f});
    }
    level_.SetDynamicObstacles(dyn);
}

void Game::UpdateWaves(float dt) {
    if (demo_) return;
    int troopers = 0;
    for (const auto& n : npcs_)
        if (n.kind == NpcKind::Trooper && n.alive) troopers++;
    if (troopers == 0) {
        if (waveT_ > 50) {  // just cleared a wave
            waveT_ = 4.0f;
            if (wave_ > 0) Say(TextFormat("Wave %d cleared! More troopers incoming...", wave_));
        }
        waveT_ -= dt;
        if (waveT_ <= 0) {
            SpawnWave();
            waveT_ = 999.0f;
        }
    }
}

void Game::Update(float dt) {
    time_ += dt;
    messageT_ = std::max(0.0f, messageT_ - dt);
    studPop_ = std::max(0.0f, studPop_ - dt * 3.0f);

    if (!demo_) {
        if (IsKeyPressed(KEY_H)) showHelp_ = !showHelp_;
    } else {
        // Scripted moments for the screenshot scene.
        int f = frame_;
        if (f == opt_.screenshotFrame - 22) {
            for (auto& n : npcs_)
                if (n.kind == NpcKind::Trooper && n.alive) { HitNpc(n, 5, V3(-0.3f, 0, -1)); break; }
        }
        if (f == opt_.screenshotFrame - 40) BreakCrate(crates_[4], V3(0, 0, 1));
        if (f == opt_.screenshotFrame - 9) {
            player_.attackT = 0;
            player_.swingStyle = 1;
            player_.swingId++;
        }
    }

    UpdateCamera(dt);
    UpdatePlayer(dt);
    UpdateCrates(dt);
    UpdateNpcs(dt);
    UpdateBolts(dt);
    UpdateWaves(dt);

    CollectResult got = fx_.Update(dt, level_, player_.pos, player_.alive);
    if (got.count > 0) {
        studs_ += got.value;
        studPop_ = 1.0f;
        Audio::Play(Sfx::Stud, Randf(0.95f, 1.15f), 0.5f);
    }
}

void Game::Draw() {
    BeginDrawing();
    ClearBackground(kSky);
    BeginMode3D(cam_.cam);
    R::ApplyFrameUniforms(cam_.cam.position);

    level_.Draw(time_);

    // Crates: a stack of 2x2 bricks with studs on top.
    for (const auto& c : crates_) {
        if (!c.alive) continue;
        Matrix I = MatrixIdentity();
        for (int layer = 0; layer < 3; layer++)
            Cube(Box(I, V3(c.pos.x, c.pos.y + 0.15f + layer * 0.3f, c.pos.z), V3(0.9f, 0.29f, 0.9f)),
                 layer % 2 ? Shade(c.color, 0.88f) : c.color);
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sz = -1; sz <= 1; sz += 2)
                Cylinder(Cyl(I, V3(c.pos.x + sx * 0.22f, c.pos.y + 0.9f, c.pos.z + sz * 0.22f), 0.13f, 0.09f), c.color);
        BlobShadow(V3(c.pos.x, 0, c.pos.z), 0.75f);
    }

    // NPCs
    for (auto& n : npcs_) {
        if (!n.alive) continue;
        MinifigPose pose;
        pose.walkPhase = n.walkPhase;
        pose.walkAmt = n.walkAmt;
        switch (n.kind) {
            case NpcKind::Trooper:
                pose.rArmPitch = n.los ? -PI / 2 + 0.1f : -0.3f;
                pose.rArmSwing = n.los ? 0.0f : 0.5f;
                pose.lArmPitch = n.los ? -1.0f : 0.0f;
                pose.lArmSwing = n.los ? 0.0f : 1.0f;
                break;
            case NpcKind::Seated:
                pose.seated = true;
                pose.rArmPitch = -0.7f - 0.7f * std::max(0.0f, sinf(time_ * 0.6f + n.seed));
                pose.lArmPitch = -0.6f;
                pose.headYaw = 0.3f * sinf(time_ * 0.3f + n.seed);
                break;
            case NpcKind::Musician:
                pose.rArmPitch = -1.2f;
                pose.lArmPitch = -1.0f;
                pose.headYaw = 0.2f * sinf(time_ * 2.0f + n.seed);
                break;
            case NpcKind::Bartender:
                pose.rArmPitch = -0.9f + 0.3f * sinf(time_ * 6.0f);
                pose.rArmYaw = 0.4f * sinf(time_ * 6.0f);
                break;
            case NpcKind::Patron:
                break;
        }
        float flash = n.flash > 0 ? 0.7f : 0.0f;
        MinifigFrame f = (n.kind == NpcKind::Trooper && trooperModel_.Loaded())
                             ? trooperModel_.Draw(pose, n.pos, n.yaw, flash)
                             : DrawMinifig(n.look, pose, n.pos, n.yaw, flash);
        n.hand = f.rightHand;
        n.hasHand = true;
        if (n.kind == NpcKind::Trooper) DrawBlaster(f.rightHand, (int)n.seed % 3 == 0);
        if (n.kind == NpcKind::Seated) DrawDrink(f.rightHand, n.drink);
        if (n.kind == NpcKind::Musician) DrawHorn(f.rightHand);
        if (n.kind != NpcKind::Seated)
            BlobShadow(V3(n.pos.x, level_.GroundHeight(n.pos.x, n.pos.z, n.pos.y + 0.1f), n.pos.z), 0.6f);
    }

    // Player
    if (player_.alive) {
        bool blink = player_.invuln > 0 && fmodf(time_ * 12.0f, 2.0f) < 0.7f;
        if (!blink) {
            MinifigFrame f = DrawMinifig(HeroLook(), PlayerPose(), player_.pos, player_.yaw, player_.hurtFlash > 0 ? 0.6f : 0.0f);
            player_.hand = f.rightHand;
            player_.hasHand = true;
            if (player_.weapon == Weapon::Sword) DrawLaserSword(f.rightHand, SwordColor(player_.swordColor), player_.ignite, time_);
            else DrawBlaster(f.rightHand, false);
        }
        float g = level_.GroundHeight(player_.pos.x, player_.pos.z, player_.pos.y + 0.1f);
        BlobShadow(V3(player_.pos.x, g, player_.pos.z), 0.65f, Clamp(1.0f - (player_.pos.y - g) * 0.3f, 0.3f, 1.0f));
    }

    for (const auto& b : bolts_) DrawBolt(b);
    fx_.Draw(time_);
    R::FlushTransparent();
    EndMode3D();

    DrawHud();

    if (opt_.screenshotPath && frame_ == opt_.screenshotFrame) {
        rlDrawRenderBatchActive();  // make sure the HUD is in the framebuffer
        Image img = LoadImageFromScreen();
        ExportImage(img, opt_.screenshotPath);
        UnloadImage(img);
        TraceLog(LOG_INFO, "Screenshot saved to %s", opt_.screenshotPath);
    }
    EndDrawing();
}

void Game::DrawHud() {
    int w = GetScreenWidth(), h = GetScreenHeight();

    // Damage vignette
    if (player_.hurtFlash > 0) DrawRectangle(0, 0, w, h, Color{255, 0, 0, (unsigned char)(player_.hurtFlash * 180)});

    // Hearts
    for (int i = 0; i < kMaxHearts; i++) {
        float x = 34.0f + i * 44.0f, y = 34.0f;
        DrawHeart(x + 2, y + 2, 15, Color{0, 0, 0, 90});
        DrawHeart(x, y, 15, i < player_.hearts ? Color{230, 40, 50, 255} : Color{90, 70, 70, 160});
        if (i < player_.hearts) DrawCircleV(Vector2{x - 8, y - 3}, 3.5f, Color{255, 170, 170, 200});
    }

    // Stud counter
    const char* studText = TextFormat("%d", studs_);
    int size = 34 + (int)(studPop_ * 10);
    int tw = MeasureText(studText, size);
    float cx = w * 0.5f - (tw + 40) * 0.5f;
    DrawStudIcon(cx + 14, 34, 15 + studPop_ * 3);
    DrawTextShadow(studText, (int)cx + 38, 34 - size / 2, size, Color{255, 245, 210, 255});

    // Wave info
    int troopers = 0;
    for (const auto& n : npcs_)
        if (n.kind == NpcKind::Trooper && n.alive) troopers++;
    const char* waveText = TextFormat("Wave %d   Troopers: %d   Smashed: %d", std::max(wave_, 1), troopers, kills_);
    DrawTextShadow(waveText, w - MeasureText(waveText, 20) - 20, 24, 20, RAYWHITE);

    // Weapon panel
    const char* weaponText = player_.weapon == Weapon::Sword
                                 ? TextFormat("Laser sword (%s)", SwordColorName(player_.swordColor))
                                 : "Blaster";
    Color wc = player_.weapon == Weapon::Sword ? SwordColor(player_.swordColor) : Color{255, 150, 70, 255};
    int ww = MeasureText(weaponText, 22);
    DrawRectangleRounded(Rectangle{(float)(w - ww - 44), (float)(h - 56), (float)(ww + 28), 38}, 0.4f, 6, Color{0, 0, 0, 120});
    DrawRectangle(w - ww - 36, h - 46, 6, 18, wc);
    DrawText(weaponText, w - ww - 24, h - 47, 22, RAYWHITE);

    // Crosshair for the blaster
    if (player_.weapon == Weapon::Blaster && player_.alive) {
        int cxh = w / 2, cyh = h / 2;
        DrawCircleLines(cxh, cyh, 9, Color{255, 255, 255, 200});
        DrawCircle(cxh, cyh, 2, Color{255, 140, 60, 230});
    }

    // Controls help
    if (showHelp_) {
        const char* lines[] = {
            "WASD  move     Mouse  look     Space  jump / double jump",
            "LMB  attack    RMB  block (sword)    Shift  run",
            "1 / 2 / Q  switch weapon    F  sword color",
            "Wheel  zoom    Tab  free mouse    H  hide help    Esc  quit",
        };
        int lh = 20, ph = 4 * lh + 16;
        DrawRectangleRounded(Rectangle{12, (float)(h - ph - 12), 560, (float)ph}, 0.15f, 6, Color{0, 0, 0, 120});
        for (int i = 0; i < 4; i++) DrawText(lines[i], 24, h - ph - 4 + i * lh, 16, Color{240, 230, 210, 255});
    }

    // Messages
    if (messageT_ > 0) {
        float a = std::min(1.0f, messageT_);
        int mw = MeasureText(message_.c_str(), 26);
        DrawTextShadow(message_.c_str(), w / 2 - mw / 2, 82, 26, Color{255, 230, 160, (unsigned char)(255 * a)});
    }

    // Title card
    if (!demo_ && time_ < 4.5f) {
        float a = time_ < 3.0f ? 1.0f : 1.0f - (time_ - 3.0f) / 1.5f;
        const char* title = "BRICK WARS: CANTINA BRAWL";
        int tsz = 54;
        int ttw = MeasureText(title, tsz);
        DrawTextShadow(title, w / 2 - ttw / 2, h / 2 - 90, tsz, Color{255, 220, 90, (unsigned char)(255 * a)});
        const char* sub = "Smash the troopers, collect the studs!";
        int sw = MeasureText(sub, 24);
        DrawTextShadow(sub, w / 2 - sw / 2, h / 2 - 28, 24, Color{255, 245, 230, (unsigned char)(255 * a)});
    }

    if (!cursorLocked_ && !demo_) {
        const char* t = "Click to capture the mouse";
        DrawTextShadow(t, w / 2 - MeasureText(t, 20) / 2, h / 2 + 40, 20, RAYWHITE);
    }
}

int Game::Run() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(opt_.width, opt_.height, "Brick Wars: Cantina Brawl");
    SetTargetFPS(60);
    Audio::Init();
    R::Init();
    if (!opt_.proceduralTroopers) trooperModel_.Load();
    Init();
    if (opt_.screenshotPath) SetupDemoScene();
    else {
        DisableCursor();
        SetExitKey(KEY_ESCAPE);
    }

    while (!WindowShouldClose()) {
        float dt = opt_.screenshotPath ? kFixedDt : std::min(GetFrameTime(), 1.0f / 30.0f);
        Update(dt);
        Draw();
        if (opt_.screenshotPath && frame_ >= opt_.screenshotFrame) break;
        frame_++;
    }

    trooperModel_.Unload();
    R::Shutdown();
    Audio::Shutdown();
    CloseWindow();
    return 0;
}

}  // namespace

int RunGame(const GameOptions& options) {
    Game game(options);
    return game.Run();
}
