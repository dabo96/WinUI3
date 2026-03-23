#include "core/LayoutSerializer.h"
#include "core/Context.h"
#include <algorithm>

namespace FluentUI {

bool LayoutSerializer::SaveLayout(const std::string& filepath, const DockSpace& dockSpace,
                                   UIContext* ctx) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        Log(LogLevel::Error, "LayoutSerializer: Cannot open file for writing: %s", filepath.c_str());
        return false;
    }

    file << "# FluentUI Layout File\n";
    file << "version=1\n\n";

    // Save dock layout
    if (dockSpace.Root()) {
        file << "# Dock Layout\n";
        WriteDockNode(file, dockSpace.Root(), "dock");
        file << "\n";
    }

    // Save panel states
    if (ctx) {
        file << "# Panel States\n";
        for (auto& [id, state] : ctx->panelStates) {
            std::string prefix = "panel." + std::to_string(id);
            file << prefix << ".x=" << state.position.x << "\n";
            file << prefix << ".y=" << state.position.y << "\n";
            file << prefix << ".w=" << state.size.x << "\n";
            file << prefix << ".h=" << state.size.y << "\n";
            file << prefix << ".minimized=" << (state.minimized ? 1 : 0) << "\n";
        }
        file << "\n";

        // Save splitter ratios
        file << "# Splitter States\n";
        for (auto& [id, state] : ctx->splitterStates) {
            file << "splitter." << id << ".ratio=" << state.ratio << "\n";
        }
        file << "\n";

        // Save tab view active tabs
        file << "# TabView States\n";
        for (auto& [id, state] : ctx->tabViewStates) {
            file << "tabview." << id << ".activeTab=" << state.activeTab << "\n";
        }
    }

    return true;
}

bool LayoutSerializer::LoadLayout(const std::string& filepath, DockSpace& dockSpace,
                                   UIContext* ctx) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false; // File doesn't exist yet — not an error
    }

    std::unordered_map<std::string, std::string> data;
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        data[key] = value;
    }

    // Load dock layout
    if (data.count("dock.type")) {
        auto root = ReadDockNode(data, "dock");
        if (root) {
            dockSpace.SetRoot(std::move(root));
        }
    }

    // Load panel states
    if (ctx) {
        for (auto& [key, value] : data) {
            if (key.substr(0, 6) == "panel.") {
                // Parse: panel.<id>.<prop>=<value>
                auto rest = key.substr(6);
                auto dot = rest.find('.');
                if (dot == std::string::npos) continue;
                uint32_t id = static_cast<uint32_t>(std::stoul(rest.substr(0, dot)));
                std::string prop = rest.substr(dot + 1);
                auto& state = ctx->panelStates[id];

                if (prop == "x") state.position.x = std::stof(value);
                else if (prop == "y") state.position.y = std::stof(value);
                else if (prop == "w") state.size.x = std::stof(value);
                else if (prop == "h") state.size.y = std::stof(value);
                else if (prop == "minimized") state.minimized = (value == "1");
            }
            else if (key.substr(0, 9) == "splitter.") {
                auto rest = key.substr(9);
                auto dot = rest.find('.');
                if (dot == std::string::npos) continue;
                uint32_t id = static_cast<uint32_t>(std::stoul(rest.substr(0, dot)));
                std::string prop = rest.substr(dot + 1);
                if (prop == "ratio") {
                    ctx->splitterStates[id].ratio = std::stof(value);
                }
            }
            else if (key.substr(0, 8) == "tabview.") {
                auto rest = key.substr(8);
                auto dot = rest.find('.');
                if (dot == std::string::npos) continue;
                uint32_t id = static_cast<uint32_t>(std::stoul(rest.substr(0, dot)));
                std::string prop = rest.substr(dot + 1);
                if (prop == "activeTab") {
                    ctx->tabViewStates[id].activeTab = std::stoi(value);
                }
            }
        }
    }

    return true;
}

void LayoutSerializer::WriteDockNode(std::ostream& out, const DockNode* node,
                                      const std::string& prefix) {
    if (!node) return;

    switch (node->type) {
        case DockNode::Type::Empty:
            out << prefix << ".type=empty\n";
            break;
        case DockNode::Type::Leaf:
            out << prefix << ".type=leaf\n";
            out << prefix << ".panel=" << node->panelId << "\n";
            break;
        case DockNode::Type::Tab:
            out << prefix << ".type=tab\n";
            out << prefix << ".count=" << node->panelIds.size() << "\n";
            out << prefix << ".activeTab=" << node->activeTabIndex << "\n";
            for (size_t i = 0; i < node->panelIds.size(); ++i) {
                out << prefix << ".panel." << i << "=" << node->panelIds[i] << "\n";
            }
            break;
        case DockNode::Type::Split:
            out << prefix << ".type=split\n";
            out << prefix << ".vertical=" << (node->splitVertical ? 1 : 0) << "\n";
            out << prefix << ".ratio=" << node->splitRatio << "\n";
            WriteDockNode(out, node->first.get(), prefix + ".first");
            WriteDockNode(out, node->second.get(), prefix + ".second");
            break;
    }
}

std::unique_ptr<DockNode> LayoutSerializer::ReadDockNode(
    const std::unordered_map<std::string, std::string>& data,
    const std::string& prefix) {

    auto it = data.find(prefix + ".type");
    if (it == data.end()) return nullptr;

    const std::string& type = it->second;

    if (type == "empty") return nullptr;

    if (type == "leaf") {
        auto panelIt = data.find(prefix + ".panel");
        if (panelIt == data.end()) return nullptr;
        return DockNode::MakeLeaf(panelIt->second);
    }

    if (type == "tab") {
        auto countIt = data.find(prefix + ".count");
        if (countIt == data.end()) return nullptr;
        int count = std::stoi(countIt->second);
        std::vector<std::string> ids;
        for (int i = 0; i < count; ++i) {
            auto pid = data.find(prefix + ".panel." + std::to_string(i));
            if (pid != data.end()) ids.push_back(pid->second);
        }
        if (ids.empty()) return nullptr;
        auto node = DockNode::MakeTab(ids);
        auto activeIt = data.find(prefix + ".activeTab");
        if (activeIt != data.end()) node->activeTabIndex = std::stoi(activeIt->second);
        return node;
    }

    if (type == "split") {
        auto vertIt = data.find(prefix + ".vertical");
        auto ratioIt = data.find(prefix + ".ratio");
        bool vertical = vertIt != data.end() && vertIt->second == "1";
        float ratio = ratioIt != data.end() ? std::stof(ratioIt->second) : 0.5f;

        auto first = ReadDockNode(data, prefix + ".first");
        auto second = ReadDockNode(data, prefix + ".second");

        if (!first && !second) return nullptr;
        if (!first) return second;
        if (!second) return first;

        return DockNode::MakeSplit(vertical, ratio, std::move(first), std::move(second));
    }

    return nullptr;
}

std::string LayoutSerializer::SerializeDockTree(const DockNode* node, const std::string& prefix) {
    std::ostringstream out;
    WriteDockNode(out, node, prefix);
    return out.str();
}

std::unique_ptr<DockNode> LayoutSerializer::DeserializeDockTree(
    const std::unordered_map<std::string, std::string>& data,
    const std::string& prefix) {
    return ReadDockNode(data, prefix);
}

} // namespace FluentUI
