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
#include "core/UIEvent.h" // WindowHandle (brief 20; no SDL in the public header)
#include "Theme/Style.h"
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <cstdarg>
#include <mutex>


namespace FluentUI {

class RenderBackend;
struct SharedResourcePool;

// ─── brief 13: regiones de hit-test de la TitleBar custom ─────────────────────
// El widget TitleBar() (hilo de UI) publica aquí la zona arrastrable (caption) y
// las exclusiones (caption buttons + contenido interactivo). El callback de
// the platform hit-test registrado por FluentApp las lee — y ese callback puede
// ejecutarse en el hilo de la cola de eventos del SO — de ahí el mutex. Las
// coordenadas están en el espacio del viewport del renderer, que coincide con el
// de the window size (= el de the hit-test `area`), así que el callback no
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
  // brief 18.5: captured from ctx->layoutDirection when this (horizontal) layout
  // is pushed. When true, AdvanceCursor flows children right→left from the right
  // edge of the container instead of left→right. Vertical layouts ignore it.
  bool rtl = false;
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
  uint64_t lastClickTime = 0;  // For double-click detection (the OS timer)
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
  WindowHandle window =
      nullptr; // Ventana (handle opaco; cast a native window handle en el .cpp de plataforma)

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

  // brief 22 (fase 2): los antiguos mapas paralelos colorAnimations/floatAnimations
  // se fundieron en WidgetState.colorAnim[]/floatAnim[] (ver GetWidgetState).
  // brief 22 (fase 9): en la práctica solo floatAnim[0] se usa (StaggeredAppear) y lo
  // conduce activeFloatAnimIds; colorAnim[] quedó sin usuarios y su driver/vector se
  // retiró (los campos del struct se conservan como reserva, ver WidgetState).

  // brief 10 Part B: global motion tokens (durationScale / reduceMotion / enabled).
  // Read via the free function MotionDuration(). Hosts can mutate it at runtime
  // (e.g. honor the OS "reduce animations" preference — see InitMotionFromOS()).
  MotionConfig motion;

  // brief 10 Part C: spring-based interactive animations (interruptible).
  // brief 22 (fase 2): fundidos en WidgetState.springColor[] (button bg/fg/border
  // ocupan springColor[0..2]); los conduce activeSpringColorIds (raw widget ids; el
  // driver recorre los 4 slots del WidgetState hallado).
  // brief 22 (fase 9): springFloat[] quedó sin usuarios y su driver/vector se retiró
  // (los campos del struct se conservan como reserva, ver WidgetState).

  // brief 10 Part D: presence tracker for managed overlays (enter/exit fade+scale).
  // Immediate-mode can't observe "stopped emitting" from inside NewFrame (the next
  // build hasn't run yet), so instead of inferring it from the retained tree's grace
  // period, the overlay system drives it explicitly: it calls BeginPresence(id,
  // active=open) EVERY frame. enterT animates 0→1 on appear and 1→0 on exit; the
  // entry self-erases once it has fully faded out. See BeginPresence() below.
  struct PresenceState {
    AnimatedValue<float> enterT{0.0f}; // 0=hidden, 1=shown
    bool everActive = false;
    bool exiting = false;
  };
  // brief 22 (fase 2): presenceStates fundido en WidgetState.presence. BeginPresence
  // usa GetWidgetState(id).presence; el "no existe entrada" se emula con
  // presence.everActive==false y el erase con reset a PresenceState{}.

  // brief 10 Part E: FLIP (First-Last-Invert-Play) layout animation state, keyed by
  // a stable per-item id. prevPos is the item's last on-screen position; when it
  // moves, `offset` is nudged by the delta and springs back to 0 so the item slides.
  // Opt-in & explicit: a list/container that wants animated reordering calls
  // LayoutFlipOffset(id, newPos) and adds the returned offset to its draw position.
  struct FlipState {
    Vec2 prevPos{0.0f, 0.0f};
    bool valid = false;
    SpringValue<Vec2> offset;
  };
  // brief 22 (fase 2): flipStates fundido en WidgetState.flip. LayoutFlipOffset usa
  // GetWidgetState(id).flip.

  // Perf 2.2: Track active animation IDs to avoid iterating all entries.
  // brief 22 (fase 9): se retiraron activeColorAnimIds y activeSpringFloatIds (y sus
  // Notify*): quedaron huérfanos tras la migración fases 2-8 (ningún widget usa
  // WidgetState.colorAnim[]/springFloat[] ni notificaba esos ids). Sobreviven los
  // tres vectores que SÍ se pueblan/usan: floatAnim (StaggeredAppear), springColor
  // (Button bg/fg/border) y ripple (Button.AddRipple; su Notify sigue disponible).
  std::vector<uint32_t> activeFloatAnimIds;
  std::vector<uint32_t> activeRippleIds;
  // brief 10 Part C: active spring ids (parallel to the tween vectors above).
  std::vector<uint32_t> activeSpringColorIds;

  // Perf 2.2: Notify that an animation became active
  void NotifyFloatAnimActive(uint32_t id) {
    for (auto aid : activeFloatAnimIds) if (aid == id) return;
    activeFloatAnimIds.push_back(id);
  }
  void NotifyRippleActive(uint32_t id) {
    for (auto aid : activeRippleIds) if (aid == id) return;
    activeRippleIds.push_back(id);
  }
  // brief 10 Part C: notify that a spring became active this frame.
  void NotifySpringColorActive(uint32_t id) {
    for (auto aid : activeSpringColorIds) if (aid == id) return;
    activeSpringColorIds.push_back(id);
  }

  // brief 10 Part G: true when any CPU-side animation is still running, so the host
  // loop can block on events (idle) instead of spinning. Union of the five active
  // vectors plus the retained tree's animation flag. Defined in Context.cpp.
  bool AnyAnimationActive() const;

  // Sistema de ripple effects
  // brief 22 (fase 2): rippleEffects fundido en WidgetState.ripple (raw widget id).

  // brief 22 (fase 3): estado bool primitivo fundido en WidgetState.boolVal (raw widget id).
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
  // brief 22 (fase 3): estados float/int/string primitivos fundidos en
  // WidgetState.floatVal/intVal/stringVal (raw widget id).
  // brief 22 (fase 4): caretPositions/selectionAnchors/textScrollOffsets fundidos
  // en WidgetState.text->{caret,anchor,scrollOffset} (ver GetTextState).

  // Per-TextInput multi-click tracking (double/triple-click word/line selection)
  // brief 22 (fase 4): la instancia por-widget vive en TextEditState::clickInfo;
  // el struct se conserva aquí porque TextEditState lo referencia por valor.
  struct TextClickInfo {
    uint64_t lastClickTime = 0;  // the OS timer of last click
    Vec2 lastClickPos{0, 0};
    int clickCount = 0;          // 1 = single, 2 = double, 3 = triple
  };

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
  // brief 22 (fase 4): la pila undo/redo por-widget vive en TextEditState::undo;
  // el struct se conserva aquí porque TextEditState lo referencia por valor.

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

  // brief 22 (fase 5): panelStates migrado a widgetStates (ws.panel) — GetPanelState(id).

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

  // brief 22 (fase 5): scrollViewStates migrado a widgetStates (ws.scroll) — GetScrollState(id).

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

  // brief 22 (fase 5): tabViewStates migrado a widgetStates (ws.tabs) — GetTabState(id).

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

  // brief 22 (fase 6): modalStates fundido en widgetStates (ws.modal) — GetModalState(id).
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

  // brief 22 (fase 6): contextMenuStates fundido en widgetStates (ws.ctxMenu) — GetCtxMenuState(id).
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
  // brief 22 (fase 6): flyoutStates fundido en widgetStates (ws.flyout) — GetFlyoutState(id).
  uint32_t activeFlyoutId = 0;   // Flyout abierto (0 = ninguno): captura input
  // Bugfix: id del scope Begin/EndFlyout en curso, independiente de activeFlyoutId.
  // CloseFlyout() puede poner activeFlyoutId=0 DENTRO del scope (al elegir una fila /
  // dismiss), así que EndFlyout debe desenrollar (restaurar clip/opacity/layout) según
  // este campo, no según activeFlyoutId, o si no la UI queda recortada a nada.
  uint32_t flyoutScopeId = 0;
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

  // brief 22 (fase 7): listViewStates migrado a widgetStates (ws.list) — GetListState(id).

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

  // brief 22 (fase 7): treeViewStates migrado a widgetStates (ws.tree) — GetTreeState(id).
  std::unordered_map<std::string, bool> treeNodeStates; // Map id -> isOpen
  // brief 22 (fase 7): treeNodeStates SE QUEDA — su clave es std::string
  // (TREENODE:<treeId>:<id>), no uint32_t, así que no cabe en WidgetState.

  // Phase C5 / brief 22 (fase 7): el orden de visita DFS y el ancla de selección
  // de rango viven ahora en WidgetState.treeVisitOrder / .treeLastSelectedId
  // (ver GetWidgetState).

  // brief 22 (fase 8): dragStates migrado a widgetStates (ws.drag) — GetDragState(id).

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

  // brief 22 (fase 8): colorPickerStates/splitterStates migrados a widgetStates
  // (ws.colorPicker / ws.splitter) — GetColorPickerState(id) / GetSplitterState(id).

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

  // brief 22 (fase 6): menuStates fundido en widgetStates (ws.menu) — GetMenuState(id).
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
  // brief 22 (fase 3): flag de cambio fundido en WidgetState.comboChanged (raw widget id).

  bool anyTooltipHoveredThisFrame = false;

  // Mouse cursor management
  enum class CursorType { Arrow, IBeam, Hand, ResizeH, ResizeV, ResizeNESW, ResizeNWSE };
  CursorType desiredCursor = CursorType::Arrow;
  CursorType currentCursor = CursorType::Arrow;
  void* systemCursors[7] = {}; // native cursor handle (opaque; cast in Context.cpp)
  bool cursorsInitialized = false;

  // Scroll consumed flag - reset each frame in NewFrame()
  bool scrollConsumedThisFrame = false;

  // WantCaptureMouse — tracks whether mouse is over any widget (for host app to skip 3D picking)
  bool mouseOverAnyWidget = false;           // Set by IsMouseOver during current frame's widget rendering
  bool mouseOverAnyWidgetLastFrame = false;  // Previous frame's value, safe to query during event processing

  // GC del estado por-widget. brief 22 (fase 9): retirado el GC rotatorio amortizado
  // (con sus 13→0 mapas paralelos), el mapa lastSeenFrame, GC_MAP_COUNT y gcMapIndex.
  // El único GC es ahora el del mapa unificado widgetStates (NewFrame): recorre las
  // entradas cada GC_ROTATE_INTERVAL frames y borra las no vistas en ese ventana de
  // retención (GetWidgetState refresca WidgetState.lastFrameSeen). El valor 10 iguala
  // el viejo threshold GC_MAP_COUNT(1) * GC_ROTATE_INTERVAL(10), sin cambio de retención.
  static constexpr uint32_t GC_ROTATE_INTERVAL = 10; // cadencia + ventana de retención (frames)

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

  // ======================================================================
  // === brief 18 — OS integration (drag-drop, IME, RTL) ==================
  // ======================================================================

  // brief 18.7: OS drag-and-drop sinks. When the OS drops files or text on this
  // window, FluentApp dispatches them here (per-window, since each context owns
  // its own InputState). dropPos is in window coordinates (same space as the
  // mouse / widget rects). Wire these from app code to react to dropped paths.
  std::function<void(const std::vector<std::string>&, Vec2)> onFilesDropped;
  std::function<void(const std::string&, Vec2)> onTextDropped;

  // brief 18.4: per-field IME ownership. The text field that currently holds the
  // caret claims IME each frame so OS text-input start/stop and the
  // candidate-window area (the IME area) follow focus instead of being
  // globally on. 0 = no field owns IME. See EnsureTextInputFocus().
  uint32_t imeOwnerId = 0;

  // brief 18.5: layout direction. RTL mirrors horizontal containers' X axis,
  // default text alignment and directional icons. Mirror via IsRTL().
  enum class LayoutDirection : uint8_t { LTR, RTL };
  LayoutDirection layoutDirection = LayoutDirection::LTR;
  bool IsRTL() const { return layoutDirection == LayoutDirection::RTL; }

  // brief 13: zonas de hit-test publicadas por TitleBar() y leídas por el callback
  // de the platform hit-test (ver TitleBarHitRegions arriba). active se limpia en
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
  // brief 22 (fase 8): prevActiveItems/editedSinceActivate migrados a WidgetState
  // (ws.prevActive / ws.editedSinceActivate) — ver SetLastItem en Context.cpp.

  // Global statics moved from Widgets.cpp
  int treeViewDepth = 0;
  uint32_t currentTreeViewId = 0;
  std::vector<PanelFrameContext> panelStack;
  std::vector<ScrollViewFrameContext> scrollViewStack;

  // Stack to track which menu is currently being processed
  std::vector<uint32_t> menuIdStack;
  std::vector<size_t> menuItemStartIndexStack;

  // Table/DataGrid state
  // brief 22 (fase 7): tableStates migrado a widgetStates (ws.table) — GetTableState(id).
  std::vector<TableFrameContext> tableStack;

  // ─── brief 22: estado unificado por-widget (FASE 1, aditiva) ────────────────
  // Objetivo del brief 22 (Opción B): fundir los ~35 mapas paralelos
  // unordered_map<uint32_t,X> en UN solo unordered_map<uint32_t, WidgetState>.
  // Esta fase 1 SOLO añade la infraestructura: el struct, el mapa (que queda
  // vacío hasta las fases 2-8) y los accesores. Los mapas viejos y el GC
  // rotatorio siguen intactos; cero cambio de comportamiento. Se define aquí,
  // al final de UIContext, para que TODOS los sub-structs que referencia por
  // valor / unique_ptr (PanelState … TableInternalState, PresenceState,
  // FlipState, TextClickInfo, TextUndoState) ya sean tipos completos.

  // Estado de edición de texto (se materializa en la fase 4). El centinela de
  // `anchor` ("sin selección") es SIZE_MAX — idéntico al que usa hoy
  // selectionAnchors (ver InputWidgets.cpp: try_emplace(id, SIZE_MAX)).
  struct TextEditState {
    size_t caret = 0;
    size_t anchor = SIZE_MAX;   // "sin selección" (== centinela de selectionAnchors)
    float scrollOffset = 0.0f;
    TextClickInfo clickInfo;    // multi-click (double/triple) tracking
    TextUndoState undo;         // pilas undo/redo
  };

  // Estado unificado de un widget. Los sub-estados pesados son lazy (unique_ptr
  // null hasta que se usan) para no inflar cada entrada. Los arrays de animación
  // tienen 4 slots porque AnimSlot(id, slot) alcanza slot=3 en el código real
  // (InputWidgets.cpp usa AnimSlot(wid, 3)); AnimSlot(id, 0..3) → [0..3].
  struct WidgetState {
    // comunes / primitivos (fases 3, 8)
    bool boolVal = false;
    float floatVal = 0.0f;
    int intVal = 0;
    std::string stringVal;
    bool comboChanged = false;
    bool prevActive = false;
    bool editedSinceActivate = false;
    // brief 22 (fase 7): TreeView range-select. Antes eran los mapas paralelos
    // treeVisitOrder[id] (orden DFS de nodos visitados este frame) y
    // treeLastSelectedId[id] (ancla de rango). treeLastSelectedSet emula la
    // prueba de existencia .find() del mapa original: false == "sin ancla aún"
    // (el default 0 de treeLastSelectedId no basta porque un nodeId puede ser 0).
    std::vector<int> treeVisitOrder;
    int treeLastSelectedId = 0;
    bool treeLastSelectedSet = false;
    // animaciones inline (fase 2): replican AnimSlot(id, 0..3).
    // brief 22 (fase 9): floatAnim[0] (StaggeredAppear) y springColor[0..2] (Button)
    // están en uso y se conducen por activeFloatAnimIds/activeSpringColorIds. colorAnim[]
    // y springFloat[] quedaron sin usuarios tras la migración; sus vectores/drivers se
    // retiraron. Se conservan los campos como reserva (coste ínfimo) por si un widget
    // futuro los reengancha con su Notify*/driver; hoy nunca se actualizan.
    AnimatedValue<Color> colorAnim[4];   // (reserva — sin driver, ver fase 9)
    AnimatedValue<float> floatAnim[4];
    SpringValue<Color> springColor[4];
    SpringValue<float> springFloat[4];   // (reserva — sin driver, ver fase 9)
    RippleEffect ripple;
    PresenceState presence;
    FlipState flip;
    // sub-estados pesados, lazy (fases 4-8) via unique_ptr (null hasta usarse)
    std::unique_ptr<TextEditState> text;
    std::unique_ptr<PanelState> panel;
    std::unique_ptr<ScrollViewState> scroll;
    std::unique_ptr<TabViewState> tabs;
    std::unique_ptr<ModalState> modal;
    std::unique_ptr<ContextMenuState> ctxMenu;
    std::unique_ptr<FlyoutState> flyout;
    std::unique_ptr<MenuState> menu;   // brief 22 (fase 6): MenuBar dropdown state
    std::unique_ptr<ListViewState> list;
    std::unique_ptr<TreeViewState> tree;
    std::unique_ptr<TableInternalState> table;
    std::unique_ptr<ColorPickerState> colorPicker;
    std::unique_ptr<SplitterState> splitter;
    std::unique_ptr<DragWidgetState> drag;
    uint32_t lastFrameSeen = 0;
  };

  std::unordered_map<uint32_t, WidgetState> widgetStates;

  // brief 22 (fase 9) — nota sobre la "doble verdad" con el WidgetNode del árbol:
  // WidgetNode (core/WidgetNode.h) tiene sus PROPIAS animaciones inline
  // (bgColorAnim/opacityAnim/enterT/scaleAnim/posSpring/prevBounds) conducidas por
  // WidgetTree::UpdateAnimations. Son DISJUNTAS de las de WidgetState: ningún archivo
  // de src/UI/*.cpp escribe los campos de animación del WidgetNode (solo WidgetNode.h
  // los actualiza internamente), mientras que las animaciones que los widgets sí usan
  // (springColor de Button, floatAnim de StaggeredAppear, ripple, presence, flip) viven
  // en WidgetState. Verificado por grep: no hay ningún widget que escriba AMBOS para el
  // mismo dato, así que NO existe doble-verdad real y no se fusiona nada (ver notes).

  // Accesores (definidos out-of-line en src/Core/Context.cpp donde los tipos
  // están completos). GetWidgetState SIEMPRE crea la entrada y refresca
  // lastFrameSeen = frame. Cada Get*State asegura su unique_ptr (make_unique si
  // es null) y devuelve la referencia al sub-estado.
  WidgetState& GetWidgetState(uint32_t id);
  TextEditState& GetTextState(uint32_t id);
  PanelState& GetPanelState(uint32_t id);
  ScrollViewState& GetScrollState(uint32_t id);
  TabViewState& GetTabState(uint32_t id);
  ModalState& GetModalState(uint32_t id);
  ContextMenuState& GetCtxMenuState(uint32_t id);
  FlyoutState& GetFlyoutState(uint32_t id);
  MenuState& GetMenuState(uint32_t id);
  ListViewState& GetListState(uint32_t id);
  TreeViewState& GetTreeState(uint32_t id);
  TableInternalState& GetTableState(uint32_t id);
  ColorPickerState& GetColorPickerState(uint32_t id);
  SplitterState& GetSplitterState(uint32_t id);
  DragWidgetState& GetDragState(uint32_t id);
};

// Selects which RenderBackend the factory instantiates. Defaults to OpenGL so
// existing code is unaffected. Call SetPreferredBackend(RenderBackendType::Vulkan)
// before CreateContext()/CreateStandaloneContext() to use the Vulkan backend; in
// that case the `existingContext` argument is a VulkanSharedContext* (or nullptr
// for standalone) instead of an GL context.
enum class RenderBackendType { OpenGL, Vulkan };
void SetPreferredBackend(RenderBackendType type);
RenderBackendType GetPreferredBackend();

UIContext *CreateContext(WindowHandle window, void* existingGLContext = nullptr);
// Convenience overload: pick the backend and create the context in one call, so
// the backend choice and its matching `existingContext` handle can't get out of
// sync. For Vulkan pass a VulkanSharedContext* (shared mode) or nullptr
// (standalone); for OpenGL pass an GL context or nullptr.
UIContext *CreateContext(WindowHandle window, RenderBackendType backend, void* existingContext = nullptr);
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
UIContext* CreateStandaloneContext(WindowHandle window, RenderBackend** outBackend = nullptr);
// brief 08: create a secondary-window context that SHARES the GPU device and
// resource pool of `shareFrom` (the main context). In OpenGL it reuses the same
// GL context; in Vulkan it creates only a surface/swapchain over shareFrom's
// device. The font atlas/MSDF/textures are not duplicated. `shareFrom` must be a
// live context created by CreateContext(). Falls back to an isolated context if
// shareFrom is null or its backend can't be shared.
UIContext* CreateStandaloneContext(WindowHandle window, UIContext* shareFrom,
                                   RenderBackend** outBackend = nullptr);
// Destroy a standalone context (does not touch the global singleton). Frees the
// backend but never the shared resource pool (owned by the main context).
void DestroyStandaloneContext(UIContext* ctx, RenderBackend* backend);

// brief 18.5: RTL layout direction control. SetLayoutDirection(RTL) mirrors the
// X axis of horizontal/grid containers (see AdvanceCursor) and directional icons
// (see MirrorDirectionalIcon). Complex bidi text shaping (Arabic/Indic via
// HarfBuzz) is out of scope — see brief 18 "Fuera de alcance".
void SetLayoutDirection(UIContext::LayoutDirection dir);
UIContext::LayoutDirection GetLayoutDirection();
bool IsLayoutRTL();

void NewFrame(float deltaTime = 0.016f);
void Render();

// brief 10 Part B: motion-duration policy. Returns base * g_ctx->motion.durationScale,
// or 0 when reduceMotion / !enabled. Returns base unchanged when there is no context.
// Also declared in Animation.h (forward) so AnimatedValue/SpringValue can call it
// without including Context.h (the Animation.h ↔ Context.h cycle).
float MotionDuration(float base);
// ─── brief 10 Part D — presence (enter/exit transitions) ─────────────────────
// Result of BeginPresence: t is the 0..1 presence factor (fade/scale), shouldDraw
// is false once a closed overlay has fully faded out (caller skips drawing and
// teardown), exiting is true while fading out.
struct PresenceResult { float t; bool shouldDraw; bool exiting; };
// Drive a managed overlay's enter/exit transition. Call EVERY frame with active =
// "is the overlay logically open". On the rising edge enterT animates 0→1
// (Decelerate); when active turns false it animates 1→0 (Accelerate) and shouldDraw
// stays true until the fade completes, so the overlay can keep re-drawing itself
// during the exit. Wrap the overlay's draw in renderer.PushOpacity(result.t).
PresenceResult BeginPresence(UIContext* ctx, uint32_t nodeId, bool active,
                             float enterResponse = 0.20f, float exitResponse = 0.14f);

// brief 10 Part E: FLIP helper. Pass a stable per-item id and the item's freshly
// computed top-left for this frame; returns the visual offset to ADD to the draw
// position so the item slides from its previous position to the new one. The first
// call for an id returns (0,0) and just records the position. Opt-in: containers
// that want animated reorder/insert call this per visible child.
Vec2 LayoutFlipOffset(UIContext* ctx, uint32_t itemId, const Vec2& currentPos,
                      float response = 0.32f, float dampingRatio = 0.85f);

// brief 10 Part F: stagger. StaggerDelaySeconds maps a child index to an entrance
// delay (index * staggerMs, capped so the last child still starts within capMs).
float StaggerDelaySeconds(int index, float staggerMs, float capMs = 120.0f);
// Per-item staggered entrance factor [0..1]. On an item's first appearance it starts
// a Decelerate tween 0→1 delayed by StaggerDelaySeconds(index, staggerMs); returns
// the current factor each frame (multiply into the item's opacity via PushOpacity,
// and/or use as a slide/scale factor). Backed by WidgetState.floatAnim[0] (brief 22).
float StaggeredAppear(UIContext* ctx, uint32_t itemId, int index, float staggerMs,
                      float enterResponse = 0.22f);

// brief 10 Part B: seed g_ctx->motion.reduceMotion from the OS accessibility flag
// (Windows: SPI_GETCLIENTAREAANIMATION). No-op / default false on other platforms.
// Safe to call once after CreateContext(); FluentApp::run() calls it for you.
void InitMotionFromOS();

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
