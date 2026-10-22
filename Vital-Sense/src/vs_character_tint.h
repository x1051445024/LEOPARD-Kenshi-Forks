#pragma once

#include "vs_runtime_state.h"
#include <ogre/OgreColourValue.h>

class Character;

namespace vs_character_tint
{

void TickKoCharacterTintRuntime(RuntimeStateView& state);
void ResetKoCharacterTintRuntime(RuntimeStateView& state);
void SyncKoCharacterTint(RuntimeStateView& state, const char* pluginName);
void ClearKoCharacterTint(RuntimeStateView& state, const char* pluginName);

// LEOPARD (22.10.2026): our tint off for the portrait render of this character, then back.
bool SuspendCharacterTintForPortrait(RuntimeStateView& state, Character* character, Ogre::ColourValue& colourOut, bool& depthOut, const char* pluginName);
void ResumeCharacterTintAfterPortrait(Character* character, const Ogre::ColourValue& colour, bool depth, const char* pluginName);

} // namespace vs_character_tint
