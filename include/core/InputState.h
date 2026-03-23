#pragma once
#ifndef __cplusplus
#error "InputState requires C++ compilation"
#endif

#include <memory>
#include <array>
#include <string>
#include <vector>
#include <SDL3/SDL.h>

namespace FluentUI {
class InputState {
public:
  void Update(SDL_Window* window = nullptr);
  void ProcessEvent(const SDL_Event &e);

  bool IsKeyDown(SDL_Scancode sc) const { return keysDown[sc]; }
  bool IsKeyPressed(SDL_Scancode sc) const { return keysPressed[sc]; }
  bool IsKeyReleased(SDL_Scancode sc) const { return keysReleased[sc]; }

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

public:
  bool anyKeyPressed = false;

private:
  struct TextInputData;

  std::array<bool, SDL_SCANCODE_COUNT> keysDown{};
  std::array<bool, SDL_SCANCODE_COUNT> keysPressed{};
  std::array<bool, SDL_SCANCODE_COUNT> keysReleased{};

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
};
} // namespace FluentUI
