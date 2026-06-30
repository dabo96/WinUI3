#pragma once

#include "Animation.h"
#include "Math/Color.h"
#include "Math/Vec2.h"
#include "Math/Rect.h"
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
#include <mutex>


namespace FluentUI {

class RenderBackend;
struct SharedResourcePool;

// ─── brief 13: regiones de hit-test de la TitleBar custom ─────────────────────
// El widget TitleBar() (hilo de UI) publica aquí la zona arrastrable (caption) y
// las exclusiones (caption buttons + contenido interactivo). El callback de
// SDL_SetWindowHitTest registrado por FluentApp las lee — y ese callback puede
// ejecutarse en el hilo de la cola de eventos del SO — de ahí el mutex. Las
// coordenadas están en el espacio del viewport del renderer, que coincide con el
// de SDL_GetWindowSize (= el de SDL_HitTest `area`), así que el callback no
// necesita convertir DPI. `active` se reinicia cada frame en NewFrame().
struct TitleBarHitRegions {
  std::mutex mutex;
  bool active = false;          ///< Un TitleBar() se construyó este frame.
  Rect caption;                 ///< Rect arrastrable (mover ventana).
  std::vector<Rect> exclusions; ///< Sub-rects NO arrastrables dentro del caption.
  float resizeBorder = 6.0f;    ///< Grosor (px) de los bordes de redimensión.
  bool resizable = true;        ///< Habilita zonas RESIZE_* en los bordes.
};

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
  // Horizontal offset applied by CollapsingHeader to subsequent siblings in a
  // vertical layout. Reset on each layout pop. Auto-resets when another header
  // at the same level is rendered.
  float collapseIndent = 0.0f;
  // brief 19 (WrapPanel): when true the layout flows children left→right like a
  // horizontal layout (isVertical is false) but AdvanceCursor wraps to a new line
  // when the next slot reaches the right edge. Orthogonal to isVertical; existing
  // Vertical/Horizontal/Grid stacks leave this false so their behaviour is
  // unchanged. The per-row bookkeeping lives in the parallel wrapStack entry.
  bool isWrap = false;
};

// brief 19: per-WrapPanel bookkeeping, pushed/popped in lockstep with the
// WrapPanel's LayoutStack (which has isWrap = true). AdvanceCursor reads/writes
// the top entry when the current layout stack is a wrap layout.
struct WrapFrameContext {
  Vec2 origin{0.0f, 0.0f};   // container origin (cursor at BeginWrapPanel)
  float left = 0.0f;         // left edge children wrap back to (contentStart.x)
  float availWidth = 0.0f;   // usable content width before wrapping
  float hGap = 8.0f;         // horizontal gap between items on a row
  float vGap = 8.0f;         // vertical gap between rows
  float rowHeight = 0.0f;    // tallest item in the current (open) row
  float maxWidth = 0.0f;     // widest row reached so far (for final size)
  float totalHeight = 0.0f;  // accumulated height of all rows incl. current
  Vec2 savedCursor{0.0f, 0.0f};
  Vec2 savedLastItemPos{0.0f, 0.0f};
  Vec2 savedLastItemSize{0.0f, 0.0f};
};

// brief 19: per-UniformGrid bookkeeping. Cells share an identical width; each
// cell receives a Fixed-width constraint and the cursor advances cell-by-cell,
// wrapping every `columns` children. Height is auto (tallest item per row).
struct UniformGridFrameContext {
  Vec2 origin{0.0f, 0.0f};
  int columns = 1;
  float cellWidth = 0.0f;
  float gap = 8.0f;
  int currentCell = 0;
  float rowHeight = 0.0f;    // tallest item in current row
  float totalHeight = 0.0f;  // committed height of completed rows
  Vec2 savedCursor{0.0f, 0.0f};
  Vec2 savedLastItemPos{0.0f, 0.0f};
  Vec2 savedLastItemSize{0.0f, 0.0f};
};

// brief 19: per-Canvas bookkeeping. Children are placed by explicit coordinates
// (their pos param, resolved against the canvas origin) rather than the flow
// cursor. A throwaway vertical LayoutStack is pushed so any AdvanceCursor calls
// from children mutate it instead of the parent layout.
struct CanvasFrameContext {
  uint32_t id = 0;
  Vec2 origin{0.0f, 0.0f};
  Vec2 size{0.0f, 0.0f};
  Vec2 savedCursor{0.0f, 0.0f};
  Vec2 savedLastItemPos{0.0f, 0.0f};
  Vec2 savedLastItemSize{0.0f, 0.0f};
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
  // Phase B4: drag threshold — small movements after mouse-down don't change the value.
  // Set to true once total mouse movement exceeds the threshold; cleared on release.
  bool dragThresholdPassed = false;
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
  // Phase C7: anchor row for shift-range multi-row selection
  int lastSelectedRow = -1;
  // Phase C7: horizontal scroll state (non-frozen columns only)
  float scrollOffsetX = 0.0f;
  bool draggingHScrollbar = false;
  Vec2 hDragStartMouse{0.0f, 0.0f};
  float hDragStartScroll = 0.0f;
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
  // Phase C7: forwarded fields from external TableState.
  int frozenColumns = 0;
  std::vector<int>* selectedRowsPtr = nullptr;
  // Anchor row id for shift-range selection (per-table)
  int lastSelectedRow = -1;
  // Phase C7: horizontal scroll for non-frozen columns
  float scrollOffsetX = 0.0f;
  float frozenContentWidth = 0.0f;   // Sum of widths of frozen columns
  float totalColumnsWidth = 0.0f;    // Sum of widths of all columns
  float dataAreaHeight = 0.0f;       // Height available for data rows (excludes h-scrollbar)
  bool needsHScrollbar = false;
  bool cellClipPushed = false;       // Whether TableSetCell has a pending clip to pop
};

// ─── brief 15 (Feedback): cola de toasts ──────────────────────────────────────
// Instancia viva de un toast en la cola global del contexto. Definida aquí (y no
// en UI/FeedbackWidgets.h) para que la cola sobreviva entre frames dentro de
// UIContext. El struct no depende de FeedbackWidgets.h salvo InfoSeverity, que se
// declara aquí como enum opaco (tipo completo: tamaño conocido = int) y se define
// con sus enumeradores en UI/FeedbackWidgets.h. Gestionada por ShowToast() /
// RenderToasts(). Ver brief 15.
enum class InfoSeverity : int; // definición real (enumeradores) en UI/FeedbackWidgets.h
struct ToastInstance {
  uint64_t id = 0;                 // id único incremental (apilado/animación estable)
  std::string title;
  std::string message;
  InfoSeverity severity{};         // value-init = Informational (0)
  float durationSec = 4.0f;        // tiempo visible antes de auto-descartarse
  std::string actionText;          // texto del botón de acción opcional
  std::function<void()> onAction;  // callback al pulsar la acción
  float age = 0.0f;                // tiempo visible acumulado (pausado por hover)
  float enterAnim = 0.0f;          // 0→1 progreso de entrada (fade/slide; sin brief 10)
  float exitAnim = 1.0f;           // 1→0 progreso de salida (fade) tras descartarse
  bool dismissed = false;          // marcado para salir (por tiempo, acción o cierre)
};

struct UIContext {
  Renderer renderer;
  InputState input;
  Style style;
  SDL_Window *window =
      nullptr; // Referencia a la ventana para obtener posición del mouse

  // brief 08: the render backend this context renders with (so multi-window code
  // can extract the shared GL context / Vulkan shared device from a parent ctx).
  RenderBackend *backend = nullptr;
  // brief 08 Part C: per-device shared resource pool. The main context owns it;
  // secondary windows reference it (not owned). null when not shared.
  SharedResourcePool *sharedResources = nullptr;
  bool ownsSharedResources = false; // true only for the device-owner context

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

  // ─── brief 15 (Feedback): cola global de toasts ────────────────────────────
  // Encolada por ShowToast() y consumida/renderizada por RenderToasts() (una vez
  // por frame, capa Overlay). Vive aquí para persistir el estado de animación y
  // el temporizador de auto-descarte entre frames. Ver ToastInstance (arriba).
  std::vector<ToastInstance> toasts;
  uint64_t nextToastId = 1;

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
  // Id del único ComboBox que puede estar abierto a la vez (0 = ninguno). Al
  // abrir uno se cierra cualquier otro automáticamente.
  uint32_t openComboId = 0;
  // Rectángulo en pantalla del dropdown del combo abierto. Sirve para que los
  // widgets que quedan DEBAJO del dropdown (renderizado diferido, encima) no
  // procesen el click que en realidad va dirigido a una opción del dropdown.
  Vec2 openComboDropdownPos;
  Vec2 openComboDropdownSize;
  // true mientras se procesa/dibuja el contenido interactivo de un overlay
  // (items de combo o de menú). Exime a ese contenido del bloqueo de input por
  // overlay, de modo que el propio overlay sí reciba los clicks de sus items.
  bool insideOverlayRender = false;
  // Rect persistente del dropdown del menú abierto (análogo a openComboId). Se
  // usa para bloquear los widgets de fondo aun cuando MenuItem cierre el menú a
  // mitad de frame: el rect persiste del frame anterior y se limpia al FINAL
  // del frame (en RenderDeferredDropdowns) cuando el menú ya no está abierto.
  uint32_t openMenuId = 0;
  Vec2 openMenuDropdownPos;
  Vec2 openMenuDropdownSize;
  std::unordered_map<uint32_t, float> floatStates;
  std::unordered_map<uint32_t, int> intStates;
  std::unordered_map<uint32_t, std::string> stringStates;
  std::unordered_map<uint32_t, size_t> caretPositions;
  std::unordered_map<uint32_t, size_t> selectionAnchors; // Selection anchor (start of selection)
  std::unordered_map<uint32_t, float> textScrollOffsets;

  // Per-TextInput multi-click tracking (double/triple-click word/line selection)
  struct TextClickInfo {
    uint64_t lastClickTime = 0;  // SDL_GetTicks of last click
    Vec2 lastClickPos{0, 0};
    int clickCount = 0;          // 1 = single, 2 = double, 3 = triple
  };
  std::unordered_map<uint32_t, TextClickInfo> textClickInfo;

  // Pending callback for the next TextInput call (consumed once).
  // Use std::any-like opaque pointer to avoid pulling Widgets.h here.
  void* pendingTextInputCallback = nullptr; // opaque: const FluentUI::TextInputCallback*
  uint32_t pendingTextInputCallbackMask = 0;

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
    bool hasBeenManuallyResized = false; // Track if user manually resized the panel
    Vec2 manualSize{0.0f, 0.0f}; // Size set by manual resize
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
    float tabBarScrollX = 0.0f;   // Horizontal scroll offset for tab bar overflow
    float totalTabsWidth = 0.0f;  // Total width of all tabs (calculated each frame)
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
  uint32_t activeModalId = 0; // Modal abierto (0 = ninguno): bloquea el fondo
  bool insideModal = false;   // true entre BeginModal/EndModal (contenido exento)

  struct ContextMenuState {
    Vec2 position{0.0f, 0.0f};
    Vec2 size{0.0f, 0.0f};       // Visual size (clamped to viewport)
    Vec2 contentSize{0.0f, 0.0f}; // Full content size (may exceed visual)
    float scrollOffset = 0.0f;
    bool open = false;
    bool initialized = false;
    // Saved parent state to restore after EndContextMenu
    Vec2 savedCursorPos{0.0f, 0.0f};
    Vec2 savedLastItemPos{0.0f, 0.0f};
    Vec2 savedLastItemSize{0.0f, 0.0f};
    std::vector<Renderer::ClipRect> savedClipStack;
    size_t savedLayoutStackSize = 0;
  };

  std::unordered_map<uint32_t, ContextMenuState> contextMenuStates;
  uint32_t activeContextMenuId = 0; // ID del context menu activo
  bool insideContextMenu = false;  // true entre BeginContextMenu/EndContextMenu

  // === Flyout state (brief 14) ===
  // Estado del Flyout genérico (popup anclado con contenido arbitrario). v1
  // single-open como ComboBox: solo uno abierto a la vez (activeFlyoutId).
  struct FlyoutState {
    bool open = false;
    Vec2 position{0.0f, 0.0f};      // Top-left dibujado en pantalla (frame anterior)
    Vec2 measuredSize{0.0f, 0.0f};  // Tamaño total de la card medido el frame anterior
    Rect anchor;                    // anchorRect del último BeginFlyout
    // Estado del padre guardado para restaurar en EndFlyout.
    Vec2 savedCursorPos{0.0f, 0.0f};
    Vec2 savedLastItemPos{0.0f, 0.0f};
    Vec2 savedLastItemSize{0.0f, 0.0f};
    std::vector<Renderer::ClipRect> savedClipStack;
    size_t savedLayoutStackSize = 0;
  };
  std::unordered_map<uint32_t, FlyoutState> flyoutStates;
  uint32_t activeFlyoutId = 0;   // Flyout abierto (0 = ninguno): captura input
  bool insideFlyout = false;     // true entre BeginFlyout/EndFlyout (contenido exento)

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
    // Scroll support
    float scrollOffset = 0.0f;
    float contentHeight = 0.0f; // Measured from previous frame
    Vec2 viewPos{0.0f, 0.0f};   // Position of the tree view area
    Vec2 viewSize{0.0f, 0.0f};  // Visual size of the tree view area
    // Scrollbar drag state (for interactive DrawScrollbar)
    bool draggingScrollbar = false;
    Vec2 dragStartMouse{0.0f, 0.0f};
    float dragStartScroll = 0.0f;
  };

  std::unordered_map<uint32_t, TreeViewState> treeViewStates;
  std::unordered_map<std::string, bool> treeNodeStates; // Map id -> isOpen

  // Phase C5: per-tree visit order and selection anchor for range-select.
  std::unordered_map<uint32_t, std::vector<int>> treeVisitOrder;
  std::unordered_map<uint32_t, int> treeLastSelectedId;

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
    // Phase C6: eyedropper mode — when true, the next click anywhere reads the framebuffer pixel.
    bool eyedropperActive = false;
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

  // Phase D: Drag-drop typed payload state
  struct DragDropState {
    bool active = false;             // True while a drag is in progress
    bool delivered = false;          // True for one frame when payload was accepted
    std::string payloadType;         // User-defined type tag (e.g. "FILE_PATH")
    std::vector<uint8_t> payloadBytes; // Raw payload bytes (POD copy)
    Vec2 startPos{0,0}, currentPos{0,0};
    uint32_t sourceWidgetId = 0;
    uint32_t hoverTargetId = 0;
    bool acceptedThisFrame = false;
    void* previewDrawCtx = nullptr;   // Opaque holder for std::function preview
  };
  DragDropState dragDrop;

  // Sistema de focus mejorado
  uint32_t focusedWidgetId = 0;
  std::vector<uint32_t> focusableWidgets; // Orden de widgets enfocables
  int focusIndex = -1;                    // Índice actual en focusableWidgets

  // Layout System State
  std::vector<LayoutStack> layoutStack;
  std::vector<Vec2> offsetStack;
  std::vector<TabContentFrame> tabFrameStack;

  // brief 19: layout-primitive stacks (parallel bookkeeping for WrapPanel /
  // UniformGrid / Canvas). Each is pushed in its Begin* and popped in End*.
  std::vector<WrapFrameContext> wrapStack;
  std::vector<UniformGridFrameContext> uniformGridStack;
  std::vector<CanvasFrameContext> canvasStack;

  // brief 21: ID scope stack. Each entry is a seed hash derived from the enclosing
  // scope + a discriminant (container id / item index / pointer). GenerateId mixes
  // the top-of-stack seed with the local label hash so duplicate labels in
  // different scopes no longer collide in widget state. With the stack empty the
  // seed is the djb2 base (5381) -> byte-identical IDs to pre-brief-21 behaviour,
  // so persisted per-frame state is preserved.
  std::vector<uint32_t> idStack;
  uint32_t CurrentIdSeed() const { return idStack.empty() ? 5381u : idStack.back(); }

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
    uint32_t iconCodepoint = 0;
  };

  // Deferred rendering for Overlays
  struct DeferredTooltip {
    std::string text;
    Vec2 pos;
    float fontSize;
    float opacity = 1.0f;
  };
  std::vector<DeferredTooltip> deferredTooltips;


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
  struct DeferredComboDropdown {
    Vec2 fieldPos;
    Vec2 fieldSize;
    Vec2 dropdownPos;
    Vec2 dropdownSize;
    std::vector<std::string> items; // Owned copy (safe against caller temporaries)
    std::vector<uint32_t> iconCodepoints; // Per-item icon (0 = none); empty = no icons
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

  // WantCaptureMouse — tracks whether mouse is over any widget (for host app to skip 3D picking)
  bool mouseOverAnyWidget = false;           // Set by IsMouseOver during current frame's widget rendering
  bool mouseOverAnyWidgetLastFrame = false;  // Previous frame's value, safe to query during event processing

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
    // Phase E3: drag-out callback fired when user releases outside the window.
    std::function<void(const std::string&, int, int)> onPanelDragOut;

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

  // brief 13: zonas de hit-test publicadas por TitleBar() y leídas por el callback
  // de SDL_SetWindowHitTest (ver TitleBarHitRegions arriba). active se limpia en
  // NewFrame y lo re-fija el widget cada frame.
  TitleBarHitRegions titleBarHit;

  // --- Phase B1: Last item published state ---
  // Each widget that returns a bool (button, textInput, slider, drag, checkbox, etc.)
  // publishes its state into lastItem before returning. The free functions IsItemActivated /
  // IsItemEdited / IsItemDeactivated / IsItemDeactivatedAfterEdit / IsItemHovered /
  // IsItemFocused / GetItemRect read from lastItem.
  struct LastItemData {
    uint32_t id = 0;
    Vec2 bboxMin{0, 0};
    Vec2 bboxMax{0, 0};
    bool hovered = false;
    bool active = false;          // Held / being interacted this frame
    bool focused = false;
    bool edited = false;          // Value changed this frame
    bool activated = false;       // Became active this frame (transition off→on)
    bool deactivated = false;     // Stopped being active this frame (transition on→off)
    bool deactivatedAfterEdit = false; // Deactivated AND edits happened during the active span
  };
  LastItemData lastItem;
  // Internal tracking: id → was-active-last-frame, plus edited-since-activation flag.
  std::unordered_map<uint32_t, bool> prevActiveItems;     // id → active last frame
  std::unordered_map<uint32_t, bool> editedSinceActivate; // id → edits seen during current active span

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

// Selects which RenderBackend the factory instantiates. Defaults to OpenGL so
// existing code is unaffected. Call SetPreferredBackend(RenderBackendType::Vulkan)
// before CreateContext()/CreateStandaloneContext() to use the Vulkan backend; in
// that case the `existingContext` argument is a VulkanSharedContext* (or nullptr
// for standalone) instead of an SDL_GLContext.
enum class RenderBackendType { OpenGL, Vulkan };
void SetPreferredBackend(RenderBackendType type);
RenderBackendType GetPreferredBackend();

UIContext *CreateContext(SDL_Window *window, void* existingGLContext = nullptr);
// Convenience overload: pick the backend and create the context in one call, so
// the backend choice and its matching `existingContext` handle can't get out of
// sync. For Vulkan pass a VulkanSharedContext* (shared mode) or nullptr
// (standalone); for OpenGL pass an SDL_GLContext or nullptr.
UIContext *CreateContext(SDL_Window *window, RenderBackendType backend, void* existingContext = nullptr);
UIContext *GetContext();
void DestroyContext();

// Wrap a host-owned GPU texture (e.g. an engine-rendered viewport) so it can be
// drawn with Image()/DrawImage(). Returns an opaque handle to pass to those APIs,
// or nullptr on failure / unsupported backend. The library does NOT take ownership
// of the underlying image — keep it alive and in the sampled layout while in use,
// and call DestroyExternalTexture() (or backend DeleteTexture) to free the wrapper.
//   Vulkan: nativeView = VkImageView; sampler = VkSampler (null → default linear);
//           layout = VkImageLayout when sampled (0 → SHADER_READ_ONLY_OPTIMAL).
void* RegisterExternalTexture(void* nativeView, void* sampler = nullptr, int layout = 0);
// Release a handle returned by RegisterExternalTexture (frees only the wrapper).
void DestroyExternalTexture(void* handle);

// Multi-context support (Phase 4: Multi-Window)
// SetCurrentContext swaps which context is used by NewFrame/Render/widgets
void SetCurrentContext(UIContext* ctx);
// Create a standalone context (not the global singleton) for secondary windows
// outBackend receives the created backend pointer (for cleanup)
UIContext* CreateStandaloneContext(SDL_Window* window, RenderBackend** outBackend = nullptr);
// brief 08: create a secondary-window context that SHARES the GPU device and
// resource pool of `shareFrom` (the main context). In OpenGL it reuses the same
// SDL_GLContext; in Vulkan it creates only a surface/swapchain over shareFrom's
// device. The font atlas/MSDF/textures are not duplicated. `shareFrom` must be a
// live context created by CreateContext(). Falls back to an isolated context if
// shareFrom is null or its backend can't be shared.
UIContext* CreateStandaloneContext(SDL_Window* window, UIContext* shareFrom,
                                   RenderBackend** outBackend = nullptr);
// Destroy a standalone context (does not touch the global singleton). Frees the
// backend but never the shared resource pool (owned by the main context).
void DestroyStandaloneContext(UIContext* ctx, RenderBackend* backend);

void NewFrame(float deltaTime = 0.016f);
void Render();

/// Returns true if the mouse was over any FluentUI widget last frame.
/// Host app should skip 3D picking / viewport interaction when this returns true.
bool WantCaptureMouse();

// --- Phase B1: Public item-state queries ---
// Read from UIContext::lastItem populated by the most recent widget call.
// All return false if no widget published state this frame.
bool IsItemHovered();
bool IsItemActive();
bool IsItemFocused();
bool IsItemEdited();
bool IsItemActivated();
bool IsItemDeactivated();
bool IsItemDeactivatedAfterEdit();
// Returns the bbox of the last published widget (zero-rect if none).
void GetItemRect(Vec2* outMin, Vec2* outMax);
// Internal: widget code calls this to publish its state into ctx->lastItem.
// edited is the per-frame value-changed flag returned by the widget.
void SetLastItem(uint32_t id, const Vec2& bboxMin, const Vec2& bboxMax,
                 bool hovered, bool active, bool focused, bool edited);

} // namespace FluentUI
