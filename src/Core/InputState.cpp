#include "core/InputState.h"
#include <string>
#include <iostream>

using namespace FluentUI;

struct InputState::TextInputData {
    std::string buffer;
};

InputState::InputState()
    : textInputData(std::make_unique<TextInputData>())
{
}

InputState::~InputState() = default;

InputState::InputState(InputState&& other) noexcept = default;
InputState& InputState::operator=(InputState&& other) noexcept = default;

const std::string& InputState::TextInputBuffer() const
{
    static const std::string empty;
    return textInputData ? textInputData->buffer : empty;
}

void InputState::Update(SDL_Window* window)
{
    keysPressed.fill(false);
    keysReleased.fill(false);
    mousePressed.fill(false);
    mouseReleased.fill(false);
    mouseWheelX = mouseWheelY = 0.0f;
    if (textInputData) {
        textInputData->buffer.clear();
    }
    anyKeyPressed = false;

    // Clear deltas at the start of the frame (before processing events)
    mouseDX = 0.0f;
    mouseDY = 0.0f;

    // We rely on SDL_EVENT_MOUSE_MOTION for accurate deltas including sub-pixel or relative modes
    // But we still update absolute position here mainly for initial state or lost events
    if (window) {
        float mouseXPos, mouseYPos;
        SDL_GetMouseState(&mouseXPos, &mouseYPos);
        // Only update if we haven't processed events yet (to avoid jitter) 
        // or just let ProcessEvent handle it.
        // Let's keep absolute pos sync but NOT delta calculation here to avoid conflict.
        // We will trust ProcessEvent for deltas.
    }
}

void InputState::ProcessEvent(const SDL_Event& e)
{
    switch (e.type)
    {
        //
    case SDL_EVENT_KEY_DOWN:
        if (e.key.scancode < SDL_SCANCODE_COUNT)
        {
            keysDown[e.key.scancode] = true;
            keysPressed[e.key.scancode] = true;
            anyKeyPressed = true;
        }
        break;

    case SDL_EVENT_KEY_UP:
        if (e.key.scancode < SDL_SCANCODE_COUNT)
        {
            keysDown[e.key.scancode] = false;
            keysReleased[e.key.scancode] = true;
        }
        break;

    case SDL_EVENT_TEXT_INPUT:
        if (textInputData) {
            textInputData->buffer += e.text.text;
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button > 0)
        {
            int idx = static_cast<int>(e.button.button) - 1;
            std::cout << "[ProcessEvent] MouseDown: Button " << (int)e.button.button << " -> Index " << idx << std::endl;
            if (idx >= 0 && idx < static_cast<int>(mouseDown.size()))
            {
                mouseX = static_cast<float>(e.button.x);
                mouseY = static_cast<float>(e.button.y);
                mouseDown[idx] = true;
                mousePressed[idx] = true;
                std::cout << "[ProcessEvent] Set mousePressed[" << idx << "] = true" << std::endl;
            } else {
                 std::cout << "[ProcessEvent] Index out of bounds! Size: " << mouseDown.size() << std::endl;
            }
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button > 0)
        {
            int idx = static_cast<int>(e.button.button) - 1;
            if (idx >= 0 && idx < static_cast<int>(mouseDown.size()))
            {
                mouseDown[idx] = false;
                mouseReleased[idx] = true;
            }
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        mouseX = static_cast<float>(e.motion.x);
        mouseY = static_cast<float>(e.motion.y);
        mouseDX += static_cast<float>(e.motion.xrel);
        mouseDY += static_cast<float>(e.motion.yrel);
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        mouseWheelX += static_cast<float>(e.wheel.x);
        mouseWheelY += static_cast<float>(e.wheel.y);
        break;

    case SDL_EVENT_QUIT:
        // Puedes manejar aquí un flag global de salida si lo deseas
        break;

    default:
        break;
    }
}
