#pragma once

#include "Animation.h"
#include "Math/Color.h"
#include "Math/Vec2.h"
#include "InputState.h"
#include "Renderer.h"
#include "RippleEffect.h"
#include "core/WidgetTree.h"
#include "core/DockSystem.h"
#include <SDL3/SDL.h>
#include "Theme/Style.h"
#include <map>
#include <functional>
#include <cstdarg>


namespace FluentUI {

// Logging system
enum class LogLevel { Debug, Info, Warning, Error };

using LogCallback = std::function<void(LogLevel, const char*)>;

void SetLogCallback(LogCallback callback);
LogCallback GetLogCallback();

// Internal logging helper — formats and dispatches to callback (or stderr if none set)
void Log(LogLevel level, const char* fmt, ...);

// Perf Phase C: Performance counters for profiling
struct PerformanceCounters {
  uint32_t batchCount = 0;         // Total batches submitted to GPU
  uint32_t drawCalls = 0;          // Total DrawBatch/DrawLines calls
  uint32_t vertexCount = 0;        // Total vertices submitted
  uint32_t indexCount = 0;         // Total indices submitted
  uint32_t textCacheHits = 0;      // MeasureTextCached cache hits
  uint32_t textCacheMisses = 0;    // MeasureTextCached cache misses
  uint32_t flushCount = 0;         // Number of FlushBatch calls
  uint32_t batchMerges = 0;        // Successful batch merges
  uint32_t stateChanges = 0;       // Shader/texture state changes in EnsureBatchState
  uint32_t activeColorAnims = 0;   // Active color animations this frame
  uint32_t activeFloatAnims = 0;   // Active float animations this frame
  uint32_t widgetNodeCount = 0;    // WidgetTree total node count
  uint32_t clipPushes = 0;         // Number of PushClipRect calls
  float cpuFrameTimeMs = 0.0f;     // CPU time for the frame (set externally)

  void Reset() {
    batchCount = drawCalls = vertexCount = indexCount = 0;
    textCacheHits = textCacheMisses = flushCount = batchMerges = 0;
    stateChanges = activeColorAnims = activeFloatAnims = 0;
    widgetNodeCount = clipPushes = 0;
    cpuFrameTimeMs = 0.0f;
  }
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
};

struct TabContentFrame {
  uint32_t tabViewId;
  Vec2 contentAreaPos;
  Vec2 contentAreaSize;
  Vec2 contentStartCursor;
};

enum class ActiveWidgetType { None, Slider, TextInput, DragWidget };

struct DragWidgetState {
  bool isDragging = false;
  bool isEditing = false;
  float dragStartValue = 0.0f;
  float dragStartMouseX = 0.0f;
  std::string editText;
  uint64_t lastClickTime = 0;  // For double-click detection (SDL_GetTicks)
};

struct PanelFrameContext {
  uint32_t id;
  Vec2 layoutOrigin;
  float titleHeight;
  Vec2 clipPos;
  Vec2 clipSize;
  bool clipPushed;
  bool layoutPushed;
  bool isVisible = true;
  float userHeight = 0.0f;
  float maxHeight = 0.0f;
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

// TableColumn is defined in UI/Widgets.h
struct TableColumn;

// Internal table state (includes interaction details not exposed to user)
struct TableInternalState {
  int sortColumn = -1;       // -1 = no sort
  bool sortAscending = true;
  float scrollOffset = 0.0f;
  bool draggingScrollbar = false;
  Vec2 dragStartMouse{0.0f, 0.0f};
  float dragStartScroll = 0.0f;
  bool draggingColumnResize = false;
  int resizeColumnIndex = -1;
  float resizeStartX = 0.0f;
  float resizeStartWidth = 0.0f;
  bool initialized = false;
};

struct TableFrameContext {
  uint32_t id;
  Vec2 position;
  Vec2 size;
  TableColumn* columnsPtr = nullptr;  // Raw pointer to user's column array
  int columnCount = 0;
  int rowCount;
  int currentRow = -1;   // -1 = header row, 0+ = data rows
  int currentCol = 0;
  float headerHeight = 28.0f;
  float rowHeight = 28.0f;
  float scrollOffset = 0.0f;
  int startVisibleRow = 0;
  int endVisibleRow = 0;
  int sortColumn = -1;
  bool sortAscending = true;
  Vec2 savedCursor;
  Vec2 savedLastItemPos;
  Vec2 savedLastItemSize;
  bool clipPushed = false;
  float contentWidth = 0.0f;  // Total width of all columns
  float scrollbarWidth = 0.0f;
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

  // Perf 2.2: Track active animation IDs to avoid iterating all entries
  std::vector<uint32_t> activeColorAnimIds;
  std::vector<uint32_t> activeFloatAnimIds;
  std::vector<uint32_t> activeRippleIds;

  // Perf 2.2: Notify that an animation became active
  void NotifyColorAnimActive(uint32_t id) {
    for (auto aid : activeColorAnimIds) if (aid == id) return;
    activeColorAnimIds.push_back(id);
  }
  void NotifyFloatAnimActive(uint32_t id) {
    for (auto aid : activeFloatAnimIds) if (aid == id) return;
    activeFloatAnimIds.push_back(id);
  }
  void NotifyRippleActive(uint32_t id) {
    for (auto aid : activeRippleIds) if (aid == id) return;
    activeRippleIds.push_back(id);
  }

  // Sistema de ripple effects
  std::unordered_map<uint32_t, RippleEffect> rippleEffects;

  std::unordered_map<uint32_t, bool> boolStates;
  std::unordered_map<uint32_t, float> floatStates;
  std::unordered_map<uint32_t, int> intStates;
  std::unordered_map<uint32_t, std::string> stringStates;
  std::unordered_map<uint32_t, size_t> caretPositions;
  std::unordered_map<uint32_t, size_t> selectionAnchors; // Selection anchor (start of selection)
  std::unordered_map<uint32_t, float> textScrollOffsets;

  // Per-TextInput undo/redo stacks
  struct TextUndoEntry {
    std::string text;
    size_t caret;
    uint32_t frame;      // Frame when this entry was created (for coalescing)
  };
  struct TextUndoState {
    std::vector<TextUndoEntry> undoStack;
    std::vector<TextUndoEntry> redoStack;
    static constexpr size_t MAX_ENTRIES = 50;

    void PushUndo(const std::string& text, size_t caret, uint32_t frame) {
      // Coalesce entries within 30 frames (~500ms at 60fps)
      if (!undoStack.empty() && (frame - undoStack.back().frame) < 30) {
        undoStack.back() = {text, caret, frame};
      } else {
        undoStack.push_back({text, caret, frame});
        if (undoStack.size() > MAX_ENTRIES)
          undoStack.erase(undoStack.begin());
      }
      redoStack.clear();
    }
  };
  std::unordered_map<uint32_t, TextUndoState> textUndoStates;

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
    Vec2 contentSize{0.0f, 0.0f}; // Real content size measured in previous frame
    
    // Scroll support
    Vec2 scrollOffset{0.0f, 0.0f};
    bool draggingScrollbar = false;
    Vec2 dragStartMouse{0.0f, 0.0f};
    float dragStartScroll = 0.0f;
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
    uint32_t lastHoveredWidgetId = 0;
  };

  TooltipState tooltipState;

  struct ModalState {
    bool open = false;
    Vec2 position{0.0f, 0.0f};
    Vec2 size{400.0f, 300.0f};
    Vec2 minSize{400.0f, 300.0f};  // User-requested size as minimum
    Vec2 contentSize{0.0f, 0.0f};  // Measured content size from previous frame
    bool dragging = false;
    Vec2 dragOffset{0.0f, 0.0f};
    bool initialized = false;
    std::vector<Renderer::ClipRect> savedClipStack;
  };

  std::unordered_map<uint32_t, ModalState> modalStates;
  std::vector<uint32_t> modalStack;  // Track active modal IDs for EndModal

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
    // Multi-select support
    int lastClickedItem = -1;
    std::vector<int> selectedItems;
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

  // DragFloat / DragInt widget state
  std::unordered_map<uint32_t, DragWidgetState> dragStates;

  struct SplitterState {
    float ratio = 0.5f;
    bool isDragging = false;
    float dragStartMouse = 0.0f;
    float dragStartRatio = 0.0f;
  };

  struct ColorPickerState {
    bool initialized = false;
    float hue = 0.0f;        // 0-360
    float saturation = 1.0f;  // 0-1
    float value = 1.0f;       // 0-1
    float alpha = 1.0f;       // 0-1
    bool draggingSV = false;
    bool draggingHue = false;
    bool draggingAlpha = false;
    std::string hexText;
    bool editingHex = false;
  };

  std::unordered_map<uint32_t, ColorPickerState> colorPickerStates;

  std::unordered_map<uint32_t, SplitterState> splitterStates;

  struct SplitterFrameContext {
    uint32_t id;
    bool vertical;       // true = vertical divider (left|right), false = horizontal (top|bottom)
    float* ratioPtr;
    Vec2 position;
    Vec2 size;
    Vec2 firstRegionPos;
    Vec2 firstRegionSize;
    Vec2 secondRegionPos;
    Vec2 secondRegionSize;
    float dividerThickness;
    int phase;           // 0 = first panel, 1 = second panel
    Vec2 savedCursor;
    Vec2 savedLastItemPos;
    Vec2 savedLastItemSize;
    bool layoutPushed;
  };

  std::vector<SplitterFrameContext> splitterStack;

  struct GridFrameContext {
    uint32_t id;
    int columns;
    int currentCell = 0;
    float cellWidth;
    float rowHeight;
    Vec2 gridOrigin;
    Vec2 savedCursor;
    Vec2 savedLastItemPos;
    Vec2 savedLastItemSize;
    float maxRowHeight = 0.0f;  // Track tallest item in current row
    float totalHeight = 0.0f;   // Total height of all completed rows
  };

  std::vector<GridFrameContext> gridStack;

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
    Vec2 dropdownPos{0.0f, 0.0f};   // Last known dropdown position
    Vec2 dropdownSize{0.0f, 0.0f};  // Last known dropdown size
    bool open = false;
    bool hover = false;
    bool initialized = false;
  };

  std::unordered_map<uint32_t, MenuState> menuStates;
  uint32_t activeMenuId = 0; // ID del menú desplegable activo

  // Issue 12: Smart text cache with last-access tracking
  // Perf 1.1: Key is uint64_t hash (text_hash << 32 | fontSize_bits) — zero allocation
  struct TextCacheEntry {
    Vec2 size;
    uint32_t lastAccessFrame = 0;
  };
  mutable std::unordered_map<uint64_t, TextCacheEntry> textMeasurementCache;
  mutable uint32_t textCacheFrame = 0;
  static constexpr uint32_t TEXT_CACHE_EVICT_INTERVAL = 120; // Evict every 120 frames
  static constexpr uint32_t TEXT_CACHE_STALE_AGE = 600;      // Remove entries not accessed in 600+ frames
  static constexpr size_t TEXT_CACHE_MAX_SIZE = 2048;         // Perf 2.6: Increased from 512 to avoid eviction hitches

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
  std::vector<Vec2> offsetStack;
  std::vector<TabContentFrame> tabFrameStack;

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

  // Deferred rendering for Overlays
  struct DeferredTooltip {
    std::string text;
    Vec2 pos;
    float fontSize;
    float opacity = 1.0f;
  };
  std::vector<DeferredTooltip> deferredTooltips;

  struct DeferredModalContent {
    uint32_t id;
    std::string title;
    bool* openPtr;
    Vec2 size;
    Vec2 pos;
    // Callback or ID to render the content
  };
  // Modals use a more complex structure, for now we will just defer the Backdrop and the Modal Frame

  // Deferred context menus
  struct DeferredContextMenuContent {
    uint32_t id;
    Vec2 pos;
    Vec2 size;
    std::vector<DeferredMenuItem> items;
  };
  std::vector<DeferredContextMenuContent> deferredContextMenus;

  bool isRenderingOverlays = false;

  // Deferred rendering for ComboBox dropdowns
  // Perf 3.2: Store pointer to items instead of copying the vector
  struct DeferredComboDropdown {
    Vec2 fieldPos;
    Vec2 fieldSize;
    Vec2 dropdownPos;
    Vec2 dropdownSize;
    const std::vector<std::string>* items; // Pointer, valid until Render()
    int selectedIndex;
    int highlightIndex = -1; // Keyboard highlight (-1 = none)
    uint32_t comboId;
    int* currentItemPtr;
    float fieldHeight;
  };
  std::vector<DeferredComboDropdown> deferredComboDropdowns;

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

  bool anyTooltipHoveredThisFrame = false;

  // Mouse cursor management
  enum class CursorType { Arrow, IBeam, Hand, ResizeH, ResizeV, ResizeNESW, ResizeNWSE };
  CursorType desiredCursor = CursorType::Arrow;
  CursorType currentCursor = CursorType::Arrow;
  SDL_Cursor* systemCursors[7] = {};
  bool cursorsInitialized = false;

  // Scroll consumed flag - reset each frame in NewFrame()
  bool scrollConsumedThisFrame = false;

  // GC for state maps — Issue 11: amortized rotation
  std::unordered_map<uint32_t, uint32_t> lastSeenFrame;
  static constexpr uint32_t GC_MAP_COUNT = 13;     // Total maps to GC
  static constexpr uint32_t GC_ROTATE_INTERVAL = 10; // GC one map every N frames
  uint32_t gcMapIndex = 0;                           // Current map being GC'd

  uint32_t lastGeneratedId = 0;

  // --- Performance counters (Phase C) ---
  PerformanceCounters perfCounters;
  bool showDebugOverlay = false;

  // --- Style override stacks (Phase 6) ---
  std::vector<Style> styleStack;
  std::vector<ButtonStyle> buttonStyleStack;
  std::vector<PanelStyle> panelStyleStack;
  std::vector<Color> textColorStack;

  // Get the effective style (top of stack or default)
  const Style& GetEffectiveStyle() const {
      return styleStack.empty() ? style : styleStack.back();
  }
  const ButtonStyle& GetEffectiveButtonStyle() const {
      return buttonStyleStack.empty() ? style.button : buttonStyleStack.back();
  }
  const PanelStyle& GetEffectivePanelStyle() const {
      return panelStyleStack.empty() ? style.panel : panelStyleStack.back();
  }

  // --- Retained Widget Tree (Phase 1) ---
  WidgetTree widgetTree;

  // --- Dock System (Phase 4) ---
  DockSpace dockSpace;

  // --- Dock Drag State (Phase 8) ---
  struct DockDragState {
    bool isDragging = false;
    std::string panelId;         // Panel being dragged
    Vec2 dragOffset;             // Mouse offset from panel top-left
    Vec2 ghostPos;               // Current ghost panel position
    Vec2 ghostSize;              // Ghost panel size
    DockPosition hoverZone = DockPosition::Float;  // Which zone mouse is over
    std::string hoverTargetId;   // Which panel the dock zone belongs to
    bool showZones = false;      // Show dock zone indicators

    void Reset() {
      isDragging = false;
      panelId.clear();
      hoverZone = DockPosition::Float;
      hoverTargetId.clear();
      showZones = false;
    }
  };
  DockDragState dockDrag;

  // --- DPI Scaling (Phase 4) ---
  float dpiScale = 1.0f;  // Display scale factor (1.0 = 100%, 1.5 = 150%, 2.0 = 200%)

  // Global statics moved from Widgets.cpp
  int treeViewDepth = 0;
  uint32_t currentTreeViewId = 0;
  std::vector<PanelFrameContext> panelStack;
  std::vector<ScrollViewFrameContext> scrollViewStack;

  // Stack to track which menu is currently being processed
  std::vector<uint32_t> menuIdStack;
  std::vector<size_t> menuItemStartIndexStack;

  // Table/DataGrid state
  std::unordered_map<uint32_t, TableInternalState> tableStates;
  std::vector<TableFrameContext> tableStack;
};

UIContext *CreateContext(SDL_Window *window);
UIContext *GetContext();
void DestroyContext();

// Multi-context support (Phase 4: Multi-Window)
// SetCurrentContext swaps which context is used by NewFrame/Render/widgets
void SetCurrentContext(UIContext* ctx);
// Create a standalone context (not the global singleton) for secondary windows
// outBackend receives the created backend pointer (for cleanup)
UIContext* CreateStandaloneContext(SDL_Window* window, RenderBackend** outBackend = nullptr);
// Destroy a standalone context (does not touch the global singleton)
void DestroyStandaloneContext(UIContext* ctx, RenderBackend* backend);

void NewFrame(float deltaTime = 0.016f);
void Render();

} // namespace FluentUI
