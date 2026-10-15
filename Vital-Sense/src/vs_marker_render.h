#pragma once

#include "vs_runtime_state.h"

#include <mygui/MyGUI_Colour.h>

namespace vs_marker_render
{

bool EnsureProjectionUtility(RuntimeStateView& state);
void HideAllKoMarkerWidgets(RuntimeStateView& state, const char* pluginName);
void TickKoMarkerRender(RuntimeStateView& state, const char* pluginName);
// LEOPARD: цвет тира награды - тот же, что у значка $ (прозрачный при 0).
MyGUI::Colour BountyTierColour(RuntimeStateView& state, int totalBounty);

} // namespace vs_marker_render
