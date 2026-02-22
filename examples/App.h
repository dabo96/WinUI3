#pragma once

#include <SDL3/SDL.h>
#include "FluentGUI.h"
#include <vector>
#include <string>

class App
{
public:
    App(const char* titulo);
    ~App();
    void Run();

private:
    void BuildUI();
    void BuildMenuBar();
    void BuildBasicWidgets();
    void BuildInputWidgets();
    void BuildContainers();
    void BuildListsAndTrees();
    void BuildOverlays();
    void BuildThemeSettings();

    SDL_Window* window;
    FluentUI::UIContext* ctx;
    uint64_t lastTime;

    // Main tab
    int m_mainTab = 0;
    std::vector<std::string> m_mainTabLabels;

    // Basic widgets state
    int m_buttonClickCount = 0;
    bool m_checkbox1 = false;
    bool m_checkbox2 = true;
    bool m_checkbox3 = false;
    int m_radioSelection = 0;
    float m_progressValue = 0.0f;
    bool m_progressAnimating = true;

    // Input widgets state
    float m_sliderFloat = 0.5f;
    int m_sliderInt = 50;
    float m_sliderVolume = 75.0f;
    std::string m_textInput;
    std::string m_searchText;
    std::string m_multilineText;
    std::string m_passwordText;
    int m_comboSelection = 0;
    std::vector<std::string> m_comboItems;
    int m_comboFont = 0;
    std::vector<std::string> m_fontItems;

    // Container state
    FluentUI::Vec2 m_scrollOffset = FluentUI::Vec2(0.0f, 0.0f);
    int m_containerTab = 0;
    std::vector<std::string> m_containerTabLabels;

    // List & Tree state
    int m_listSelection = 0;
    std::vector<std::string> m_listItems;
    int m_fileListSelection = -1;
    std::vector<std::string> m_fileListItems;
    bool m_treeRoot1Open = true;
    bool m_treeRoot2Open = false;
    bool m_treeRoot3Open = false;
    bool m_treeSub1Open = false;
    bool m_treeSub2Open = false;
    bool m_treeSelected1 = false;
    bool m_treeSelected2 = false;
    bool m_treeSelected3 = false;
    bool m_treeSelected4 = false;
    bool m_treeSelected5 = false;

    // Overlay state
    bool m_modalOpen = false;
    bool m_confirmModalOpen = false;
    std::string m_modalInput;
    int m_tooltipDemo = 0;

    // Theme state
    bool m_isDarkTheme = true;
    int m_accentColorIdx = 0;
    std::vector<std::string> m_accentNames;

    // Status
    std::string m_statusText;
};
