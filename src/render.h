// Tiny lit-primitive renderer on top of raylib.
// Everything in the game is built from a handful of unit meshes
// (cube, cylinder, sphere, hemisphere and a trapezoid "torso" block)
// drawn with a small plastic-looking lighting shader.
#pragma once

#include "raylib.h"
#include "raymath.h"

namespace R {

void Init();
void Shutdown();
// The lit "plastic" shader, e.g. to assign to the materials of a loaded Model.
Shader LitShader();

// Per-frame lighting setup -------------------------------------------------
void ClearLights();
void AddLight(Vector3 pos, Vector3 color);  // color may exceed 1.0 (intensity)
void SetSun(Vector3 dir, Vector3 color, Vector3 ambientOutside, Vector3 ambientInside);
void SetFog(Color color, float density);
void SetRoom(float radius);                 // interior of the domed room gets indoor light
void ApplyFrameUniforms(Vector3 viewPos);   // call once per frame after the setters
void SetShellMode(bool on);                 // two-sided interior/exterior shading (dome)

// Lit primitives. The matrix is the full world transform of the unit mesh.
//   Cube:     1x1x1 centered at the origin
//   Cylinder: radius 1, y in [0, 1]
//   Sphere:   radius 1, centered
//   Dome:     hemisphere radius 1, y in [0, 1]
//   Torso:    trapezoid block, bottom 1x1 at y=0, top 0.7x1 at y=1
void Cube(const Matrix& m, Color c);
void Cylinder(const Matrix& m, Color c);
void Sphere(const Matrix& m, Color c);
void Dome(const Matrix& m, Color c);
void Torso(const Matrix& m, Color c);

// Unlit (emissive) primitives, drawn immediately.
void GlowCube(const Matrix& m, Color c);
void GlowCylinder(const Matrix& m, Color c);
void GlowSphere(const Matrix& m, Color c);

// Additive glow halos and soft blob shadows. They are queued and drawn by
// FlushTransparent() after all opaque geometry.
void HaloCylinder(const Matrix& m, Color c);
void HaloSphere(const Matrix& m, Color c);
void BlobShadow(Vector3 groundPos, float radius, float strength = 1.0f);
void FlushTransparent();

// Matrix helpers --------------------------------------------------------------
inline Matrix Mul(const Matrix& first, const Matrix& then) { return MatrixMultiply(first, then); }

// Box of `size` centered at `center` in the parent frame.
inline Matrix Box(const Matrix& parent, Vector3 center, Vector3 size) {
    return Mul(Mul(MatrixScale(size.x, size.y, size.z), MatrixTranslate(center.x, center.y, center.z)), parent);
}
// Vertical cylinder with its base at `base` in the parent frame.
inline Matrix Cyl(const Matrix& parent, Vector3 base, float radius, float height) {
    return Mul(Mul(MatrixScale(radius, height, radius), MatrixTranslate(base.x, base.y, base.z)), parent);
}
// Sphere (optionally squashed) centered at `center`.
inline Matrix Sph(const Matrix& parent, Vector3 center, Vector3 radii) {
    return Mul(Mul(MatrixScale(radii.x, radii.y, radii.z), MatrixTranslate(center.x, center.y, center.z)), parent);
}
// Cylinder running from local point `from` along local +Z (for weapons).
inline Matrix CylZ(const Matrix& parent, Vector3 from, float radius, float length) {
    return Mul(Mul(Mul(MatrixScale(radius, length, radius), MatrixRotateX(PI / 2.0f)),
                   MatrixTranslate(from.x, from.y, from.z)), parent);
}
// Cylinder running from local point `from` along local -Y.
inline Matrix CylNegY(const Matrix& parent, Vector3 from, float radius, float length) {
    return Mul(Mul(Mul(MatrixScale(radius, length, radius), MatrixRotateX(PI)),
                   MatrixTranslate(from.x, from.y, from.z)), parent);
}
// Cylinder from world point a to world point b.
Matrix CylBetween(Vector3 a, Vector3 b, float radius);

inline Vector3 V3(float x, float y, float z) { return Vector3{x, y, z}; }
inline Color Mix(Color a, Color b, float t) {
    t = Clamp(t, 0.0f, 1.0f);
    return Color{(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                 (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}
inline Color Shade(Color c, float f) {
    return Color{(unsigned char)Clamp(c.r * f, 0.0f, 255.0f), (unsigned char)Clamp(c.g * f, 0.0f, 255.0f),
                 (unsigned char)Clamp(c.b * f, 0.0f, 255.0f), c.a};
}
inline Color WithAlpha(Color c, float a) { c.a = (unsigned char)Clamp(a * 255.0f, 0.0f, 255.0f); return c; }

}  // namespace R
