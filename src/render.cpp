#include "render.h"

#include "rlgl.h"

#include <utility>
#include <vector>

namespace {

const char* kVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragPos;
out vec3 fragNormal;
out vec2 fragTexCoord;
void main() {
    fragTexCoord = vertexTexCoord;
    fragPos = vec3(matModel * vec4(vertexPosition, 1.0));
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* kFragmentShader = R"(#version 330
in vec3 fragPos;
in vec3 fragNormal;
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec3 sunDir;
uniform vec3 sunColor;
uniform vec3 ambientOut;
uniform vec3 ambientIn;
uniform vec3 fogColor;
uniform float fogDensity;
uniform float roomRadius;
uniform int shellMode;
uniform int lightCount;
uniform vec3 lightPos[8];
uniform vec3 lightColor[8];
out vec4 finalColor;
void main() {
    vec3 n = normalize(fragNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 v = normalize(viewPos - fragPos);

    float interior;
    if (shellMode == 1) interior = gl_FrontFacing ? 0.0 : 1.0;
    else interior = (length(fragPos.xz) < roomRadius - 0.45) ? 1.0 : 0.0;

    float sunAmt = mix(1.0, 0.38, interior);
    vec3 amb = mix(ambientOut, ambientIn, interior);
    // simple hemisphere ambient: a bit brighter from above
    amb *= 0.8 + 0.2 * n.y;

    vec3 diffuse = amb + sunColor * max(dot(n, -sunDir), 0.0) * sunAmt;
    vec3 hs = normalize(-sunDir + v);
    float spec = pow(max(dot(n, hs), 0.0), 40.0) * 0.55 * sunAmt;

    for (int i = 0; i < 8; i++) {
        if (i >= lightCount) break;
        vec3 L = lightPos[i] - fragPos;
        float d = length(L);
        L /= d;
        float att = interior / (1.0 + 0.16 * d + 0.06 * d * d);
        diffuse += lightColor[i] * max(dot(n, L), 0.0) * att;
        vec3 hl = normalize(L + v);
        spec += pow(max(dot(n, hl), 0.0), 32.0) * att * 0.7;
    }

    float rim = pow(1.0 - max(dot(n, v), 0.0), 3.0) * 0.18;
    vec3 base = texture(texture0, fragTexCoord).rgb * colDiffuse.rgb;
    vec3 col = base * diffuse + vec3(spec) + rim * amb;

    float dist = length(viewPos - fragPos);
    float f = 1.0 - exp(-pow(dist * fogDensity, 2.0));
    col = mix(col, fogColor, clamp(f, 0.0, 1.0));
    finalColor = vec4(col, colDiffuse.a);
}
)";

Shader gShader;
Material gLit;
Material gUnlit;
Mesh gCube, gCylinder, gSphere, gDome, gTorso;

int gLocViewPos, gLocSunDir, gLocSunColor, gLocAmbOut, gLocAmbIn, gLocFogColor, gLocFogDensity;
int gLocRoom, gLocShell, gLocLightCount, gLocLightPos, gLocLightColor;

Vector3 gLightPos[8];
Vector3 gLightColor[8];
int gLightCount = 0;

enum class MeshId { Cube, Cylinder, Sphere };
struct HaloCmd { MeshId mesh; Matrix m; Color c; };
std::vector<HaloCmd> gHalos;
struct ShadowCmd { Vector3 pos; float radius; float strength; };
std::vector<ShadowCmd> gShadows;

void DrawWith(Material& mat, const Mesh& mesh, const Matrix& m, Color c) {
    mat.maps[MATERIAL_MAP_DIFFUSE].color = c;
    DrawMesh(mesh, mat, m);
}

const Mesh& MeshFor(MeshId id) {
    switch (id) {
        case MeshId::Cube: return gCube;
        case MeshId::Cylinder: return gCylinder;
        default: return gSphere;
    }
}

// Trapezoid torso: bottom face 1 x 1 (x,z) at y=0, top face topScale x 1 at y=1.
Mesh MakeTorsoMesh(float topScale) {
    const float b = 0.5f, t = 0.5f * topScale, d = 0.5f;
    Vector3 p[8] = {
        {-b, 0, -d}, {b, 0, -d}, {b, 0, d}, {-b, 0, d},  // bottom
        {-t, 1, -d}, {t, 1, -d}, {t, 1, d}, {-t, 1, d},  // top
    };
    int quads[6][4] = {
        {3, 2, 6, 7},  // front (+z)
        {1, 0, 4, 5},  // back
        {2, 1, 5, 6},  // right
        {0, 3, 7, 4},  // left
        {7, 6, 5, 4},  // top
        {0, 1, 2, 3},  // bottom
    };
    Mesh mesh = {};
    mesh.vertexCount = 36;
    mesh.triangleCount = 12;
    mesh.vertices = (float*)MemAlloc(36 * 3 * sizeof(float));
    mesh.normals = (float*)MemAlloc(36 * 3 * sizeof(float));
    mesh.texcoords = (float*)MemAlloc(36 * 2 * sizeof(float));
    Vector3 center = {0, 0.5f, 0};
    int v = 0;
    for (auto& q : quads) {
        Vector3 a = p[q[0]], bb = p[q[1]], c = p[q[2]], dd = p[q[3]];
        Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(bb, a), Vector3Subtract(c, a)));
        Vector3 fc = Vector3Scale(Vector3Add(Vector3Add(a, bb), Vector3Add(c, dd)), 0.25f);
        if (Vector3DotProduct(n, Vector3Subtract(fc, center)) < 0) {  // make sure it faces outwards (CCW)
            std::swap(bb, dd);
            n = Vector3Negate(n);
        }
        Vector3 tri[6] = {a, bb, c, a, c, dd};
        for (auto& pt : tri) {
            mesh.vertices[v * 3 + 0] = pt.x;
            mesh.vertices[v * 3 + 1] = pt.y;
            mesh.vertices[v * 3 + 2] = pt.z;
            mesh.normals[v * 3 + 0] = n.x;
            mesh.normals[v * 3 + 1] = n.y;
            mesh.normals[v * 3 + 2] = n.z;
            mesh.texcoords[v * 2 + 0] = 0;
            mesh.texcoords[v * 2 + 1] = 0;
            v++;
        }
    }
    UploadMesh(&mesh, false);
    return mesh;
}

}  // namespace

namespace R {

void Init() {
    gShader = LoadShaderFromMemory(kVertexShader, kFragmentShader);
    gShader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(gShader, "matModel");
    gShader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(gShader, "matNormal");
    gLocViewPos = GetShaderLocation(gShader, "viewPos");
    gLocSunDir = GetShaderLocation(gShader, "sunDir");
    gLocSunColor = GetShaderLocation(gShader, "sunColor");
    gLocAmbOut = GetShaderLocation(gShader, "ambientOut");
    gLocAmbIn = GetShaderLocation(gShader, "ambientIn");
    gLocFogColor = GetShaderLocation(gShader, "fogColor");
    gLocFogDensity = GetShaderLocation(gShader, "fogDensity");
    gLocRoom = GetShaderLocation(gShader, "roomRadius");
    gLocShell = GetShaderLocation(gShader, "shellMode");
    gLocLightCount = GetShaderLocation(gShader, "lightCount");
    gLocLightPos = GetShaderLocation(gShader, "lightPos");
    gLocLightColor = GetShaderLocation(gShader, "lightColor");

    gLit = LoadMaterialDefault();
    gLit.shader = gShader;
    gUnlit = LoadMaterialDefault();

    gCube = GenMeshCube(1, 1, 1);
    gCylinder = GenMeshCylinder(1, 1, 20);
    gSphere = GenMeshSphere(1, 14, 18);
    gDome = GenMeshHemiSphere(1, 16, 32);
    gTorso = MakeTorsoMesh(0.7f);

    SetShellMode(false);
    SetRoom(0);
}

void Shutdown() {
    UnloadMesh(gCube);
    UnloadMesh(gCylinder);
    UnloadMesh(gSphere);
    UnloadMesh(gDome);
    UnloadMesh(gTorso);
    UnloadShader(gShader);
}

Shader LitShader() { return gShader; }

void ClearLights() { gLightCount = 0; }

void AddLight(Vector3 pos, Vector3 color) {
    if (gLightCount >= 8) return;
    gLightPos[gLightCount] = pos;
    gLightColor[gLightCount] = color;
    gLightCount++;
}

void SetSun(Vector3 dir, Vector3 color, Vector3 ambientOutside, Vector3 ambientInside) {
    dir = Vector3Normalize(dir);
    SetShaderValue(gShader, gLocSunDir, &dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(gShader, gLocSunColor, &color, SHADER_UNIFORM_VEC3);
    SetShaderValue(gShader, gLocAmbOut, &ambientOutside, SHADER_UNIFORM_VEC3);
    SetShaderValue(gShader, gLocAmbIn, &ambientInside, SHADER_UNIFORM_VEC3);
}

void SetFog(Color color, float density) {
    Vector3 c = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
    SetShaderValue(gShader, gLocFogColor, &c, SHADER_UNIFORM_VEC3);
    SetShaderValue(gShader, gLocFogDensity, &density, SHADER_UNIFORM_FLOAT);
}

void SetRoom(float radius) { SetShaderValue(gShader, gLocRoom, &radius, SHADER_UNIFORM_FLOAT); }

void SetShellMode(bool on) {
    int v = on ? 1 : 0;
    SetShaderValue(gShader, gLocShell, &v, SHADER_UNIFORM_INT);
}

void ApplyFrameUniforms(Vector3 viewPos) {
    SetShaderValue(gShader, gLocViewPos, &viewPos, SHADER_UNIFORM_VEC3);
    SetShaderValue(gShader, gLocLightCount, &gLightCount, SHADER_UNIFORM_INT);
    if (gLightCount > 0) {
        SetShaderValueV(gShader, gLocLightPos, gLightPos, SHADER_UNIFORM_VEC3, gLightCount);
        SetShaderValueV(gShader, gLocLightColor, gLightColor, SHADER_UNIFORM_VEC3, gLightCount);
    }
}

void Cube(const Matrix& m, Color c) { DrawWith(gLit, gCube, m, c); }
void Cylinder(const Matrix& m, Color c) { DrawWith(gLit, gCylinder, m, c); }
void Sphere(const Matrix& m, Color c) { DrawWith(gLit, gSphere, m, c); }
void Dome(const Matrix& m, Color c) { DrawWith(gLit, gDome, m, c); }
void Torso(const Matrix& m, Color c) { DrawWith(gLit, gTorso, m, c); }

void GlowCube(const Matrix& m, Color c) { DrawWith(gUnlit, gCube, m, c); }
void GlowCylinder(const Matrix& m, Color c) { DrawWith(gUnlit, gCylinder, m, c); }
void GlowSphere(const Matrix& m, Color c) { DrawWith(gUnlit, gSphere, m, c); }

void HaloCylinder(const Matrix& m, Color c) { gHalos.push_back({MeshId::Cylinder, m, c}); }
void HaloSphere(const Matrix& m, Color c) { gHalos.push_back({MeshId::Sphere, m, c}); }
void BlobShadow(Vector3 groundPos, float radius, float strength) {
    gShadows.push_back({groundPos, radius, strength});
}

void FlushTransparent() {
    rlDrawRenderBatchActive();
    rlDisableDepthMask();

    BeginBlendMode(BLEND_ALPHA);
    for (const auto& s : gShadows) {
        for (int i = 0; i < 3; i++) {  // three stacked discs = soft edge
            float r = s.radius * (1.0f - 0.22f * i);
            Matrix m = Cyl(MatrixIdentity(), V3(s.pos.x, s.pos.y + 0.012f + 0.002f * i, s.pos.z), r, 0.004f);
            DrawWith(gUnlit, gCylinder, m, Color{0, 0, 0, (unsigned char)(40 * s.strength)});
        }
    }
    EndBlendMode();

    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& h : gHalos) DrawWith(gUnlit, MeshFor(h.mesh), h.m, h.c);
    EndBlendMode();

    rlEnableDepthMask();
    gHalos.clear();
    gShadows.clear();
}

Matrix CylBetween(Vector3 a, Vector3 b, float radius) {
    Vector3 d = Vector3Subtract(b, a);
    float len = Vector3Length(d);
    if (len < 1e-5f) return Cyl(MatrixIdentity(), a, radius, 0.0001f);
    Vector3 dir = Vector3Scale(d, 1.0f / len);
    Vector3 up = {0, 1, 0};
    Vector3 axis = Vector3CrossProduct(up, dir);
    float s = Vector3Length(axis);
    float angle = atan2f(s, Vector3DotProduct(up, dir));
    Matrix rot = (s < 1e-5f) ? (dir.y > 0 ? MatrixIdentity() : MatrixRotateX(PI)) : MatrixRotate(Vector3Scale(axis, 1.0f / s), angle);
    return Mul(Mul(MatrixScale(radius, len, radius), rot), MatrixTranslate(a.x, a.y, a.z));
}

}  // namespace R
