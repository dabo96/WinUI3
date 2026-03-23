#include <catch2/catch_test_macros.hpp>
#include "core/ShortcutRegistry.h"

using namespace FluentUI;

TEST_CASE("ShortcutRegistry register and has", "[shortcuts]") {
    ShortcutRegistry reg;
    KeyCombo combo{SDL_SCANCODE_S, MOD_CTRL};

    bool called = false;
    reg.Register("save", combo, [&]() { called = true; });

    REQUIRE(reg.HasShortcut("save"));
    REQUIRE_FALSE(reg.HasShortcut("open"));
}

TEST_CASE("ShortcutRegistry unregister", "[shortcuts]") {
    ShortcutRegistry reg;
    reg.Register("save", {SDL_SCANCODE_S, MOD_CTRL}, []() {});
    REQUIRE(reg.HasShortcut("save"));

    reg.Unregister("save");
    REQUIRE_FALSE(reg.HasShortcut("save"));
}

TEST_CASE("ShortcutRegistry GetShortcutText", "[shortcuts]") {
    ShortcutRegistry reg;
    reg.Register("save", {SDL_SCANCODE_S, MOD_CTRL}, []() {});

    std::string text = reg.GetShortcutText("save");
    REQUIRE_FALSE(text.empty());
}

TEST_CASE("ShortcutRegistry ComboToString", "[shortcuts]") {
    KeyCombo combo{SDL_SCANCODE_S, MOD_CTRL};
    std::string text = ShortcutRegistry::ComboToString(combo);
    REQUIRE_FALSE(text.empty());
    // Should contain "Ctrl" and "S"
    REQUIRE(text.find("Ctrl") != std::string::npos);
}

TEST_CASE("ShortcutRegistry ComboToString with multiple modifiers", "[shortcuts]") {
    KeyCombo combo{SDL_SCANCODE_Z, static_cast<uint16_t>(MOD_CTRL | MOD_SHIFT)};
    std::string text = ShortcutRegistry::ComboToString(combo);
    REQUIRE(text.find("Ctrl") != std::string::npos);
    REQUIRE(text.find("Shift") != std::string::npos);
}

TEST_CASE("KeyCombo equality", "[shortcuts]") {
    KeyCombo a{SDL_SCANCODE_A, MOD_CTRL};
    KeyCombo b{SDL_SCANCODE_A, MOD_CTRL};
    KeyCombo c{SDL_SCANCODE_B, MOD_CTRL};
    KeyCombo d{SDL_SCANCODE_A, MOD_SHIFT};

    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
    REQUIRE_FALSE(a == d);
}
