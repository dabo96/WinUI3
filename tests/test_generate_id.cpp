#include <catch2/catch_test_macros.hpp>
#include "UI/WidgetHelpers.h"
#include <set>
#include <string>

using namespace FluentUI;

TEST_CASE("GenerateId produces consistent results", "[id]") {
    uint32_t a = GenerateId("test_widget");
    uint32_t b = GenerateId("test_widget");
    REQUIRE(a == b);
}

TEST_CASE("GenerateId different strings produce different IDs", "[id]") {
    uint32_t a = GenerateId("button_1");
    uint32_t b = GenerateId("button_2");
    REQUIRE(a != b);
}

TEST_CASE("GenerateId two-part overload", "[id]") {
    uint32_t a = GenerateId("PREFIX:", "widget1");
    uint32_t b = GenerateId("PREFIX:", "widget2");
    REQUIRE(a != b);

    uint32_t c = GenerateId("PREFIX:", "widget1");
    REQUIRE(a == c);
}

TEST_CASE("GenerateId three-part overload", "[id]") {
    uint32_t a = GenerateId("A", "B", "C");
    uint32_t b = GenerateId("A", "B", "D");
    REQUIRE(a != b);
}

TEST_CASE("GenerateId no collisions for common widget names", "[id]") {
    std::set<uint32_t> ids;
    const char* names[] = {
        "button_ok", "button_cancel", "button_apply",
        "label_title", "label_subtitle",
        "input_name", "input_email", "input_password",
        "panel_main", "panel_sidebar", "panel_footer",
        "checkbox_agree", "slider_volume", "combo_theme",
        "tab_general", "tab_advanced", "tab_about",
        "tree_files", "list_items", "menu_file",
    };

    for (const auto& name : names) {
        uint32_t id = GenerateId(name);
        REQUIRE(ids.find(id) == ids.end());
        ids.insert(id);
    }
}

TEST_CASE("GenerateId prefix collision avoidance", "[id]") {
    // Ensure "BUTTON:ok" != "PANEL:ok"
    uint32_t a = GenerateId("BUTTON:", "ok");
    uint32_t b = GenerateId("PANEL:", "ok");
    REQUIRE(a != b);
}

// brief 21: with no active context (empty ID scope stack) GenerateId must still
// produce the exact legacy djb2 hash (seed 5381), so IDs persisted across frames
// by pre-brief-21 code stay byte-identical.
static uint32_t LegacyDjb2(const char* s) {
    uint32_t h = 5381;
    int c;
    while ((c = *s++)) h = ((h << 5) + h) + c;
    return h;
}

TEST_CASE("GenerateId byte-compatible with legacy djb2 (empty scope)", "[id][brief21]") {
    REQUIRE(GenerateId("test_widget") == LegacyDjb2("test_widget"));
    REQUIRE(GenerateId("panel_main") == LegacyDjb2("panel_main"));
    // Two-part: equivalent to djb2 of the concatenation.
    REQUIRE(GenerateId("PANEL:", "main") == LegacyDjb2("PANEL:main"));
    // Three-part: equivalent to djb2 of the full concatenation.
    REQUIRE(GenerateId("A", "B", "C") == LegacyDjb2("ABC"));
}

// brief 21: PushID/PopID are no-ops without an active context, so GenerateId is
// unchanged here (state is owned by UIContext). Scoping behaviour with a live
// context is covered in test_widgets_comprehensive.cpp.
TEST_CASE("PushID/PopID without context leave GenerateId unchanged", "[id][brief21]") {
    uint32_t base = GenerateId("widget");
    PushID("scope");          // no-op: no context
    REQUIRE(GenerateId("widget") == base);
    PopID();                  // no-op
    REQUIRE(GenerateId("widget") == base);
}
