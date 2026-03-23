#include "core/InputState.h"
#include <string>

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

    case SDL_EVENT_QUIT:
        break;

    default:
        break;
    }
}
