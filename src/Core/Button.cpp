#include "core/Button.h"

namespace FluentUI {

Button *Button::Create(const std::string &label, const Vec2 &pos,
                       const Vec2 &size) {
  Button *btn = new Button();
  btn->text = label;
  btn->position = pos;
  btn->size = size;
  return btn;
}

} // namespace FluentUI
