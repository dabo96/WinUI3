#ifdef FLUENTUI_HAS_SDL
#include <SDL3/SDL.h> // only for the per-frame mouse poll (owned/SDL loop); gated for embedded
#endif
#include "core/InputState.h"
#include <string>
#include <cstdlib>
// brief 25: the SDL→UIEvent translation (UIKeyFromScancode / TranslateSDLEvent /
// ProcessSDLEvent) moved to src/Core/SDLPlatform.cpp so SDL event translation lives
// in the SDL platform TU. brief 26: clipboard now routes through PlatformBackend at
// the call sites; the only SDL left here is the per-frame mouse poll, gated so the
// embedded build (SDL=OFF) drives the mouse purely from injected UIEvents.

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

void InputState::Update(WindowHandle window)
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
    // brief 26: this per-frame poll is an SDL-loop convenience; the embedded build
    // (SDL=OFF) tracks the mouse purely from injected mouse-move UIEvents.
#ifdef FLUENTUI_HAS_SDL
    SDL_Window* win = static_cast<SDL_Window*>(window);
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
#else
    (void)window;
#endif

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
