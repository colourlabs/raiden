#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace RaidenUI {

// internal effect tracking

struct Effect {
  std::function<void()> fn;
  std::vector<void *> dependencies;
  bool scheduled{false};
};

struct TrackingContext {
  Effect *current{nullptr};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local TrackingContext g_tracking;

// batcher: deferred effect execution

class Batcher {
public:
  static void begin();
  static void end();
  static bool isActive();

private:
  static int s_depth;
};

// Signal<T> : reactive value with getter/setter pair

template <typename T>
class Signal {
public:
  using Getter = std::function<T()>;
  using Setter = std::function<void(T)>;

  struct State {
    T value;
    std::vector<Effect *> subs;
  };

  explicit Signal(std::shared_ptr<State> state) : state_(std::move(state)) {}
  Signal() = default;

  Getter getter() const {
    auto state = state_;
    return [state]() -> T {
      if (g_tracking.current) {
        state->subs.push_back(g_tracking.current);
      }
      return state->value;
    };
  }

  Setter setter() const {
    auto state = state_;
    return [state](T newValue) {
      if (state->value == newValue) {
        return;
      }
      state->value = std::move(newValue);
      for (auto *effect : state->subs) {
        if (!effect->scheduled) {
          effect->scheduled = true;
          scheduleEffect(effect);
        }
      }
    };
  }

  T peek() const { return state_->value; }

private:
  std::shared_ptr<State> state_;
};

// factory functions

template <typename T>
auto createSignal(T initial) -> std::pair<typename Signal<T>::Getter,
                                          typename Signal<T>::Setter> {
  auto state = std::make_shared<typename Signal<T>::State>();
  state->value = std::move(initial);
  Signal<T> signal(state);
  return {signal.getter(), signal.setter()};
}

void createEffect(std::function<void()> fn);

template <typename T>
auto createMemo(std::function<T()> fn)
    -> std::pair<std::function<T()>, std::function<void()>> {
  struct MemoState {
    T value;
    std::vector<Effect *> subs;
    bool dirty{true};
  };
  auto memo = std::make_shared<MemoState>();
  memo->value = T{};

  auto invalidate = [memo]() { memo->dirty = true; };

  auto getter = [fn, memo]() -> T {
    if (memo->dirty) {
      memo->dirty = false;
      memo->value = fn();
    }
    if (g_tracking.current) {
      memo->subs.push_back(g_tracking.current);
    }
    return memo->value;
  };

  return {std::move(getter), std::move(invalidate)};
}

// batching

template <typename Fn>
void batch(Fn fn) {
  Batcher::begin();
  fn();
  Batcher::end();
}

// internal

void scheduleEffect(Effect *effect);
void flushPendingEffects();

} // namespace RaidenUI
