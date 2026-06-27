#pragma once

namespace FluentUI {
namespace Demo {

/// Phase H1: Comprehensive demo window showcasing every widget in real use.
/// Call once per frame from your root build callback. Pass an external bool*
/// to hook the close button.
void ShowDemoWindow(bool* open = nullptr);

/// About-FluentUI window with version, build info and credits.
void ShowAboutWindow(bool* open = nullptr);

/// Performance metrics window (frame time graph, draw call counts, perfCounters).
void ShowMetricsWindow(bool* open = nullptr);

/// Phase H2: Live style editor — edit Style fields and preview in real time.
void ShowStyleEditor(bool* open = nullptr);

/// Phase H3: Item picker / inspector — Ctrl+Shift+P to activate.
/// Highlights any widget under the cursor and displays its rect / id stack.
void ShowItemPicker(bool* open = nullptr);

} // namespace Demo
} // namespace FluentUI
