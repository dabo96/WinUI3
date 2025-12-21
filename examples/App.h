#pragma once

#include <SDL3/SDL.h>
#include "FluentGUI.h"

class App
{
public:
	App(const char* titulo);
	~App();
	void Run();
public:
	void ShowWidgetsBasicTab();
	void ShowInputControlsTab();
    void ShowEditorTab(); // New function declaration
	void ShowContainersTab(FluentUI::UIContext* ctx);
	void ShowAdvancedWidgetsTab(FluentUI::UIContext* ctx);
private:
    // Estado de la UI
    int currentTab = 0;
    
    // Demo variables
    float m_gameObjectPos[3] = { 0.0f, 0.0f, 0.0f };
	std::string m_nombreCompleto; // Buffer de texto que se edita en tiempo real
	std::string m_nombreMostrado; // Texto que se muestra en el Label (solo se actualiza al presionar el botón)
	bool m_mostrarNombre = false; // Estado para controlar si se muestra el nombre (solo se activa al presionar el botón)
	SDL_Window* window;
	FluentUI::UIContext* ctx;
	uint64_t lastTime;
private:
	FluentUI::Vec2 scrollOffset = FluentUI::Vec2(0, 0);
	bool checkboxValue = false;
	int selectedListItem = -1;
	bool treeItem1Open = false;
    bool treeItem2Open = false;
    bool treeItem3Open = false;
    
    // Advanced Widgets State
    float splitterSize1 = -1.0f;
    float splitterSize2 = -1.0f;
    FluentUI::Color colorPickerValue = FluentUI::Color(1.0f, 0.0f, 0.0f, 1.0f);
};

