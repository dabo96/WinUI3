#pragma once
#ifndef __cplusplus
#error "InputState requires C++ compilation"
#endif

#include <memory>
#include <array>
#include <string>
#include <vector>
#include "core/UIKey.h"
#include "core/UIEvent.h" // UIEvent, WindowHandle (brief 20; no SDL in this header)

namespace FluentUI {
class InputState {
public:
  // Update once per frame; `window` (opaque WindowHandle) is used to refresh the
  // mouse position for the focused window. Implementation casts it to SDL_Window*.
  void Update(WindowHandle window = nullptr);
  // Feed a platform-neutral event (brief 20). The SDL→UIEvent translation lives
  // in SDLPlatform (ProcessSDLEvent), so this core stays SDL-free.
  void ProcessEvent(const UIEvent& e);

  // brief 20: platform-neutral key queries. Key state is stored indexed by UIKey,
  // so widgets never see SDL scancodes.
  bool IsKeyDown(UIKey key) const;
  bool IsKeyPressed(UIKey key) const;
  bool IsKeyReleased(UIKey key) const;

  // brief 20 Part B: modifier state from tracked key state (no SDL_GetModState in
  // widgets). Each is true if either left/right modifier is currently held.
  bool CtrlDown() const;
  bool ShiftDown() const;
  bool AltDown() const;

  bool IsMouseDown(int button) const { return mouseDown[button]; }
  bool IsMousePressed(int button) const { return mousePressed[button]; }
  bool IsMouseReleased(int button) const { return mouseReleased[button]; }

  float MouseX() const { return mouseX; }
  float MouseY() const { return mouseY; }
  float MouseDeltaX() const { return mouseDX; }
  float MouseDeltaY() const { return mouseDY; }
  float MouseWheelX() const { return mouseWheelX; }
  float MouseWheelY() const { return mouseWheelY; }

  InputState();

  ~InputState();
  InputState(const InputState&) = delete;
  InputState& operator=(const InputState&) = delete;
  InputState(InputState&&) noexcept;
  InputState& operator=(InputState&&) noexcept;

  const std::string& TextInputBuffer() const;

  // IME composition accessors
  const std::string& CompositionText() const;
  int CompositionCursor() const;
  int CompositionLength() const;
  bool HasComposition() const;

  // File drop support
  const std::vector<std::string>& DroppedFiles() const { return droppedFiles; }
  bool HasDroppedFiles() const { return !droppedFiles.empty(); }

  // brief 18.7: OS drag-and-drop — text payloads and the drop position (window
  // coordinates, same space as MouseX/MouseY). dropX/dropY are valid the frame a
  // file/text drop arrives. osDragActive is true between DROP_BEGIN and
  // DROP_COMPLETE so widgets can highlight a drop target during the drag.
  const std::string& DroppedText() const { return droppedText; }
  bool HasDroppedText() const { return !droppedText.empty(); }
  float DropX() const { return dropX; }
  float DropY() const { return dropY; }
  bool OSDragActive() const { return osDragActive; }

  // OS clipboard helpers (platform-centralized). UTF-8 in/out.
  // Brief 18.1: reusable by TextInput/SelectableText/PasswordBox/NumberBox.
  void SetClipboardText(const std::string& utf8);  // SDL_SetClipboardText
  std::string GetClipboardText();                   // SDL_GetClipboardText (frees with SDL_free)
  bool HasClipboardText();                           // SDL_HasClipboardText

public:
  bool anyKeyPressed = false;

private:
  struct TextInputData;

  // Key state indexed by UIKey (brief 20). Sized to the neutral key set.
  std::array<bool, static_cast<size_t>(UIKey::Count)> keysDown{};
  std::array<bool, static_cast<size_t>(UIKey::Count)> keysPressed{};
  std::array<bool, static_cast<size_t>(UIKey::Count)> keysReleased{};

  std::array<bool, 5> mouseDown{};
  std::array<bool, 5> mousePressed{};
  std::array<bool, 5> mouseReleased{};

  float mouseX = -1000.0f, mouseY = -1000.0f; // Inicializar fuera de la ventana
  float prevMouseX = -1000.0f, prevMouseY = -1000.0f;
  float mouseDX = 0.0f, mouseDY = 0.0f;
  float mouseWheelX = 0.0f, mouseWheelY = 0.0f;

  std::unique_ptr<TextInputData> textInputData;

  // File drop state (cleared each frame in Update())
  std::vector<std::string> droppedFiles;
  // brief 18.7: OS drag-drop extras. droppedText/dropX/dropY cleared each frame;
  // osDragActive persists across frames (toggled by DROP_BEGIN/DROP_COMPLETE).
  std::string droppedText;
  float dropX = 0.0f, dropY = 0.0f;
  bool osDragActive = false;
};
} // namespace FluentUI
