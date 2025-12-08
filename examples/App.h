#pragma once

#include <SDL3/SDL.h>
#include "FluentGUI.h"

class App
{
public:
	App(const char* titulo);
	~App();
	void Run();
private:
	std::string m_nombreCompleto; // Buffer de texto que se edita en tiempo real
	std::string m_nombreMostrado; // Texto que se muestra en el Label (solo se actualiza al presionar el botón)
	bool m_mostrarNombre = false; // Estado para controlar si se muestra el nombre
	SDL_Window* window;
	FluentUI::UIContext* ctx;
	uint64_t lastTime;
};

