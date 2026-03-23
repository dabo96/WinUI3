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
