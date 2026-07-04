#include <RaidenUI/Core/Signal.hpp>

#include <vector>

namespace RaidenUI {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
thread_local TrackingContext g_tracking;

int Batcher::s_depth = 0;

void Batcher::begin() { ++s_depth; }

void Batcher::end() {
  if (--s_depth == 0) {
    flushPendingEffects();
  }
}

bool Batcher::isActive() { return s_depth > 0; }

namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<Effect *> s_pending;
} // namespace

void scheduleEffect(Effect *effect) {
  if (Batcher::isActive()) {
    s_pending.push_back(effect);
  } else {
    effect->fn();
    effect->scheduled = false;
  }
}

void flushPendingEffects() {
  for (auto *effect : s_pending) {
    effect->fn();
    effect->scheduled = false;
  }
  s_pending.clear();
}

void createEffect(std::function<void()> fn) {
  auto *effect =
      new Effect{.fn = std::move(fn), .dependencies = {}, .scheduled = false};

  TrackingContext prev = g_tracking;
  g_tracking.current = effect;

  effect->fn();

  g_tracking = prev;

  // TODO:
  // effect ownership transferred to the reactive system.
  // disposal should be handled by the owning scope (DOM node, component).
}

} // namespace RaidenUI
