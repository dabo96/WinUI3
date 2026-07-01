#include <SDL3/SDL.h>
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

// ─── brief 20 Part B: platform-neutral key layer ─────────────────────────────
// The only place UIKey is translated to an SDL scancode. Widgets never see SDL.
namespace {
SDL_Scancode ScancodeForUIKey(UIKey k) {
    // Letters A..Z are contiguous in both enums.
    if (k >= UIKey::A && k <= UIKey::Z)
        return static_cast<SDL_Scancode>(SDL_SCANCODE_A + (static_cast<int>(k) - static_cast<int>(UIKey::A)));
    // Digits: SDL orders 1..9 then 0, so map 0 explicitly and 1..9 by offset.
    if (k == UIKey::Num0) return SDL_SCANCODE_0;
    if (k >= UIKey::Num1 && k <= UIKey::Num9)
        return static_cast<SDL_Scancode>(SDL_SCANCODE_1 + (static_cast<int>(k) - static_cast<int>(UIKey::Num1)));
    // Function keys F1..F12 are contiguous in both enums.
    if (k >= UIKey::F1 && k <= UIKey::F12)
        return static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + (static_cast<int>(k) - static_cast<int>(UIKey::F1)));
    switch (k) {
        case UIKey::Space:       return SDL_SCANCODE_SPACE;
        case UIKey::Enter:       return SDL_SCANCODE_RETURN;
        case UIKey::KeypadEnter: return SDL_SCANCODE_KP_ENTER;
        case UIKey::Escape:      return SDL_SCANCODE_ESCAPE;
        case UIKey::Tab:         return SDL_SCANCODE_TAB;
        case UIKey::Backspace:   return SDL_SCANCODE_BACKSPACE;
        case UIKey::Delete:      return SDL_SCANCODE_DELETE;
        case UIKey::Insert:      return SDL_SCANCODE_INSERT;
        case UIKey::Left:        return SDL_SCANCODE_LEFT;
        case UIKey::Right:       return SDL_SCANCODE_RIGHT;
        case UIKey::Up:          return SDL_SCANCODE_UP;
        case UIKey::Down:        return SDL_SCANCODE_DOWN;
        case UIKey::Home:        return SDL_SCANCODE_HOME;
        case UIKey::End:         return SDL_SCANCODE_END;
        case UIKey::PageUp:      return SDL_SCANCODE_PAGEUP;
        case UIKey::PageDown:    return SDL_SCANCODE_PAGEDOWN;
        case UIKey::LeftCtrl:    return SDL_SCANCODE_LCTRL;
        case UIKey::RightCtrl:   return SDL_SCANCODE_RCTRL;
        case UIKey::LeftShift:   return SDL_SCANCODE_LSHIFT;
        case UIKey::RightShift:  return SDL_SCANCODE_RSHIFT;
        case UIKey::LeftAlt:     return SDL_SCANCODE_LALT;
        case UIKey::RightAlt:    return SDL_SCANCODE_RALT;
        default:                 return SDL_SCANCODE_UNKNOWN;
    }
}
} // namespace

bool InputState::IsKeyDown(UIKey key) const     { return IsKeyDown(ScancodeForUIKey(key)); }
bool InputState::IsKeyPressed(UIKey key) const  { return IsKeyPressed(ScancodeForUIKey(key)); }
bool InputState::IsKeyReleased(UIKey key) const { return IsKeyReleased(ScancodeForUIKey(key)); }

bool InputState::CtrlDown() const  { return keysDown[SDL_SCANCODE_LCTRL]  || keysDown[SDL_SCANCODE_RCTRL]; }
bool InputState::ShiftDown() const { return keysDown[SDL_SCANCODE_LSHIFT] || keysDown[SDL_SCANCODE_RSHIFT]; }
bool InputState::AltDown() const   { return keysDown[SDL_SCANCODE_LALT]   || keysDown[SDL_SCANCODE_RALT]; }

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

void InputState::SetClipboardText(const std::string& utf8)
{
    SDL_SetClipboardText(utf8.c_str());
}

std::string InputState::GetClipboardText()
{
    char* clip = SDL_GetClipboardText();
    std::string result;
    if (clip) {
        result = clip;
        SDL_free(clip);
    }
    return result;
}

bool InputState::HasClipboardText()
{
    return SDL_HasClipboardText();
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
    droppedText.clear();   // brief 18.7
    anyKeyPressed = false;

    prevMouseX = mouseX;
    prevMouseY = mouseY;

    // En SDL3, obtener la posición actual del mouse cada frame asegura que siempre
    // tengamos la posición correcta, incluso sin eventos. PERO SDL_GetMouseState
    // devuelve coords relativas a la ventana CON FOCO de ratón: en multi-ventana,
    // si el ratón está sobre OTRA ventana, aplicarlas aquí provoca hover fantasma
    // (la ventana sin foco "cree" que el cursor está dentro). Solo refrescamos la
    // posición cuando ESTA ventana tiene el foco del ratón; si no, mandamos el
    // cursor fuera para que el hover se limpie.
    if (window) {
        if (SDL_GetMouseFocus() == window) {
            float mouseXPos, mouseYPos;
            SDL_GetMouseState(&mouseXPos, &mouseYPos);
            mouseX = mouseXPos;
            mouseY = mouseYPos;
        } else {
            mouseX = -1000.0f;
            mouseY = -1000.0f;
        }
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

    case SDL_EVENT_DROP_BEGIN:
        // brief 18.7: an OS drag entered this window — start tracking so widgets
        // can show a drop-target highlight while the cursor moves over them.
        osDragActive = true;
        dropX = e.drop.x;
        dropY = e.drop.y;
        break;

    case SDL_EVENT_DROP_POSITION:
        // Cursor moving inside the window during an active OS drag.
        osDragActive = true;
        dropX = e.drop.x;
        dropY = e.drop.y;
        break;

    case SDL_EVENT_DROP_FILE:
        if (e.drop.data) {
            droppedFiles.emplace_back(e.drop.data);
        }
        dropX = e.drop.x;
        dropY = e.drop.y;
        break;

    case SDL_EVENT_DROP_TEXT:
        // brief 18.7: dragged text payload (e.g. a URL or selection from another app).
        if (e.drop.data) {
            droppedText = e.drop.data;
        }
        dropX = e.drop.x;
        dropY = e.drop.y;
        break;

    case SDL_EVENT_DROP_COMPLETE:
        // The OS drag ended (whether or not anything was dropped on us).
        osDragActive = false;
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
