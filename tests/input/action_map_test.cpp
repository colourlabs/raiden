#include <gtest/gtest.h>
#include <Raiden/Input/ActionMap.hpp>
#include <Raiden/Platform/InputState.hpp>
#include <Raiden/Logger.hpp>

using namespace Raiden::Input;
using namespace Raiden::Platform;

class ActionMapTest : public testing::Test {
protected:
  void SetUp() override {
    Raiden::Core::Logger::setMinLogLevel(Raiden::Core::LogLevel::Critical);
  }

  static InputState stateWithKey(int scancode, bool down) {
    InputState s;
    if (scancode >= 0 && scancode < InputState::kMaxKeys) {
      s.keysDown[scancode] = down;
    }
    return s;
  }
};

TEST_F(ActionMapTest, RegisterAndFind) {
  ActionMap map;
  Action action;
  action.bindings.push_back({ActionBinding::Keyboard, 42});
  map.registerAction("jump", std::move(action));

  const Action *found = map.find("jump");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->bindings.size(), 1);
  EXPECT_EQ(found->bindings[0].code, 42);
}

TEST_F(ActionMapTest, FindNonexistentReturnsNull) {
  ActionMap map;
  EXPECT_EQ(map.find("nonexistent"), nullptr);
}

TEST_F(ActionMapTest, Contains) {
  ActionMap map;
  Action action;
  map.registerAction("move", std::move(action));
  EXPECT_TRUE(map.contains("move"));
  EXPECT_FALSE(map.contains("other"));
}

TEST_F(ActionMapTest, OperatorBracket) {
  ActionMap map;
  Action &a = map["test"];
  EXPECT_FALSE(a.pressed);
  a.pressed = true;

  Action &a2 = map["test"];
  EXPECT_TRUE(a2.pressed);
}

TEST_F(ActionMapTest, EvaluateKeyboardPress) {
  ActionMap map;
  Action action;
  action.bindings.push_back({ActionBinding::Keyboard, 42});
  map.registerAction("fire", std::move(action));

  InputState s = stateWithKey(42, true);
  map.evaluate(s);

  const Action *result = map.find("fire");
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(result->pressed);
  EXPECT_TRUE(result->justPressed);
  EXPECT_FALSE(result->justReleased);
  EXPECT_FLOAT_EQ(result->value, 1.0F);
}

TEST_F(ActionMapTest, EvaluateKeyboardRelease) {
  ActionMap map;
  Action action;
  action.bindings.push_back({ActionBinding::Keyboard, 10});
  map.registerAction("fire", std::move(action));

  // press
  map.evaluate(stateWithKey(10, true));
  // release
  map.evaluate(stateWithKey(10, false));

  const Action *result = map.find("fire");
  ASSERT_NE(result, nullptr);
  EXPECT_FALSE(result->pressed);
  EXPECT_FALSE(result->justPressed);
  EXPECT_TRUE(result->justReleased);
}

TEST_F(ActionMapTest, JustPressedOnlyOneFrame) {
  ActionMap map;
  Action action;
  action.bindings.push_back({ActionBinding::Keyboard, 1});
  map.registerAction("key", std::move(action));

  map.evaluate(stateWithKey(1, true));
  EXPECT_TRUE(map.find("key")->justPressed);

  map.evaluate(stateWithKey(1, true));
  EXPECT_FALSE(map.find("key")->justPressed);
  EXPECT_TRUE(map.find("key")->pressed);
}

TEST_F(ActionMapTest, MultipleBindingsForOneAction) {
  ActionMap map;
  Action action;
  action.bindings.push_back({ActionBinding::Keyboard, 1});
  action.bindings.push_back({ActionBinding::Keyboard, 2});
  map.registerAction("shoot", std::move(action));

  // first binding triggers
  map.evaluate(stateWithKey(1, true));
  EXPECT_TRUE(map.find("shoot")->pressed);

  // only second binding triggers
  map.evaluate(stateWithKey(2, true));
  EXPECT_TRUE(map.find("shoot")->pressed);
}

TEST_F(ActionMapTest, LoadFromString) {
  ActionMap map;
  std::string_view toml = R"(
[fire]
bindings = [
  { source = "keyboard", code = "space" }
]

[forward]
bindings = [
  { source = "keyboard", code = "w" }
]
)";

  EXPECT_TRUE(map.loadFromString(toml));
  EXPECT_TRUE(map.contains("fire"));
  EXPECT_TRUE(map.contains("forward"));

  const Action *fire = map.find("fire");
  ASSERT_NE(fire, nullptr);
  EXPECT_EQ(fire->bindings.size(), 1);
  ASSERT_FALSE(fire->bindings.empty());
  // SDL_GetScancodeFromName("space") should return a non-zero code
  EXPECT_NE(fire->bindings[0].code, 0);
}

TEST_F(ActionMapTest, NoCrashOnEmptyState) {
  ActionMap map;
  InputState s{};
  EXPECT_NO_THROW(map.evaluate(s));
}

TEST_F(ActionMapTest, MouseButtonBindings) {
  ActionMap map;
  Action action;
  action.bindings.push_back({ActionBinding::MouseButton, 0}); // left click
  map.registerAction("click", std::move(action));

  InputState s;
  s.mouseButtons[0] = true;
  map.evaluate(s);

  EXPECT_TRUE(map.find("click")->pressed);
}

TEST_F(ActionMapTest, DeadZoneFiltering) {
  ActionMap map;
  Action action;
  ActionBinding binding;
  binding.source = ActionBinding::GamepadAxis;
  binding.code = 0;   // leftx
  binding.deadZone = 0.5F;
  binding.scale = 1.0F;
  action.bindings.push_back(binding);
  map.registerAction("move_x", std::move(action));

  InputState s;
  s.gamepadConnected = true;
  s.leftStickX = 0.1F; // below deadzone

  map.evaluate(s);
  EXPECT_FALSE(map.find("move_x")->pressed);
  EXPECT_FLOAT_EQ(map.find("move_x")->value, 0.0F);
}
