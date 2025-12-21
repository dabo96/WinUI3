#include "core/Context.h"
#include "core/Renderer.h"
#include "Theme/FluentTheme.h"

namespace FluentUI {

    static UIContext* g_ctx = nullptr;

    UIContext* CreateContext(SDL_Window* window) {
        if (g_ctx) return g_ctx;
        g_ctx = new UIContext();
        g_ctx->window = window;
        g_ctx->renderer.Init(window);
        g_ctx->style = GetDarkFluentStyle();
        g_ctx->initialized = true;
        g_ctx->frame = 0;
        return g_ctx;
    }

    UIContext* GetContext() {
        return g_ctx;
    }

    void DestroyContext() {
        if (!g_ctx) return;
        g_ctx->renderer.Shutdown();
        delete g_ctx;
        g_ctx = nullptr;
    }

    void NewFrame(float deltaTime) {
        if (!g_ctx || !g_ctx->initialized) return;
        
        // Z-Order management
        g_ctx->hoveredPanelId = g_ctx->nextHoveredPanelId;
        g_ctx->nextHoveredPanelId = "";

        // Reset counters
        g_ctx->frame++;
        g_ctx->deltaTime = deltaTime; // Corrected from 'dt;eltaTime'
        
        // Actualizar tiempo
        g_ctx->time += deltaTime;
        
        // Actualizar animaciones de colores
        for (auto& [id, anim] : g_ctx->colorAnimations) {
            anim.Update(deltaTime);
        }
        
        // Actualizar animaciones de floats
        for (auto& [id, anim] : g_ctx->floatAnimations) {
            anim.Update(deltaTime);
        }
        
        // Actualizar ripple effects
        for (auto& [id, ripple] : g_ctx->rippleEffects) {
            ripple.Update(deltaTime);
        }
        
        g_ctx->renderer.BeginFrame();
        g_ctx->cursorPos = { 20.0f, 20.0f };
        g_ctx->lastItemPos = g_ctx->cursorPos;
        g_ctx->lastItemSize = { 0.0f, 0.0f };
        if (!g_ctx->input.IsMouseDown(0)) {
            if (g_ctx->activeWidgetType == ActiveWidgetType::Slider || 
                g_ctx->activeWidgetType == ActiveWidgetType::Splitter) {
                g_ctx->activeWidgetId = 0;
                g_ctx->activeWidgetType = ActiveWidgetType::None;
                if (g_ctx->window) SDL_SetWindowRelativeMouseMode(g_ctx->window, false); // Unlock cursor centrally
            }
        }
        if (!g_ctx->input.IsKeyDown(SDL_SCANCODE_LCTRL) && !g_ctx->input.IsKeyDown(SDL_SCANCODE_RCTRL)) {
            g_ctx->input.anyKeyPressed = false;
        }
        
        // Manejar navegación con Tab
        if (g_ctx->input.IsKeyPressed(SDL_SCANCODE_TAB)) {
            bool shift = g_ctx->input.IsKeyDown(SDL_SCANCODE_LSHIFT) || g_ctx->input.IsKeyDown(SDL_SCANCODE_RSHIFT);
            if (!g_ctx->focusableWidgets.empty()) {
                if (shift) {
                    // Navegar hacia atrás
                    g_ctx->focusIndex = (g_ctx->focusIndex <= 0) ? 
                        static_cast<int>(g_ctx->focusableWidgets.size() - 1) : g_ctx->focusIndex - 1;
                } else {
                    // Navegar hacia adelante
                    g_ctx->focusIndex = (g_ctx->focusIndex >= static_cast<int>(g_ctx->focusableWidgets.size() - 1)) ? 
                        0 : g_ctx->focusIndex + 1;
                }
                if (g_ctx->focusIndex >= 0 && g_ctx->focusIndex < static_cast<int>(g_ctx->focusableWidgets.size())) {
                    g_ctx->focusedWidgetId = g_ctx->focusableWidgets[g_ctx->focusIndex];
                }
            }
        }
        
        // Limpiar lista de widgets enfocables al inicio de cada frame
        g_ctx->focusableWidgets.clear();
        g_ctx->focusableWidgets.reserve(64); // Pre-reservar capacidad
        
        // Limpiar stack de menús al inicio de cada frame
        g_ctx->menuIdStack.clear();
        g_ctx->currentMenuItems.clear();
        g_ctx->menuItemStartIndexStack.clear();
        
        // Optimización: Limpiar caché de texto periódicamente
        g_ctx->textCacheFrame++;
        if (g_ctx->textCacheFrame >= UIContext::TEXT_CACHE_MAX_AGE) {
            g_ctx->textMeasurementCache.clear();
            g_ctx->textCacheFrame = 0;
        }
        
        // Cerrar context menus y menus si se hace click fuera de ellos
        if (g_ctx->input.IsMousePressed(0) || g_ctx->input.IsMousePressed(1) || g_ctx->input.IsMousePressed(2)) {
            float mouseX = g_ctx->input.MouseX();
            float mouseY = g_ctx->input.MouseY();
            bool clickedOutside = true;
            
            // Verificar context menus
            for (auto& [id, menuState] : g_ctx->contextMenuStates) {
                if (menuState.open) {
                    Vec2 menuPos = menuState.position;
                    Vec2 menuSize = menuState.size;
                    if (mouseX >= menuPos.x && mouseX <= menuPos.x + menuSize.x &&
                        mouseY >= menuPos.y && mouseY <= menuPos.y + menuSize.y) {
                        clickedOutside = false;
                    } else {
                        menuState.open = false;
                    }
                }
            }
            
            // Verificar menus del MenuBar
            for (auto& [id, menuState] : g_ctx->menuStates) {
                if (menuState.open) {
                    Vec2 menuPos = menuState.position;
                    Vec2 menuSize = menuState.size;
                    // Verificar si el click está en el menú o en su dropdown
                    if (mouseX >= menuPos.x && mouseX <= menuPos.x + menuSize.x &&
                        mouseY >= menuPos.y && mouseY <= menuPos.y + menuSize.y) {
                        clickedOutside = false;
                    } else {
                        menuState.open = false;
                    }
                }
            }
            
            // Si se hizo click fuera de todos los menus, cerrar todos
            if (clickedOutside) {
                for (auto& [id, menuState] : g_ctx->contextMenuStates) {
                    menuState.open = false;
                }
                g_ctx->activeContextMenuId = 0;
                for (auto& [id, menuState] : g_ctx->menuStates) {
                    menuState.open = false;
                }
                g_ctx->activeMenuId = 0;
            }
        }
        
        g_ctx->frame++;
    }

    void Render() {
        if (!g_ctx || !g_ctx->initialized) return;
        g_ctx->renderer.EndFrame();
    }

} // namespace FluentUI
