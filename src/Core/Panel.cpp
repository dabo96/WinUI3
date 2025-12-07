#include "core/Panel.h"
#include <algorithm>

namespace FluentUI {

    void Panel::AddChild(Element* child) {
        if (child) {
            children.push_back(child);
        }
    }

    void Panel::RemoveChild(Element* child) {
        auto it = std::find(children.begin(), children.end(), child);
        if (it != children.end()) {
            children.erase(it);
        }
    }

} // namespace FluentUI

