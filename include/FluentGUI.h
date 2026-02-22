// FluentGUI.h : Main header file for FluentGUI library
// A Fluent Design System inspired Immediate Mode GUI library
//
// USAGE EXAMPLE:
// ==============
// #include <SDL3/SDL.h>
// #include "FluentGUI.h"
// using namespace FluentUI;
//
// int main() {
//     SDL_Init(SDL_INIT_VIDEO);
//     SDL_Window* window = SDL_CreateWindow("App", 800, 600, 
//                                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
//     
//     // 1. Initialize FluentGUI
//     UIContext* ctx = CreateContext(window);
//     if (!ctx) return -1;
//     
//     // Optional: Set theme
//     ctx->style = GetDarkFluentStyle(); // or GetDefaultFluentStyle()
//     
//     // Optional: Enable text input
//     SDL_StartTextInput(window);
//     
//     bool running = true;
//     SDL_Event e;
//     uint64_t lastTime = SDL_GetTicks();
//     
//     while (running) {
//         // 2. Calculate deltaTime
//         uint64_t currentTime = SDL_GetTicks();
//         float deltaTime = (currentTime - lastTime) / 1000.0f;
//         deltaTime = std::min(deltaTime, 0.1f); // Cap at 100ms
//         lastTime = currentTime;
//         
//         // 3. Update input state
//         ctx->input.Update(window);
//         
//         // 4. Process SDL events
//         while (SDL_PollEvent(&e)) {
//             if (e.type == SDL_EVENT_QUIT)
//                 running = false;
//             else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
//                 int width, height;
//                 SDL_GetWindowSize(window, &width, &height);
//                 ctx->renderer.SetViewport(width, height);
//             }
//             else
//                 ctx->input.ProcessEvent(e);
//         }
//         
//         // 5. Start new frame
//         NewFrame(deltaTime);
//         
//         // 6. Build your UI
//         BeginVertical();
//         if (Button("Click Me!")) {
//             // Handle click
//         }
//         Label("Hello FluentGUI!");
//         EndVertical();
//         
//         // 7. Render deferred elements (dropdowns, menus, etc.)
//         RenderDeferredDropdowns();
//         
//         // 8. Render everything
//         Render();
//         
//         // 9. Swap buffers
//         SDL_GL_SwapWindow(window);
//     }
//     
//     // 10. Cleanup
//     SDL_StopTextInput(window);
//     DestroyContext();
//     SDL_DestroyWindow(window);
//     SDL_Quit();
//     return 0;
// }

#pragma once

// Core includes
#include "core/Context.h"
#include "core/InputState.h"
#include "core/Renderer.h"

// UI includes
#include "UI/Layout.h"
#include "UI/Widgets.h"

// Theme includes
#include "Theme/FluentTheme.h"
#include "Theme/Style.h"

// Math includes
#include "Math/Color.h"
#include "Math/Vec2.h"

// Main namespace
namespace FluentUI {
    // Convenience function to initialize the library
    // Equivalent to CreateContext(window)
    // Usage: UIContext* ctx = FluentUI::Init(window);
    inline UIContext *Init(SDL_Window *window) { 
        return CreateContext(window); 
    }
    
    // Quick access to global context (if you don't want to store the pointer)
    // Usage: UIContext* ctx = FluentUI::GetContext();
    // Note: Returns nullptr if context hasn't been created yet
    // Note: This is already available via ::FluentUI::GetContext() from Context.h
} // namespace FluentUI
