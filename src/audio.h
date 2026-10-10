// Procedurally synthesized sound effects (no audio files needed).
#pragma once

enum class Sfx { Blaster, EnemyBlaster, Swing, Ignite, Break, Stud, Hurt, Jump, Deflect, Count };

namespace Audio {
void Init();
void Shutdown();
void Play(Sfx sfx, float pitch = 1.0f, float volume = 1.0f);
}  // namespace Audio
