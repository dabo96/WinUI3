#pragma once
#include "Math/Vec2.h"
#include <cstring>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

namespace FluentUI {

class UIContext;

/// Phase D: Typed drag-drop API. Use as RAII objects scoped to a single widget
/// region. Both source and target inspect the global UIContext::dragDrop state.
///
/// Source pattern:
///   if (button("Drag me")) { ... }
///   if (DragDropSource src("FILE_PATH"); src.IsActive()) {
///     src.SetPayload(filePath);
///     src.DragPreview([&]{ Label("📄 " + filePath); });
///   }
///
/// Target pattern:
///   if (DragDropTarget tgt; tgt.IsHovering()) {
///     std::string payload;
///     if (tgt.AcceptPayload("FILE_PATH", &payload)) { /* handle drop */ }
///   }
class DragDropSource {
public:
    /// @param payloadType  User-defined type tag identifying drop semantics.
    explicit DragDropSource(const std::string &payloadType);
    ~DragDropSource();

    /// True while the user holds the mouse and dragged this widget past threshold.
    bool IsActive() const;

    /// Stamp the payload bytes for this drag (POD-copy semantics).
    template <typename T>
    void SetPayload(const T &data) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Drag-drop payload must be trivially copyable; use raw bytes for std::string variant");
        SetPayloadBytes(reinterpret_cast<const uint8_t *>(&data), sizeof(T));
    }

    /// Specialised stamping for std::string (length-prefixed bytes).
    void SetPayload(const std::string &s);

    /// Provide a callback drawing the floating preview under the cursor.
    /// Captures by value; lifetime managed by the source until drag ends.
    void DragPreview(std::function<void()> draw);

private:
    UIContext *ctx;
    bool ownedActivation = false;
    void SetPayloadBytes(const uint8_t *data, size_t bytes);
};

class DragDropTarget {
public:
    DragDropTarget();
    ~DragDropTarget();

    /// True while a drag is hovering this widget's region this frame.
    bool IsHovering() const;

    /// On mouse-up over this target, copy the payload into *out and clear the drag.
    /// @return true the single frame the payload is delivered.
    template <typename T>
    bool AcceptPayload(const std::string &payloadType, T *out) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Drag-drop payload must be trivially copyable");
        const uint8_t *data = nullptr; size_t size = 0;
        if (!AcceptRaw(payloadType, &data, &size)) return false;
        if (size != sizeof(T)) return false;
        std::memcpy(out, data, sizeof(T));
        return true;
    }

    /// std::string overload (length-prefixed payload).
    bool AcceptPayload(const std::string &payloadType, std::string *out);

private:
    UIContext *ctx;
    Vec2 regionPos{0, 0};
    Vec2 regionSize{0, 0};
    bool AcceptRaw(const std::string &payloadType, const uint8_t **out, size_t *outSize);
public:
    /// Set the hover region rect (defaults to the current lastItem rect).
    void SetRegion(const Vec2 &pos, const Vec2 &size);
};

} // namespace FluentUI
