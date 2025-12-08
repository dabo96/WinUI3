#include "App.h"

using namespace FluentUI;

App::App(const char* titulo) : m_nombreCompleto(""), m_nombreMostrado(""), m_mostrarNombre(false)
{
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(titulo, 1280, 720, 
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    // Inicializar FluentGUI
    ctx = CreateContext(window);

    if (!ctx) {
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    //
    ctx->style = GetDarkFluentStyle();

    SDL_StartTextInput(window);

    lastTime = SDL_GetTicks();
}

App::~App()
{
    DestroyContext();
    SDL_StopTextInput(window);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void App::Run()
{
    SDL_Event e;
    bool running = true;

    while (running) {

        uint64_t currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        deltaTime = std::min(deltaTime, 0.1f);
        lastTime = currentTime;
        
        // IMPORTANTE: Actualizar el estado de input ANTES de procesar eventos
        // Esto resetea los flags de IsMousePressed, IsKeyPressed, etc. cada frame
        ctx->input.Update(window);
        
        // Procesar eventos
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = false;
            else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
                int width, height;
                SDL_GetWindowSize(window, &width, &height);
                ctx->renderer.SetViewport(width, height);
            }
            else
                ctx->input.ProcessEvent(e);
        }

        // Nuevo frame
        NewFrame(deltaTime);

        // Construir UI
        BeginVertical();


		Label("Ingresa tu nombre completo:");
        TextInput("nombre_usuario", &m_nombreCompleto, 250.0f);
        
        Spacing(5);

        if (Button("Mostrar Nombre")) {
            m_nombreMostrado = m_nombreCompleto; // Copiar el texto actual al texto a mostrar
            m_mostrarNombre = true; // Activar el flag cuando se presiona el botón
        }
        
        // Mostrar el label solo si el flag está activo Y hay texto para mostrar
        if (m_mostrarNombre && !m_nombreMostrado.empty()) {
            Label(m_nombreMostrado); // Mostrar el texto guardado, no el que se está editando
        }
        
        EndVertical();

        // Renderizar
        Render();

        SDL_GL_SwapWindow(window);
    }
}
