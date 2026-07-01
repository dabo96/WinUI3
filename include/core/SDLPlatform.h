#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// INTERNAL platform header (brief 20). This is the ONE header allowed to expose
// SDL — it is the SDL side of the PlatformBackend seam. Public headers stay
// SDL-free; only the SDL driver (FluentApp) and SDL-owning .cpp include this.
// (F-c will grow this into a full PlatformBackend implementation.)
// ─────────────────────────────────────────────────────────────────────────────
#include <SDL3/SDL.h>
#include "core/UIEvent.h"
#include "core/UIKey.h"

namespace FluentUI {

class InputState;

// Map an SDL scancode to the platform-neutral UIKey (inverse of the mapping used
// by widgets). Returns UIKey::Unknown for keys outside the neutral set.
UIKey UIKeyFromScancode(SDL_Scancode sc);

// Translate a single SDL_Event into a neutral UIEvent. Returns false when the
// event produces no UIEvent (e.g. gamepad-added, unhandled types). This is the
// SDL→UIEvent seam that will live in SDLPlatform::PollEvent (F-c).
bool TranslateSDLEvent(const SDL_Event& e, UIEvent& out);

// Convenience: translate `e` and feed it to `input` (neutral ProcessEvent). Used
// by the SDL-driven loop (FluentApp) during migration; replaces the old
// InputState::ProcessEvent(SDL_Event) shim.
void ProcessSDLEvent(InputState& input, const SDL_Event& e);

} // namespace FluentUI
