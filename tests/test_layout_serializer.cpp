#include <catch2/catch_test_macros.hpp>
#include "core/LayoutSerializer.h"
#include "core/DockSystem.h"

using namespace FluentUI;

TEST_CASE("LayoutSerializer DockTree serialize/deserialize roundtrip", "[serializer]") {
    // Build a simple split tree: left panel | right panel
    auto left = DockNode::MakeLeaf("panel_left");
    auto right = DockNode::MakeLeaf("panel_right");
    auto root = DockNode::MakeSplit(true, 0.4f, std::move(left), std::move(right));

    std::string serialized = LayoutSerializer::SerializeDockTree(root.get(), "dock");
    REQUIRE_FALSE(serialized.empty());

    // Parse the serialized string into a key-value map
    std::unordered_map<std::string, std::string> data;
    std::istringstream iss(serialized);
    std::string line;
    while (std::getline(iss, line)) {
        auto eq = line.find('=');
        if (eq != std::string::npos) {
            data[line.substr(0, eq)] = line.substr(eq + 1);
        }
    }

    auto restored = LayoutSerializer::DeserializeDockTree(data, "dock");
    REQUIRE(restored != nullptr);
    REQUIRE(restored->type == DockNode::Type::Split);
    REQUIRE(restored->splitVertical == true);

    // Verify children
    REQUIRE(restored->first != nullptr);
    REQUIRE(restored->second != nullptr);

    // Collect panel IDs
    std::vector<std::string> ids;
    restored->CollectPanelIds(ids);
    REQUIRE(ids.size() == 2);

    bool hasLeft = false, hasRight = false;
    for (auto& id : ids) {
        if (id == "panel_left") hasLeft = true;
        if (id == "panel_right") hasRight = true;
    }
    REQUIRE(hasLeft);
    REQUIRE(hasRight);
}

TEST_CASE("LayoutSerializer handles tab groups", "[serializer]") {
    auto tabNode = DockNode::MakeTab({"tab_a", "tab_b", "tab_c"});

    std::string serialized = LayoutSerializer::SerializeDockTree(tabNode.get());
    REQUIRE_FALSE(serialized.empty());

    std::unordered_map<std::string, std::string> data;
    std::istringstream iss(serialized);
    std::string line;
    while (std::getline(iss, line)) {
        auto eq = line.find('=');
        if (eq != std::string::npos) {
            data[line.substr(0, eq)] = line.substr(eq + 1);
        }
    }

    auto restored = LayoutSerializer::DeserializeDockTree(data);
    REQUIRE(restored != nullptr);

    std::vector<std::string> ids;
    restored->CollectPanelIds(ids);
    REQUIRE(ids.size() == 3);
}

TEST_CASE("LayoutSerializer empty tree", "[serializer]") {
    std::string serialized = LayoutSerializer::SerializeDockTree(nullptr);
    REQUIRE(serialized.empty());
}
