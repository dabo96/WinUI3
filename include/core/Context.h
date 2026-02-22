#pragma once

#include "Animation.h"
#include "Math/Color.h"
#include "Math/Vec2.h"
#include "InputState.h"
#include "Renderer.h"
#include "RippleEffect.h"
#include <SDL3/SDL.h>
#include "Theme/Style.h"
#include <map>


namespace FluentUI {

struct OccupiedArea {
  Vec2 pos;
  Vec2 size;
};

struct LayoutStack {
  Vec2 origin;
  Vec2 cursor;
  Vec2 contentStart;
  Vec2 padding;
  Vec2 contentSize;
  Vec2 availableSpace;
  float spacing = 0.0f;
  bool isVertical = true;
  float lineHeight = 0.0f;
  int itemCount = 0;
  std::vector<OccupiedArea> occupiedAreas;
};

struct TabContentFrame {
  uint32_t tabViewId;
  Vec2 contentAreaPos;
  Vec2 contentAreaSize;
  Vec2 contentStartCursor;
};

enum class ActiveWidgetType { None, Slider, TextInput };

struct PanelFrameContext {
  uint32_t id;
  Vec2 layoutOrigin;
  float titleHeight;
  Vec2 clipPos;
  Vec2 clipSize;
  bool clipPushed;
  bool layoutPushed;
  bool reserveLayout = true;
  Vec2 reservedLayoutSize{0.0f, 0.0f};
  Vec2 savedCursor;
  Vec2 savedLastItemPos;
  Vec2 savedLastItemSize;
  Vec2 parentCursor;
  Vec2 parentContentSize;
  Vec2 parentAvailable;
  int parentItemCount = 0;
};

struct ScrollViewFrameContext {
  uint32_t id;
  Vec2 position;
  Vec2 size;
  Vec2 contentAreaPos;
  Vec2 contentAreaSize;
  Vec2 availableSize;
  float scrollbarWidth;
  bool layoutPushed;
  bool useAbsolutePos;
  Vec2 savedCursor;
  Vec2 savedLastItemPos;
  Vec2 savedLastItemSize;
};

struct UIContext {
  Renderer renderer;
  InputState input;
  Style style;
  SDL_Window *window =
      nullptr; // Referencia a la ventana para obtener posición del mouse

  // Sistema de callbacks y eventos
  using WidgetCallback = std::function<void()>;
  using ValueChangedCallback = std::function<void(const std::string &, void *)>;

  std::unordered_map<uint32_t, WidgetCallback> widgetCallbacks;
  std::unordered_map<std::string, ValueChangedCallback> valueChangedCallbacks;

  // Helper para registrar callbacks
  void RegisterCallback(uint32_t widgetId, const WidgetCallback &callback) {
    widgetCallbacks[widgetId] = callback;
  }

  void RegisterValueChanged(const std::string &widgetId,
                            const ValueChangedCallback &callback) {
    valueChangedCallbacks[widgetId] = callback;
  }

  Vec2 cursorPos;           // posición actual del layout vertical
  uint32_t frame = 0;       // contador de frames
  float deltaTime = 0.016f; // tiempo entre frames (default 60 FPS)
  float time = 0.0f;        // tiempo total transcurrido
  Vec2 lastItemPos{0.0f, 0.0f};
  Vec2 lastItemSize{0.0f, 0.0f};

  bool initialized = false;

  // Sistema de animaciones para widgets
  std::unordered_map<uint32_t, AnimatedValue<Color>> colorAnimations;
  std::unordered_map<uint32_t, AnimatedValue<float>> floatAnimations;

  // Sistema de ripple effects
  std::unordered_map<uint32_t, RippleEffect> rippleEffects;

  std::unordered_map<uint32_t, bool> boolStates;
  std::unordered_map<uint32_t, float> floatStates;
  std::unordered_map<uint32_t, int> intStates;
  std::unordered_map<uint32_t, std::string> stringStates;
  std::unordered_map<uint32_t, size_t> caretPositions;
  std::unordered_map<uint32_t, float> textScrollOffsets;

  struct PanelState {
    Vec2 position{0.0f, 0.0f};
    Vec2 size{300.0f, 200.0f};
    Vec2 reservedLayoutSize{0.0f, 0.0f};
    Vec2 expandedLayoutSize{300.0f, 200.0f};
    bool initialized = false;
    bool minimized = false;
    bool dragging = false;
    Vec2 dragOffset{0.0f, 0.0f};
    bool resizing = false;
    Vec2 resizeStartMouse{0.0f, 0.0f};
    Vec2 resizeStartSize{0.0f, 0.0f};
    bool useAcrylic = false; // Configuración específica del panel para acrylic
    float acrylicOpacity = 0.85f; // Opacidad específica del panel para acrylic
    bool useAbsolutePos =
        false; // Si se usó posición absoluta (pos != Vec2(0,0))
    Vec2 absolutePos{0.0f, 0.0f}; // Posición absoluta guardada
    Vec2 dragPositionOffset{0.0f, 0.0f}; // Offset from layout position when dragged
    bool hasBeenDragged = false; // Track if panel has been manually repositioned
  };

  std::unordered_map<uint32_t, PanelState> panelStates;

  struct ScrollViewState {
    Vec2 scrollOffset{0.0f, 0.0f};
    Vec2 contentSize{0.0f, 0.0f};
    Vec2 viewSize{0.0f, 0.0f};
    bool initialized = false;
    bool draggingScrollbar = false;
    int draggingScrollbarType = 0; // 0 = none, 1 = vertical, 2 = horizontal
    Vec2 dragStartMouse{0.0f, 0.0f};
    float dragStartScroll{0.0f};
    bool useAbsolutePos =
        false; // Si se usó posición absoluta (pos != Vec2(0,0))
    Vec2 absolutePos{0.0f, 0.0f}; // Posición absoluta guardada
    Vec2 position{0.0f, 0.0f}; // Posición del ScrollView (siempre guardada para
                               // restaurar cursor)
  };

  std::unordered_map<uint32_t, ScrollViewState> scrollViewStates;

  struct TabViewState {
    int activeTab = 0;
    Vec2 tabBarSize{0.0f, 0.0f};
    bool initialized = false;
    // Scroll soportado para contenido del tab - un scrollOffset por cada tab
    std::map<int, Vec2> tabScrollOffsets; // Mapa de índice de tab -> scrollOffset
    Vec2 contentSize{0.0f, 0.0f};
    Vec2 viewSize{0.0f, 0.0f};
    bool draggingScrollbar = false;
    Vec2 dragStartMouse{0.0f, 0.0f};
    float dragStartScroll = 0.0f;
    bool useAbsolutePos =
        false; // Si se usó posición absoluta (pos != Vec2(0,0))
    Vec2 absolutePos{0.0f, 0.0f}; // Posición absoluta guardada
  };

  std::unordered_map<uint32_t, TabViewState> tabViewStates;

  struct TooltipState {
    std::string text;
    Vec2 position{0.0f, 0.0f};
    float hoverTime = 0.0f;
    float delay = 0.5f;
    bool visible = false;
  };

  TooltipState tooltipState;

  struct ModalState {
    bool open = false;
    Vec2 position{0.0f, 0.0f};
    Vec2 size{400.0f, 300.0f};
    bool dragging = false;
    Vec2 dragOffset{0.0f, 0.0f};
    bool initialized = false;
  };

  std::unordered_map<uint32_t, ModalState> modalStates;

  struct ContextMenuState {
    Vec2 position{0.0f, 0.0f};
    Vec2 size{0.0f, 0.0f};
    bool open = false;
    bool initialized = false;
  };

  std::unordered_map<uint32_t, ContextMenuState> contextMenuStates;
  uint32_t activeContextMenuId = 0; // ID del context menu activo

  struct ListViewState {
    int selectedItem = -1;
    Vec2 itemSize{0.0f, 32.0f};
    bool initialized = false;
    bool useAbsolutePos =
        false; // Si se usó posición absoluta (pos != Vec2(0,0))
    Vec2 absolutePos{0.0f, 0.0f}; // Posición absoluta guardada
    // Scrollbar support
    float scrollOffset = 0.0f;
    bool draggingScrollbar = false;
    Vec2 dragStartMouse{0.0f, 0.0f};
    float dragStartScroll = 0.0f;
  };

  std::unordered_map<uint32_t, ListViewState> listViewStates;

  struct TreeViewState {
    Vec2 itemSize{0.0f, 24.0f};
    float indentSize = 20.0f;
    float expandButtonSize = 14.0f;
    bool initialized = false;
    bool useAbsolutePos =
        false; // Si se usó posición absoluta (pos != Vec2(0,0))
    Vec2 absolutePos{0.0f, 0.0f}; // Posición absoluta guardada
  };

  std::unordered_map<uint32_t, TreeViewState> treeViewStates;
  std::unordered_map<std::string, bool> treeNodeStates; // Map id -> isOpen

  struct MenuBarState {
    Vec2 position{0.0f, 0.0f};
    Vec2 size{0.0f, 0.0f};
    bool initialized = false;
  };

  MenuBarState menuBarState;

  struct MenuState {
    std::string id;
    Vec2 position{0.0f, 0.0f};
    Vec2 size{0.0f, 0.0f};
    bool open = false;
    bool hover = false;
    bool initialized = false;
  };

  std::unordered_map<uint32_t, MenuState> menuStates;
  uint32_t activeMenuId = 0; // ID del menú desplegable activo

  // Optimización: Caché de medidas de texto
  struct TextMeasurement {
    std::string text;
    float fontSize;
    Vec2 size;
  };
  mutable std::unordered_map<std::string, Vec2> textMeasurementCache;
  mutable uint32_t textCacheFrame = 0;
  static constexpr uint32_t TEXT_CACHE_MAX_AGE =
      60; // Limpiar caché cada 60 frames
  static constexpr size_t TEXT_CACHE_MAX_SIZE = 512; // Max entries before eviction

  // Optimización: Pool de IDs reutilizables
  std::vector<uint32_t> reusableIds;
  uint32_t nextId = 1;
  static constexpr uint32_t MAX_REUSABLE_IDS = 1000;

  uint32_t activeWidgetId = 0;
  ActiveWidgetType activeWidgetType = ActiveWidgetType::None;

  // Sistema de focus mejorado
  uint32_t focusedWidgetId = 0;
  std::vector<uint32_t> focusableWidgets; // Orden de widgets enfocables
  int focusIndex = -1;                    // Índice actual en focusableWidgets

  // Layout System State
  std::vector<LayoutStack> layoutStack;
  std::vector<OccupiedArea> globalOccupiedAreas;
  std::vector<Vec2> offsetStack;
  std::vector<TabContentFrame> tabFrameStack;

  // Deferred rendering for ComboBox dropdowns (to ensure they appear on top)
  struct DeferredComboDropdown {
    Vec2 fieldPos;
    Vec2 fieldSize;
    Vec2 dropdownPos;
    Vec2 dropdownSize;
    std::vector<std::string> items;
    int selectedIndex;
    uint32_t comboId;
    int* currentItemPtr;
    float fieldHeight;
  };
  std::vector<DeferredComboDropdown> deferredComboDropdowns;

  // Deferred rendering for Menu dropdowns (to ensure they appear on top)
  struct DeferredMenuItem {
    enum class Type { Item, Separator };
    Type type = Type::Item;
    std::string label;
    bool enabled;
    Vec2 pos;
    Vec2 size;
    Color bgColor;
    Color textColor;
  };

  struct DeferredMenuDropdown {
    Vec2 dropdownPos;
    Vec2 dropdownSize;
    uint32_t menuId;
    std::vector<DeferredMenuItem> items;
  };
  std::vector<DeferredMenuDropdown> deferredMenuDropdowns;
  std::vector<DeferredMenuItem> currentMenuItems;

  // ComboBox change tracking (deferred dropdowns notify change next frame)
  std::unordered_map<uint32_t, bool> comboBoxChanged;

  // Scroll consumed flag - reset each frame in NewFrame()
  bool scrollConsumedThisFrame = false;

  // GC for state maps
  std::unordered_map<uint32_t, uint32_t> lastSeenFrame;
  static constexpr uint32_t GC_INTERVAL = 300; // Run GC every N frames

  // Global statics moved from Widgets.cpp
  int treeViewDepth = 0;
  uint32_t currentTreeViewId = 0;
  std::vector<PanelFrameContext> panelStack;
  std::vector<ScrollViewFrameContext> scrollViewStack;

  // Stack to track which menu is currently being processed
  std::vector<uint32_t> menuIdStack;
  std::vector<size_t> menuItemStartIndexStack;
};

UIContext *CreateContext(SDL_Window *window);
UIContext *GetContext();
void DestroyContext();

void NewFrame(float deltaTime = 0.016f);
void Render();

} // namespace FluentUI
