#include "core/InputState.h"
#include <string>

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

    prevMouseX = mouseX;
    prevMouseY = mouseY;

    // En SDL3, obtener la posición actual del mouse cada frame
    // Esto asegura que siempre tengamos la posición correcta, incluso sin eventos
    if (window) {
        float mouseXPos, mouseYPos;
        SDL_GetMouseState(&mouseXPos, &mouseYPos);
        mouseX = mouseXPos;
        mouseY = mouseYPos;
    }
    
    mouseDX = mouseX - prevMouseX;
    mouseDY = mouseY - prevMouseY;
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
            if (idx >= 0 && idx < static_cast<int>(mouseDown.size()))
            {
                mouseX = static_cast<float>(e.button.x);
                mouseY = static_cast<float>(e.button.y);
                mouseDown[idx] = true;
                mousePressed[idx] = true;
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
        // En SDL3, las coordenadas están en el evento motion
        mouseX = static_cast<float>(e.motion.x);
        mouseY = static_cast<float>(e.motion.y);
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
