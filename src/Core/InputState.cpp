#include "core/InputState.h"
#include <string>
#include <cstdlib>

using namespace FluentUI;

struct InputState::TextInputData {
    std::string buffer;
    std::string compositionText;  // IME composition string
    int compositionCursor = 0;    // Cursor position within composition
    int compositionLength = 0;    // Selection length within composition
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

const std::string& InputState::CompositionText() const
{
    static const std::string empty;
    return textInputData ? textInputData->compositionText : empty;
}

int InputState::CompositionCursor() const
{
    return textInputData ? textInputData->compositionCursor : 0;
}

int InputState::CompositionLength() const
{
    return textInputData ? textInputData->compositionLength : 0;
}

bool InputState::HasComposition() const
{
    return textInputData && !textInputData->compositionText.empty();
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
    droppedFiles.clear();
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
            // Committed text clears any active composition
            textInputData->compositionText.clear();
            textInputData->compositionCursor = 0;
            textInputData->compositionLength = 0;
        }
        break;

    case SDL_EVENT_TEXT_EDITING:
        if (textInputData) {
            textInputData->compositionText = e.edit.text ? e.edit.text : "";
            textInputData->compositionCursor = e.edit.start;
            textInputData->compositionLength = e.edit.length;
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

    case SDL_EVENT_DROP_FILE:
        if (e.drop.data) {
            droppedFiles.emplace_back(e.drop.data);
        }
        break;

    // Phase F2: Gamepad navigation — translate buttons to virtual key events
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    {
        bool down = (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
        SDL_Scancode mapped = SDL_SCANCODE_UNKNOWN;
        switch (e.gbutton.button) {
            case SDL_GAMEPAD_BUTTON_DPAD_UP:    mapped = SDL_SCANCODE_UP; break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  mapped = SDL_SCANCODE_DOWN; break;
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  mapped = SDL_SCANCODE_LEFT; break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: mapped = SDL_SCANCODE_RIGHT; break;
            case SDL_GAMEPAD_BUTTON_SOUTH:      mapped = SDL_SCANCODE_RETURN; break;   // A
            case SDL_GAMEPAD_BUTTON_EAST:       mapped = SDL_SCANCODE_ESCAPE; break;   // B
            case SDL_GAMEPAD_BUTTON_WEST:       mapped = SDL_SCANCODE_SPACE; break;    // X
            case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  mapped = SDL_SCANCODE_TAB; break;
            case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: mapped = SDL_SCANCODE_TAB; break;
            default: break;
        }
        if (mapped != SDL_SCANCODE_UNKNOWN && mapped < SDL_SCANCODE_COUNT) {
            if (down) {
                keysDown[mapped] = true;
                keysPressed[mapped] = true;
                anyKeyPressed = true;
            } else {
                keysDown[mapped] = false;
                keysReleased[mapped] = true;
            }
        }
        break;
    }

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    {
        // Right stick → scroll wheel; threshold 8000 of 32767 (~25%)
        const Sint16 deadzone = 8000;
        if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX) {
            if (std::abs(e.gaxis.value) > deadzone) {
                mouseWheelX += static_cast<float>(e.gaxis.value) / 32767.0f * 0.25f;
            }
        } else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY) {
            if (std::abs(e.gaxis.value) > deadzone) {
                mouseWheelY += -static_cast<float>(e.gaxis.value) / 32767.0f * 0.25f;
            }
        }
        break;
    }

    case SDL_EVENT_GAMEPAD_ADDED:
    {
        // Auto-open the gamepad so events flow
        SDL_OpenGamepad(e.gdevice.which);
        break;
    }

    case SDL_EVENT_QUIT:
        break;

    default:
        break;
    }
}
