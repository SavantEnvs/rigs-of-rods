#pragma once
namespace ror_fuzz {
void Init();   // create OGRE's LogManager (silent) once
void Reset();  // drop per-input state kept by RoR::Console
}
