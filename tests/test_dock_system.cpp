#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/DockSystem.h"

using namespace FluentUI;
using Catch::Matchers::WithinAbs;

// ============================================================================
// DockNode Tests
// ============================================================================

TEST_CASE("DockNode MakeLeaf", "[dock]") {
    auto node = DockNode::MakeLeaf("panel1");
    REQUIRE(node != nullptr);
    REQUIRE(node->type == DockNode::Type::Leaf);
    REQUIRE(node->panelId == "panel1");
}

TEST_CASE("DockNode MakeTab", "[dock]") {
    auto node = DockNode::MakeTab({"a", "b", "c"});
    REQUIRE(node != nullptr);
    REQUIRE(node->type == DockNode::Type::Tab);
    REQUIRE(node->panelIds.size() == 3);
    REQUIRE(node->activeTabIndex == 0);
}

TEST_CASE("DockNode MakeSplit", "[dock]") {
    auto left = DockNode::MakeLeaf("left");
    auto right = DockNode::MakeLeaf("right");
    auto split = DockNode::MakeSplit(true, 0.3f, std::move(left), std::move(right));

    REQUIRE(split->type == DockNode::Type::Split);
    REQUIRE(split->splitVertical == true);
    REQUIRE_THAT(split->splitRatio, WithinAbs(0.3f, 1e-5));
    REQUIRE(split->first != nullptr);
    REQUIRE(split->second != nullptr);
}

TEST_CASE("DockNode FindPanel", "[dock]") {
    auto left = DockNode::MakeLeaf("panel_a");
    auto right = DockNode::MakeLeaf("panel_b");
    auto root = DockNode::MakeSplit(true, 0.5f, std::move(left), std::move(right));

    REQUIRE(root->FindPanel("panel_a") != nullptr);
    REQUIRE(root->FindPanel("panel_b") != nullptr);
    REQUIRE(root->FindPanel("panel_c") == nullptr);
}

TEST_CASE("DockNode RemovePanel", "[dock]") {
    auto left = DockNode::MakeLeaf("panel_a");
    auto right = DockNode::MakeLeaf("panel_b");
    auto root = DockNode::MakeSplit(true, 0.5f, std::move(left), std::move(right));

    // RemovePanel returns false for Split because it promotes the remaining child
    root->RemovePanel("panel_a");
    REQUIRE(root->FindPanel("panel_a") == nullptr);
    // panel_b should still be there (promoted)
    REQUIRE(root->FindPanel("panel_b") != nullptr);
}

TEST_CASE("DockNode RemovePanel from leaf returns true", "[dock]") {
    auto leaf = DockNode::MakeLeaf("only");
    REQUIRE(leaf->RemovePanel("only"));
    REQUIRE(leaf->type == DockNode::Type::Empty);
}

TEST_CASE("DockNode CollectPanelIds", "[dock]") {
    auto left = DockNode::MakeLeaf("a");
    auto right = DockNode::MakeTab({"b", "c"});
    auto root = DockNode::MakeSplit(false, 0.5f, std::move(left), std::move(right));

    std::vector<std::string> ids;
    root->CollectPanelIds(ids);
    REQUIRE(ids.size() == 3);
}

// ============================================================================
// DockSpace Tests
// ============================================================================

TEST_CASE("DockSpace starts empty", "[dock]") {
    DockSpace space;
    REQUIRE(space.IsEmpty());
    REQUIRE(space.GetDockedPanels().empty());
}

TEST_CASE("DockSpace dock and undock", "[dock]") {
    DockSpace space;

    space.DockPanel("panel1", DockPosition::Center);
    REQUIRE_FALSE(space.IsEmpty());
    REQUIRE(space.IsPanelDocked("panel1"));

    space.DockPanel("panel2", DockPosition::Right, "panel1");
    REQUIRE(space.IsPanelDocked("panel2"));
    REQUIRE(space.GetDockedPanels().size() == 2);

    space.UndockPanel("panel1");
    REQUIRE_FALSE(space.IsPanelDocked("panel1"));
    REQUIRE(space.IsPanelDocked("panel2"));
}

TEST_CASE("DockSpace ComputeLayout", "[dock]") {
    DockSpace space;
    space.DockPanel("left", DockPosition::Center);
    space.DockPanel("right", DockPosition::Right, "left");

    Rect area(0, 0, 800, 600);
    space.ComputeLayout(area);

    Rect leftBounds = space.GetPanelBounds("left");
    Rect rightBounds = space.GetPanelBounds("right");

    // Both panels should have non-zero size
    REQUIRE_FALSE(leftBounds.IsEmpty());
    REQUIRE_FALSE(rightBounds.IsEmpty());

    // They shouldn't overlap significantly
    REQUIRE(leftBounds.Right() <= rightBounds.Left() + DockSpace::DIVIDER_THICKNESS + 1.0f);
}

TEST_CASE("DockSpace multiple dock positions", "[dock]") {
    DockSpace space;
    space.DockPanel("center", DockPosition::Center);
    space.DockPanel("left", DockPosition::Left, "center");
    space.DockPanel("bottom", DockPosition::Bottom, "center");

    REQUIRE(space.GetDockedPanels().size() == 3);

    Rect area(0, 0, 1000, 800);
    space.ComputeLayout(area);

    REQUIRE_FALSE(space.GetPanelBounds("center").IsEmpty());
    REQUIRE_FALSE(space.GetPanelBounds("left").IsEmpty());
    REQUIRE_FALSE(space.GetPanelBounds("bottom").IsEmpty());
}

TEST_CASE("DockSpace SetRoot", "[dock]") {
    DockSpace space;
    auto root = DockNode::MakeLeaf("custom");
    space.SetRoot(std::move(root));

    REQUIRE_FALSE(space.IsEmpty());
    REQUIRE(space.IsPanelDocked("custom"));
}
