#include <SDL3/SDL.h>
#include "core/InputState.h"
#include "core/SDLPlatform.h" // SDL→UIEvent translation (this .cpp owns the seam)
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

// ─── brief 20: platform-neutral key queries (state indexed by UIKey) ─────────
static inline bool keyIndexValid(UIKey k) {
    return static_cast<int>(k) > 0 && static_cast<int>(k) < static_cast<int>(UIKey::Count);
}
bool InputState::IsKeyDown(UIKey key) const     { return keyIndexValid(key) && keysDown[static_cast<size_t>(key)]; }
bool InputState::IsKeyPressed(UIKey key) const  { return keyIndexValid(key) && keysPressed[static_cast<size_t>(key)]; }
bool InputState::IsKeyReleased(UIKey key) const { return keyIndexValid(key) && keysReleased[static_cast<size_t>(key)]; }

bool InputState::CtrlDown() const  { return keysDown[static_cast<size_t>(UIKey::LeftCtrl)]  || keysDown[static_cast<size_t>(UIKey::RightCtrl)]; }
bool InputState::ShiftDown() const { return keysDown[static_cast<size_t>(UIKey::LeftShift)] || keysDown[static_cast<size_t>(UIKey::RightShift)]; }
bool InputState::AltDown() const   { return keysDown[static_cast<size_t>(UIKey::LeftAlt)]   || keysDown[static_cast<size_t>(UIKey::RightAlt)]; }

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

void InputState::Update(WindowHandle window)
{
    SDL_Window* win = static_cast<SDL_Window*>(window);
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
    if (win) {
        if (SDL_GetMouseFocus() == win) {
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

void InputState::ProcessEvent(const UIEvent& e)
{
    switch (e.type)
    {
    case UIEventType::KeyDown:
        if (keyIndexValid(e.key)) {
            keysDown[static_cast<size_t>(e.key)] = true;
            keysPressed[static_cast<size_t>(e.key)] = true;
        }
        anyKeyPressed = true;
        break;

    case UIEventType::KeyUp:
        if (keyIndexValid(e.key)) {
            keysDown[static_cast<size_t>(e.key)] = false;
            keysReleased[static_cast<size_t>(e.key)] = true;
        }
        break;

    case UIEventType::TextInput:
        if (textInputData) {
            textInputData->buffer += e.text;
            // Committed text clears any active composition
            textInputData->compositionText.clear();
            textInputData->compositionCursor = 0;
            textInputData->compositionLength = 0;
        }
        break;

    case UIEventType::TextEditing:
        if (textInputData) {
            textInputData->compositionText = e.text;
            textInputData->compositionCursor = e.editStart;
            textInputData->compositionLength = e.editLength;
        }
        break;

    case UIEventType::MouseButton:
        if (e.button >= 0 && e.button < static_cast<int>(mouseDown.size())) {
            if (e.pressed) {
                mouseX = e.x;
                mouseY = e.y;
                mouseDown[e.button] = true;
                mousePressed[e.button] = true;
            } else {
                mouseDown[e.button] = false;
                mouseReleased[e.button] = true;
            }
        }
        break;

    case UIEventType::MouseMove:
        mouseX = e.x;
        mouseY = e.y;
        break;

    case UIEventType::MouseWheel:
        mouseWheelX += e.wheelX;
        mouseWheelY += e.wheelY;
        break;

    case UIEventType::DropBegin:
    case UIEventType::DropPosition:
        // brief 18.7: an OS drag is over this window — track so widgets can show a
        // drop-target highlight while the cursor moves.
        osDragActive = true;
        dropX = e.x;
        dropY = e.y;
        break;

    case UIEventType::DropFile:
        if (!e.text.empty()) droppedFiles.push_back(e.text);
        dropX = e.x;
        dropY = e.y;
        break;

    case UIEventType::DropText:
        if (!e.text.empty()) droppedText = e.text;
        dropX = e.x;
        dropY = e.y;
        break;

    case UIEventType::DropComplete:
        osDragActive = false;
        break;

    default:
        break;
    }
}

// ─── brief 20: SDL → neutral-event seam (SDL lives ONLY here, not in the core) ─
// Defined in namespace FluentUI to match the declarations in SDLPlatform.h (this
// TU uses `using namespace FluentUI`, which would otherwise make these ::global).
namespace FluentUI {

UIKey UIKeyFromScancode(SDL_Scancode sc)
{
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
        return static_cast<UIKey>(static_cast<int>(UIKey::A) + (sc - SDL_SCANCODE_A));
    if (sc == SDL_SCANCODE_0) return UIKey::Num0;
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
        return static_cast<UIKey>(static_cast<int>(UIKey::Num1) + (sc - SDL_SCANCODE_1));
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12)
        return static_cast<UIKey>(static_cast<int>(UIKey::F1) + (sc - SDL_SCANCODE_F1));
    switch (sc) {
        case SDL_SCANCODE_SPACE:     return UIKey::Space;
        case SDL_SCANCODE_RETURN:    return UIKey::Enter;
        case SDL_SCANCODE_KP_ENTER:  return UIKey::KeypadEnter;
        case SDL_SCANCODE_ESCAPE:    return UIKey::Escape;
        case SDL_SCANCODE_TAB:       return UIKey::Tab;
        case SDL_SCANCODE_BACKSPACE: return UIKey::Backspace;
        case SDL_SCANCODE_DELETE:    return UIKey::Delete;
        case SDL_SCANCODE_INSERT:    return UIKey::Insert;
        case SDL_SCANCODE_LEFT:      return UIKey::Left;
        case SDL_SCANCODE_RIGHT:     return UIKey::Right;
        case SDL_SCANCODE_UP:        return UIKey::Up;
        case SDL_SCANCODE_DOWN:      return UIKey::Down;
        case SDL_SCANCODE_HOME:      return UIKey::Home;
        case SDL_SCANCODE_END:       return UIKey::End;
        case SDL_SCANCODE_PAGEUP:    return UIKey::PageUp;
        case SDL_SCANCODE_PAGEDOWN:  return UIKey::PageDown;
        case SDL_SCANCODE_LCTRL:     return UIKey::LeftCtrl;
        case SDL_SCANCODE_RCTRL:     return UIKey::RightCtrl;
        case SDL_SCANCODE_LSHIFT:    return UIKey::LeftShift;
        case SDL_SCANCODE_RSHIFT:    return UIKey::RightShift;
        case SDL_SCANCODE_LALT:      return UIKey::LeftAlt;
        case SDL_SCANCODE_RALT:      return UIKey::RightAlt;
        default:                     return UIKey::Unknown;
    }
}

static UIKey UIKeyFromGamepadButton(int button)
{
    switch (button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:        return UIKey::Up;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return UIKey::Down;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return UIKey::Left;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return UIKey::Right;
        case SDL_GAMEPAD_BUTTON_SOUTH:          return UIKey::Enter;   // A
        case SDL_GAMEPAD_BUTTON_EAST:           return UIKey::Escape;  // B
        case SDL_GAMEPAD_BUTTON_WEST:           return UIKey::Space;   // X
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return UIKey::Tab;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return UIKey::Tab;
        default:                                return UIKey::Unknown;
    }
}

bool TranslateSDLEvent(const SDL_Event& e, UIEvent& out)
{
    out = UIEvent{};
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN: out.type = UIEventType::KeyDown; out.key = UIKeyFromScancode(e.key.scancode); return true;
    case SDL_EVENT_KEY_UP:   out.type = UIEventType::KeyUp;   out.key = UIKeyFromScancode(e.key.scancode); return true;
    case SDL_EVENT_TEXT_INPUT:
        out.type = UIEventType::TextInput;
        out.text = e.text.text ? e.text.text : "";
        return true;
    case SDL_EVENT_TEXT_EDITING:
        out.type = UIEventType::TextEditing;
        out.text = e.edit.text ? e.edit.text : "";
        out.editStart = e.edit.start;
        out.editLength = e.edit.length;
        return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        out.type = UIEventType::MouseButton;
        out.pressed = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        out.button = static_cast<int>(e.button.button) - 1; // 0 = left
        out.x = static_cast<float>(e.button.x);
        out.y = static_cast<float>(e.button.y);
        return true;
    case SDL_EVENT_MOUSE_MOTION:
        out.type = UIEventType::MouseMove;
        out.x = static_cast<float>(e.motion.x);
        out.y = static_cast<float>(e.motion.y);
        return true;
    case SDL_EVENT_MOUSE_WHEEL:
        out.type = UIEventType::MouseWheel;
        out.wheelX = static_cast<float>(e.wheel.x);
        out.wheelY = static_cast<float>(e.wheel.y);
        return true;
    case SDL_EVENT_DROP_BEGIN:    out.type = UIEventType::DropBegin;    out.x = e.drop.x; out.y = e.drop.y; return true;
    case SDL_EVENT_DROP_POSITION: out.type = UIEventType::DropPosition; out.x = e.drop.x; out.y = e.drop.y; return true;
    case SDL_EVENT_DROP_FILE:
        out.type = UIEventType::DropFile;
        out.text = e.drop.data ? e.drop.data : "";
        out.x = e.drop.x; out.y = e.drop.y;
        return true;
    case SDL_EVENT_DROP_TEXT:
        out.type = UIEventType::DropText;
        out.text = e.drop.data ? e.drop.data : "";
        out.x = e.drop.x; out.y = e.drop.y;
        return true;
    case SDL_EVENT_DROP_COMPLETE: out.type = UIEventType::DropComplete; return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        UIKey k = UIKeyFromGamepadButton(e.gbutton.button);
        if (k == UIKey::Unknown) return false;
        out.type = (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) ? UIEventType::KeyDown : UIEventType::KeyUp;
        out.key = k;
        return true;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        // Right stick → scroll wheel; threshold ~25% (8000 of 32767).
        const Sint16 deadzone = 8000;
        if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX && std::abs(e.gaxis.value) > deadzone) {
            out.type = UIEventType::MouseWheel;
            out.wheelX = static_cast<float>(e.gaxis.value) / 32767.0f * 0.25f;
            return true;
        }
        if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY && std::abs(e.gaxis.value) > deadzone) {
            out.type = UIEventType::MouseWheel;
            out.wheelY = -static_cast<float>(e.gaxis.value) / 32767.0f * 0.25f;
            return true;
        }
        return false;
    }
    case SDL_EVENT_GAMEPAD_ADDED:
        SDL_OpenGamepad(e.gdevice.which); // side effect only — produces no UIEvent
        return false;
    // ── Window events (routed by FluentApp; size is queried by the handler) ──
    case SDL_EVENT_WINDOW_RESIZED:
        out.type = UIEventType::Resize;
        out.width = e.window.data1; out.height = e.window.data2;
        return true;
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_MOVED:
        out.type = UIEventType::DpiChange;
        return true;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        out.type = UIEventType::WindowClose;
        return true;
    case SDL_EVENT_QUIT:
        out.type = UIEventType::Quit;
        return true;
    default:
        return false;
    }
}

void ProcessSDLEvent(InputState& input, const SDL_Event& e)
{
    UIEvent ui;
    if (TranslateSDLEvent(e, ui)) input.ProcessEvent(ui);
}

} // namespace FluentUI
