# INFORME COMPLETO: Free Animation Power

> **Versión:** 2.0.0 | **Fecha:** Julio 2026 | **Autor:** Eduardo Fierro Duque  
> **Licencia:** GPLv3 | **Lenguaje:** C++20 | **Framework:** Qt 6.5+ | **Tests:** 139/139

---

## ÍNDICE

1. [Resumen Ejecutivo](#1-resumen-ejecutivo)
2. [Arquitectura General](#2-arquitectura-general)
3. [Modelo de Datos - Core](#3-modelo-de-datos---core)
4. [Capa Engine](#4-capa-engine)
5. [Pipeline de Renderizado](#5-pipeline-de-renderizado)
6. [Capa UI V2](#6-capa-ui-v2)
7. [Capa IO](#7-capa-io)
8. [Capa Platform](#8-capa-platform)
9. [Sistema de Build](#9-sistema-de-build)
10. [Historial de Errores y Soluciones](#10-historial-de-errores-y-soluciones)
11. [Tests](#11-tests)
12. [Prototipo Python](#12-prototipo-python)
13. [Glosario Técnico](#13-glosario-técnico)
14. [Apéndices](#14-apéndices)

---

## 1. RESUMEN EJECUTIVO

### 1.1 ¿Qué es Free Animation Power?

Free Animation Power (FAP) es un **software de animación 2D de código abierto** que combina un motor de renderizado **híbrido raster + vectorial**. Está escrito en **C++20** sobre **Qt 6.5+ Widgets**, sin dependencias externas más allá de Qt, FFmpeg y GoogleTest.

### 1.2 Capacidades actuales

| Característica | Estado |
|----------------|--------|
| Herramientas de dibujo | 11 (Brush, Eraser, ColorPicker, Fill, Text, Line, Rectangle, Ellipse, Move, Select, Hand) |
| Herramientas especializadas | 6 adicionales (PencilRetouch, RulerLine, RulerEllipse, DeformMesh, TweenEdit, Audio) |
| Tipos de capa | 5 (Raster, Vector, Group, Audio, Camera) |
| Modos de blending | 12 (Normal, Multiply, Screen, Overlay, Add, Subtract, Darken, Lighten, ColorBurn, ColorDodge, SoftLight, HardLight) |
| Presets de pincel | 5 built-in (Round Soft, Round Hard, Flat, Calligraphy, Eraser) |
| Presión de tableta | Wacom: pressure, tilt, rotation, eraser detection |
| Onion skinning | Prev/Next frames con tintes rojo/azul, opacidad configurable |
| Timeline | 24 fps default, playback, scrubbing, thumbnails, keyframes |
| Texto in-canvas | Edición directa con fuente, cursor, selección, caret blink |
| Undo/Redo | Stack de 128 entradas, snapshots completos de capa |
| Selección | Rectangular, flotante, copy/paste/cut/delete, clipboard QImage |
| Formatos de archivo | FAP (directorio JSON+PNG), FA2D (ZIP contenedor vía miniz) |
| Exportación | MP4 H.264, GIF animado, secuencia PNG (vía FFmpeg) |
| Importación | Photoshop .abr brushes (v1, v2, v6 con descompresión RLE) |
| Texturas de papel | PGM y BMP con muestreo bilineal |
| Deformación | Mesh bilineal/perspectiva/rígida para raster y vector |
| Tweening | 8 curvas de easing, 6 propiedades interpolables |
| Composición nodal | Node graph con Input, Output, Blend, Transform, Filter, Group nodes |
| Guías de regla | Lineal, radial, espejo, perspectiva con snapping |
| Editor de color | Paleta dinámica con 9 variaciones (monocromática/análoga/triádica) |

### 1.3 Estructura del proyecto

```
FREE ANIMATION POWER/
├── src/
│   ├── main.cpp              # Punto de entrada
│   ├── core/                 # 10 archivos - Modelo de datos puro (sin Qt excepto tool_state)
│   ├── engine/               # 25 archivos - Motores sin dependencia Qt
│   │   ├── common/           # blend_utils.hpp
│   │   ├── raster/           # RasterEngine + raster_stroke
│   │   ├── vector/           # BezierPath + VectorEngine + vector_stroke
│   │   ├── brush/            # BrushEngine, ABR importer, PaperTexture, PencilRetouch, RulerGuide
│   │   ├── animation/        # AudioEngine, TweenEngine, FrameThumbnail, OnionSkin, Playback, TimelineEngine
│   │   ├── compositor/       # Compositor, NodeGraph
│   │   └── deformation/      # DeformationEngine, DeformationMesh
│   ├── ui/                   # LEGACY V1 - no compilado actualmente (10 archivos)
│   ├── ui_v2/                # ACTIVO - 16 archivos (8 widgets + 8 implementaciones)
│   ├── io/                   # 5 archivos - File format, DocumentManager, VideoExport, Serialization
│   └── platform/             # 4 archivos - InputManager, TabletHandler
├── src_py/                   # 9 archivos - Prototipo Python (referencia)
├── tests/                    # 17 archivos - 139 tests GoogleTest
├── docs/                     # Documentación técnica
├── resources/                # Brushes, fuentes, iconos, paletas, texturas
├── icons/                    # Iconos de barra superior/toolbox/paneles
└── CMakeLists.txt            # Build system
```

### 1.4 Métricas del código

| Métrica | Valor |
|---------|-------|
| Archivos fuente (.cpp/.hpp) | ~60 en src/ |
| Archivos de test | 17 |
| Tests pasando | 139/139 |
| Líneas de código (estimado) | ~18,000 en src/ |
| Documentación | 10+ archivos Markdown |
| Rama activa | `master` (tras revertir 41 commits de la sesión de julio 2026) |
| Backup de emergencia | `backup-fixes-20260702-083912` |

---

## 2. ARQUITECTURA GENERAL

### 2.1 Diagrama de capas

```
┌──────────────────────────────────────────────────────────────┐
│                    UI LAYER (src/ui_v2/)                      │
│  MainWindowV2 → CanvasWidgetV2, TimelinePanelV2,             │
│  ToolboxPanelV2, LayerPanelV2, ColorPanelV2, PropertyEditorV2│
│  (Depende de Qt 6 Widgets + AppState compartido)             │
├──────────────────────────────────────────────────────────────┤
│                   ENGINE LAYER (src/engine/)                  │
│  raster/, vector/, brush/, animation/, compositor/,          │
│  deformation/, common/                                        │
│  (Sin dependencia Qt - puro C++20 estándar)                  │
├──────────────────────────────────────────────────────────────┤
│                  CORE DATA MODEL (src/core/)                  │
│  Document, Layer, Stroke, Canvas, Timeline, UndoManager,     │
│  ToolState, AppState, Types                                   │
│  (ToolState y AppState usan QObject/Q_PROPERTY)              │
├──────────────────────────────────────────────────────────────┤
│                PLATFORM LAYER (src/platform/)                 │
│  InputManager, TabletHandler                                  │
│  (Adapta eventos Qt → structs engine-agnósticos)             │
└──────────────────────────────────────────────────────────────┘
```

### 2.2 Principio de diseño fundamental

**Separación estricta Engine/Core ↔ UI/Qt:**

- Todo el código en `src/engine/` y la mayoría de `src/core/` **no dependen de Qt**
- `ToolState` y `AppState` son las únicas clases en core que usan QObject (para signals/slots reactivos)
- El engine usa tipos propios (`fap::Color`, `fap::Vec2`, `fap::Rect`) en lugar de QColor/QPointF/QRectF
- La UI convierte entre tipos Qt y tipos engine en los bordes

### 2.3 Flujo de datos

```
Mouse/Tablet Event (Qt)
     │
     ▼
InputManager::processMouseEvent() / processTabletEvent()
     │  Convierte QMouseEvent/QTabletEvent → InputEvent (struct engine)
     ▼
CanvasWidgetV2::mousePressEvent/Move/Release()
     │  Convierte coordenadas widget → canvas (widgetToCanvas)
     │  Gestiona estado de herramienta activa
     ▼
RasterLayer::blendPixel() / setPixel()
     │  Escribe al pixelBuffer_ (DATA domain)
     ▼
buildBackgroundCache()
     │  Compone: white bg + onion skin + todas las capas → backgroundCache_ (DISPLAY)
     ▼
paintEvent()
     │  Dibuja: backgroundCache_ + strokeBuffer_ (overlay) + grid + cursores UI
     ▼
QPainter → Pantalla
```

### 2.4 Convenciones de código

- **Namespace:** Todo bajo `fap`
- **Headers:** `#pragma once` (no include guards tradicionales)
- **NonCopyable/NonMovable:** Clases base para prevenir copia/movimiento en objetos de datos
- **COW (Copy-on-Write):** `RasterLayer::pixelBuffer_` es `shared_ptr<PixelBuffer>`, se copia al escribir mediante `ensureUnique()`
- **UIDs:** `LayerUid` (uint64_t) generados con `std::atomic` `fetch_add`
- **Epoch:** `buffer_epoch_` (uint64_t) incrementado en cada modificación del buffer para invalidación de cachés
- **Estilo Qt:** dark theme (#2d2d30 fondo, #d4782a acento naranja)
- **Sin excepciones:** El engine evita excepciones, usa valores de retorno y asserts

### 2.5 El AppState central

`AppState` (`src/core/app_state.hpp:108`) es el **punto de agregación central** que contiene referencias a todos los subsistemas:

```cpp
class AppState : public QObject {
    Document document_;                    // Modelo de documento
    Timeline timeline_;                    // Control de timeline
    ToolState toolState_;                  // Estado de herramienta actual (QObject con signals)
    AudioEngine audioEngine_;              // Motor de audio
    RulerTool rulerTool_;                  // Guías de regla
    TweenEngine tweenEngine_;             // Interpolación
    DeformationEngine deformationEngine_;  // Deformación de mesh
    FrameThumbnailCache thumbnailCache_;   // Caché LRU de thumbnails
    FrameViewerData frameViewerData_;      // Metadatos de frames
    PencilRetouchEngine pencilRetouch_;    // Retoque de lápiz
};
```

Se comparte como `std::shared_ptr<AppState>` entre todos los widgets de la UI.

---

## 3. MODELO DE DATOS - CORE

### 3.1 types.hpp / types.cpp

**Archivo:** `src/core/types.hpp` (223 líneas) - Define todos los tipos fundamentales.

#### 3.1.1 Enums

```cpp
enum class LayerType { Raster, Vector, Group, Audio, Camera };
```
5 tipos de capa. Raster y Vector son capas de dibujo. Group contiene otras capas. Audio y Camera son tipos placeholder.

```cpp
enum class BlendMode { Normal, Multiply, Screen, Overlay, Add, Subtract,
                       Darken, Lighten, ColorBurn, ColorDodge, SoftLight, HardLight };
```
12 modos de blending, mapeados 1:1 con los modos de composición de QPainter.

```cpp
enum class ToolType { Brush, Eraser, ColorPicker, Fill, Text, Line, Rectangle,
                      Ellipse, Move, Select, Hand, PencilRetouch, RulerLine,
                      RulerEllipse, DeformMesh, TweenEdit };
```
17 herramientas. Las primeras 11 son las herramientas principales visibles en el toolbox. Las 6 restantes son herramientas especializadas accesibles programáticamente.

```cpp
enum class FillType { Solid, Fabric, Ramp };
```
3 tipos de relleno: sólido, tela (patrón) y rampa (degradado).

```cpp
enum class ColorVariationType { Monochromatic, Analogous, Triadic };
```
3 esquemas de variación de color para la paleta dinámica.

```cpp
enum class LineStyle { Solid, Tapered, Dashed, Dotted };
```
4 estilos de línea para herramientas de forma.

#### 3.1.2 Structs de Datos

**Color** - RGBA en punto flotante (0.0-1.0):
```cpp
struct Color {
    float r, g, b, a;
    static Color fromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    Color premultiplied() const;  // Multiplica RGB por alpha
};
```
El formato interno de píxeles es **ARGB32 premultiplied** (el estándar de Qt). `premultiplied()` convierte a este formato.

**Vec2** - Vector 2D con operadores completos:
```cpp
struct Vec2 {
    float x, y;
    // +, -, *, /, +=, -=, ==, !=
    float length(), lengthSquared();
    Vec2 normalized();
    float dot(), cross();
    static float distance(), lerp();
};
```

**Rect** - Rectángulo:
```cpp
struct Rect {
    float x, y, width, height;
    float left(), right(), top(), bottom();
    bool contains(px, py) / contains(Vec2);
    bool isEmpty();
    Rect intersect(other);
    Rect expanded(margin);  // Expande borders por margen
};
```

**StrokePoint** - Punto de trazo con datos de tableta:
```cpp
struct StrokePoint {
    Vec2 position;
    float pressure = 1.0f;   // 0.0 - 1.0
    float tilt_x = 0.0f;     // Wacom tilt
    float tilt_y = 0.0f;
    float rotation = 0.0f;   // Wacom rotation
    float timestamp = 0.0f;  // Tiempo del evento
};
```

**Size** - Dimensiones enteras:
```cpp
struct Size { int width, height; bool isValid(); };
```

#### 3.1.3 Clases Utility

```cpp
class NonCopyable { /* delete copy constructor/assignment */ };
class NonMovable  { /* delete move constructor/assignment */ };
```
Usadas como clases base para prevenir copia/movimiento. `Document`, `Stroke`, y `UndoManager` heredan de `NonCopyable`.

#### 3.1.4 Funciones Utility

```cpp
float clampf(v, lo, hi);                           // Clamp flotante
float lerpf(a, b, t);                              // Interpolación lineal
float smoothstepf(edge0, edge1, x);                // Curva S suave (Hermite)
```

---

### 3.2 layer.hpp / layer.cpp

**Archivo:** `src/core/layer.hpp` (145 líneas) + `layer.cpp` (~280 líneas)

#### 3.2.1 LayerUid y PixelBuffer

```cpp
using LayerUid = uint64_t;
LayerUid generateLayerUid();  // atomic fetch_add desde static global

struct PixelBuffer {
    std::vector<uint32_t> pixels;  // Formato ARGB32 premultiplied
};
```

#### 3.2.2 Layer (clase base abstracta)

```cpp
class Layer : public NonCopyable {
    LayerUid uid_;           // Identificador único global
    std::string name_;       // Nombre de capa
    LayerType type_;         // Tipo (Raster, Vector, Group, etc.)
    bool visible_ = true;    // Visibilidad
    float opacity_ = 1.0f;   // Opacidad [0, 1]
    BlendMode blend_mode_;   // Modo de blending
    bool locked_ = false;    // Bloqueo de edición
    virtual std::unique_ptr<Layer> clone() const = 0;  // Patrón prototipo
};
```

Métodos públicos: `uid()`, `name()`, `type()`, `visible()`, `opacity()`, `blendMode()`, `locked()`, `setName()`, `setVisible()`, `setOpacity()`, `setBlendMode()`, `setLocked()`.

`setOpacity` usa `std::clamp(o, 0.0f, 1.0f)` para garantizar rango.

#### 3.2.3 RasterLayer

La clase más compleja del modelo. Implementa **Copy-on-Write (COW)** y **crecimiento dinámico de buffer**.

```cpp
class RasterLayer : public Layer {
    int width_, height_;                         // Dimensiones actuales
    int originX_ = 0, originY_ = 0;              // Offset del buffer en espacio canvas
    std::shared_ptr<PixelBuffer> pixelBuffer_;   // COW: compartido entre clones
    uint64_t buffer_epoch_ = 0;                  // Contador de modificaciones
    
    static constexpr int kMaxDim = 10'000;       // Dimensión máxima
    static constexpr int kGrowthPad = 512;       // Padding de crecimiento
    static constexpr int kGuardBand = 2;         // Banda de guarda
};
```

**Sistema COW (Copy-on-Write):**

```cpp
void RasterLayer::ensureUnique() {
    if (pixelBuffer_.use_count() <= 1) return;   // Ya es único
    auto newBuf = std::make_shared<PixelBuffer>(pixelBuffer_->pixels);  // Copia profunda
    pixelBuffer_ = std::move(newBuf);
}
```

**Acceso a píxeles:**

```cpp
const uint32_t* pixelData() const;   // Acceso lectura
uint32_t* pixelData();               // Acceso escritura (el caller debe llamar ensureUnique)
std::span<const uint32_t> pixelSpan() const;   // C++20 span para iteración segura
std::span<uint32_t> mutablePixelSpan();
size_t pixelCount() const;
```

**Indexación:**

```cpp
size_t indexAt(int x, int y) const {
    int bx = x - originX_;  // Ajusta por offset del buffer
    int by = y - originY_;
    if (bx < 0 || bx >= width_ || by < 0 || by >= height_) return -1;  // Fuera de bounds
    return by * width_ + bx;
}
```

El sistema de coordenadas usa `originX_`/`originY_` como offset. El buffer no necesariamente empieza en (0,0) del canvas. Esto permite que el buffer crezca dinámicamente en cualquier dirección.

**Operaciones de píxel:**

`pixelAt(x, y)` - Lee un píxel, **des-premultiplica** el alpha para retornar valores RGB puros:
```cpp
Color RasterLayer::pixelAt(int x, int y) const {
    uint32_t p = pixelBuffer_->pixels[idx];
    uint8_t a = (p >> 24) & 0xFF;
    if (a == 0) return transparent;
    float invA = 255.0f / a;  // Inversa de alpha para des-premultiplicar
    // r = min(255, premul_r * invA), etc.
}
```

`setPixel(x, y, color)` - Escribe un píxel, **pre-multiplica** RGB por alpha antes de guardar:
```cpp
void RasterLayer::setPixel(int x, int y, const Color& color) {
    ensureUnique();
    float a = clamp(color.a, 0, 1);
    uint8_t r = (color.r * a) * 255;  // Pre-multiplicación
    // Empaqueta: b | (g << 8) | (r << 16) | (a << 24)
    ++buffer_epoch_;
}
```

`blendPixel(x, y, color)` - **Alpha blending estándar** (source-over) contra el píxel existente:
```cpp
void RasterLayer::blendPixel(int x, int y, const Color& color) {
    ensureUnique();
    float srcA = sa / 255.0f;
    float invSrcA = 1.0f - srcA;
    // out_r = sr * srcA + dr * invSrcA  (pre-multiplied)
    // out_a = sa + da * invSrcA
    ++buffer_epoch_;
}
```

`clear(color)` - Llena todo el buffer con un color (pre-multiplicado), incrementa epoch.

`resize(width, height)` - Redimensiona el buffer preservando píxeles existentes. Crea nuevo vector, copia la intersección del área antigua y nueva.

**Crecimiento dinámico - `ensureContains(x, y, w, h, pad)`:**
Este es el método más sofisticado de RasterLayer. Permite que el buffer crezca automáticamente cuando se dibuja fuera de sus bounds actuales.

Algoritmo:
1. Aplica `kGuardBand` (2px) alrededor del área requerida
2. Calcula nuevos bounds: `newOrigin = min(current, required)`, `newExtent = max(current, required)`
3. Si se expandió algún borde, aplica `kGrowthPad` (512px) extra en esa dirección
4. Limita a `kMaxDim` (10,000px) en cada eje
5. Crea nuevo buffer con `relocatePixels()` que copia los píxeles antiguos a su nueva posición

`relocatePixels()` copia la región de intersección entre el buffer viejo y el nuevo, mapeando coordenadas a través de los offsets `originX_/originY_`.

**clone()** - Crea una copia compartiendo el `pixelBuffer_` (COW). Solo copia los metadatos.

#### 3.2.4 VectorLayer

```cpp
class VectorLayer : public Layer {
    std::vector<VectorStroke> strokes_;  // Colección de trazos vectoriales
    void addStroke(), removeStroke(), clearStrokes();
    VectorStroke& strokeAt(index);
    size_t strokeCount() const;
};
```

`clone()` copia todos los strokes. Cada `VectorStroke` contiene un `BezierPath`, color, ancho, opacidad, y blend mode.

#### 3.2.5 GroupLayer

```cpp
class GroupLayer : public Layer {
    std::deque<std::unique_ptr<Layer>> layers_;  // Hijos ordenados
    void addLayer(), removeLayer(), moveLayer(), duplicateLayer();
    Layer* layerAt(index), layerByUid(uid);
    int indexOfLayer(layer) const;
    size_t layerCount() const;
};
```

`duplicateLayer(index)` clona la capa en index usando `clone()`, la inserta en index+1, renombra con sufijo " copy".

`clone()` hace deep clone recursivo de todos los hijos.

---

### 3.3 document.hpp / document.cpp

**Archivo:** `src/core/document.hpp` (58 líneas) + `document.cpp`

```cpp
class Document : public NonCopyable {
    int width_ = 1920, height_ = 1080;     // Tamaño de canvas
    int fps_ = 24, total_frames_ = 24;     // Configuración de animación
    bool modified_ = false;                // Flag de documento modificado
    std::map<int, unique_ptr<GroupLayer>> frames_;  // Datos por frame
    UndoManager undo_manager_;             // Stack de undo/redo
};
```

**Modelo de frames:** Cada frame es una `GroupLayer` independiente que contiene sus capas. La key del mapa es el índice del frame. Esto permite:
- Frames no contiguos (puede haber huecos)
- Cada frame tiene sus propias capas con nombres/visibilidad independiente
- Soporte para onion skinning (acceder frames anteriores/posteriores)

```cpp
GroupLayer& rootLayerForFrame(int frame) {
    // Crea el frame si no existe (emplace + ensureDefaultLayer)
    ensureDefaultLayer(*root);  // Añade "Layer 1" raster si está vacío
}
```

`shiftFrameData(fromFrame, delta)` - Reindexa frames. Útil para insertar/eliminar frames en el timeline.

---

### 3.4 stroke.hpp / stroke.cpp

```cpp
class Stroke : public NonCopyable {
    BrushMode mode_;  // Raster, Vector, Hybrid
    std::vector<StrokePoint> points_;  // Puntos capturados durante el trazo
    void addPoint(), clear();
};

class RasterStroke : public Stroke { /* TODO: rasterized pixel data */ };
```

Nota: `VectorStroke` se define en `engine/vector/bezier_path.hpp`, no aquí.

---

### 3.5 canvas.hpp / canvas.cpp

```cpp
class Canvas {
    int width_, height_;           // Tamaño del documento
    float zoom_ = 1.0f;            // [0.01, 64.0]
    float pan_x_ = 0, pan_y_ = 0;  // Offset de pan
    float rotation_ = 0;           // Rotación [0, 360)
    bool flip_h_ = false, flip_v_ = false;  // Espejo
};
```

**Transformaciones de coordenadas:**

`worldToPixel(wx, wy)` - Convierte coordenadas del mundo del documento a píxeles de pantalla:
1. Aplica flip horizontal/vertical
2. Aplica rotación alrededor del centro (cx, cy)
3. Aplica zoom y pan: `px = (wx - panX) * zoom`

`pixelToWorld(px, py)` - La inversa:
1. `wx = px / zoom + panX`
2. Aplica rotación inversa
3. Aplica flip inverso

`zoomAt(factor, cx, cy)` - Zoom hacia un punto fijo:
```cpp
panX = cx - ratio * (cx - panX);  // Mantiene cx fijo en pantalla
panY = cy - ratio * (cy - panY);
```

**NOTA:** La clase `Canvas` del core es un modelo de datos de viewport. NO se usa directamente en la UI actual. `CanvasWidgetV2` tiene su propia implementación inline de transformaciones viewport (`zoom_`, `offsetX_`, `offsetY_`, `rotationAngle_`, `flippedH_`).

---

### 3.6 timeline.hpp / timeline.cpp

```cpp
class Timeline {
    int current_frame_ = 0, total_frames_ = 1, fps_ = 24;
    bool playing_ = false, looping_ = false;
    std::vector<std::vector<bool>> keyframes_;  // [layer][frame]
    FrameCallback on_frame_changed_;  // std::function<void(int)>
};
```

Operaciones: `play()`, `pause()`, `stop()`, `nextFrame()`, `prevFrame()`, `goToStart()`, `goToEnd()`.

`setCurrentFrame(frame)` emite callback solo si el frame cambió.

---

### 3.7 undo_manager.hpp / undo_manager.cpp

```cpp
class UndoCommand {
    virtual void undo() = 0;
    virtual void redo() = 0;
};

class UndoManager {
    static constexpr size_t kMaxEntries = 128;
    std::vector<unique_ptr<UndoCommand>> undoStack_;
    std::vector<unique_ptr<UndoCommand>> redoStack_;
};
```

**Dos métodos de push:**
- `pushCommand(cmd)` - Ejecuta `redo()` inmediatamente, luego guarda
- `pushApplied(cmd)` - No ejecuta nada (la acción ya fue aplicada externamente). Se usa en `CanvasWidgetV2` cuando el deshacer es vía snapshot.

**Límite:** 128 entradas. Al exceder, elimina la más antigua (FIFO).

---

### 3.8 tool_state.hpp / tool_state.cpp

El archivo más extenso de core (171 líneas). Es un **QObject** con **40+ Q_PROPERTY** que actúa como binding reactivo entre la UI y el estado de herramienta.

```cpp
class ToolState : public QObject {
    Q_OBJECT
    Q_PROPERTY(ToolType activeTool READ activeTool WRITE setActiveTool NOTIFY activeToolChanged)
    Q_PROPERTY(QColor primaryColor READ primaryColor WRITE setPrimaryColor NOTIFY ...)
    Q_PROPERTY(int brushSize READ brushSize WRITE setBrushSize NOTIFY ...)
    Q_PROPERTY(int brushOpacity ...)  Q_PROPERTY(int brushHardness ...)
    Q_PROPERTY(QString brushShape ...)  Q_PROPERTY(bool pressureSize ...)
    Q_PROPERTY(bool pressureOpacity ...)  Q_PROPERTY(int stabilizerLevel ...)
    Q_PROPERTY(int canvasWidth ...)  Q_PROPERTY(int canvasHeight ...)
    Q_PROPERTY(bool onionEnabled ...)  Q_PROPERTY(int onionPrevFrames ...)
    Q_PROPERTY(int onionNextFrames ...)  Q_PROPERTY(int onionOpacity ...)
    Q_PROPERTY(int fillType ...)  Q_PROPERTY(QColor sampledColor ...)
    Q_PROPERTY(int colorVariationType ...)  Q_PROPERTY(int colorVariationCount ...)
    Q_PROPERTY(int lineStyle ...)  Q_PROPERTY(QString textString ...)
    Q_PROPERTY(QFont textFont ...)  Q_PROPERTY(int textLeading ...)
    Q_PROPERTY(int textTracking ...)  Q_PROPERTY(int textAlignment ...)
    Q_PROPERTY(bool textAntiAliasing ...)  Q_PROPERTY(bool textUnderline ...)
    Q_PROPERTY(bool textStrikethrough ...)
```

Cada propiedad tiene:
- **getter**: retorna el valor miembro
- **setter**: verifica cambio, actualiza miembro, emite 2 signals: la específica y `toolSettingsChanged()`
- **signal**: notifica cambios a la UI

`setActiveToolByIndex(index)` mapea 0=Brush, 1=Eraser, 2=ColorPicker, ..., 10=Hand.

Valores por defecto notables:
- Tool inicial: **Eraser** (decisión deliberada para evitar dibujo accidental)
- Brush size: 20px
- Brush hardness: 80%
- Onion skin: enabled, 3 prev frames, 1 next frame, 35% opacity
- Canvas: 1920x1080

---

### 3.9 app_state.hpp / app_state.cpp

```cpp
class AppState : public QObject {
    Q_OBJECT
public:
    Document& document();
    Timeline& timeline();
    ToolState& toolState();
    AudioEngine& audioEngine();
    RulerTool& rulerTool();
    TweenEngine& tweenEngine();
    DeformationEngine& deformationEngine();
    FrameThumbnailCache& thumbnailCache();
    FrameViewerData& frameViewerData();
    PencilRetouchEngine& pencilRetouch();
};
```

**Agregación pura** - no hace cómputo, solo provee acceso a subsistemas. Se pasa como `shared_ptr<AppState>` a todos los widgets para que compartan estado.

**Señales de conveniencia:**
- `documentLoaded()` - emitido al cargar/crear documento
- `frameChanged(int)` - re-emitido desde Timeline
- `toolChanged(int)` - re-emitido desde ToolState

---

### 3.10 project.cpp

Contiene funciones de inicialización del proyecto que configuran el estado inicial del `AppState` y `Document`, incluyendo carga de presets de pincel por defecto y configuración de dimensiones del canvas.

---

## 4. CAPA ENGINE

### 4.1 common/blend_utils.hpp

Define las operaciones de blending por software usadas por todo el engine.

```cpp
struct FloatPixel { float r, g, b, a; };  // Pixel en [0,1]

FloatPixel unpackPixel(uint32_t p);       // ARGB32 → FloatPixel
uint32_t packPixel(const FloatPixel& fp);  // FloatPixel → ARGB32 (con clamp+round)
```

**blendChannel(src, dst, mode)** - Implementa los 12 modos de blending:

| Modo | Fórmula |
|------|---------|
| Normal | `src` |
| Multiply | `src * dst` |
| Screen | `1 - (1-src)*(1-dst)` |
| Overlay | `dst<0.5 ? 2*src*dst : 1-2*(1-src)*(1-dst)` |
| Add | `min(1, src+dst)` |
| Subtract | `max(0, dst-src)` |
| Darken | `min(src, dst)` |
| Lighten | `max(src, dst)` |
| ColorBurn | `1 - min(1, (1-dst)/src)` |
| ColorDodge | `min(1, dst/(1-src))` |
| SoftLight | Fórmula compleja con 3 ramas (ver código) |
| HardLight | `src<0.5 ? 2*src*dst : 1-2*(1-src)*(1-dst)` |

**blendPixel(pixels, w, h, x, y, r, g, b, a, mode)** - Alpha blending con modo:
```cpp
blendedR = blendChannel(src.r, dst.r, mode);
finalA = src.a + dst.a * (1 - src.a);
result.r = src.a * blendedR + (1 - src.a) * dst.r;  // Compone resultado
```

---

### 4.2 raster/ - Motor Raster

#### RasterEngine

```cpp
class RasterEngine {
    int width_, height_;
    std::vector<uint32_t> pixels_;  // ARGB32 premultiplied
    void fill(const Color& color);  // Pre-multiplica y llena
    void clear();                   // Transparente (0x00000000)
};
```

Motor simple: buffer de píxeles lineal con fill/clear. Usado como target de composición.

#### raster_stroke.cpp

Este archivo contiene la lógica de **stampado de pinceles** (sin dependencia Qt). Funciones anónimas internas:

**stampBrush(target, cx, cy, radius, hardness, r, g, b, opacity):**
- Calcula bounding box de la estampa: `[cx - ceil(radius), cy - ceil(radius)]`
- Para cada píxel en el bbox:
  - Calcula distancia al centro normalizada: `dist = sqrt(dx² + dy²) / radius`
  - Si `dist >= 1.0`, skip
  - Calcula alpha del pincel: `alpha = 1.0 - smoothstep(hardness, 1.0, dist)`
  - Multiplica por opacity
  - Aplica `alphaBlendPixel` (source-over) sobre el píxel destino

**alphaBlendPixel(dst, r, g, b, a):**
- Desempaqueta RGBA del destino
- Calcula blending pre-multiplicado source-over puro
- `out_a = src_a + dst_a * (1 - src_a)`
- `out_r = (src_r * src_a + dst_r * dst_a * (1 - src_a)) / out_a`

**renderRasterStroke(target, points, preset):**
- Si es un solo punto: estampa un único dab
- Si son múltiples puntos: recorre segmentos entre puntos consecutivos
- Calcula `seg_dist = distance(p0.pos, p1.pos)`
- Calcula `spacing = preset.tip.spacing * preset.size` (mínimo 0.5px)
- Si `seg_dist < spacing`: solo estampa el punto inicial
- Si `seg_dist >= spacing`: genera `N = floor(seg_dist / spacing)` estampas intermedias
- Interpola linealmente posición, presión, size y opacity entre p0 y p1
- Aplica dynamics de presión si están activos

---

### 4.3 vector/ - Motor Vectorial

#### bezier_path.hpp/cpp

Implementa curvas de **Bézier cúbicas** y paths vectoriales.

**BezierPoint:** posición + tangentes de entrada/salida + flag smooth.

**CubicBezier:** 4 puntos de control (p0, p1, p2, p3).

**BezierPath:**
- `moveTo(x, y)` - Inicia nuevo subpath
- `lineTo(x, y)` - Segmento recto (genera Bézier con puntos de control a 1/3 y 2/3)
- `curveTo(cx1, cy1, cx2, cy2, x, y)` - Segmento curvo
- `closePath()` - Conecta al inicio del subpath
- `evaluateAt(t)` - Evalúa posición en t ∈ [0,1], mapea t global a segmento local
- `tangentAt(t)` - Tangente normalizada en t
- `flattenPoints(tolerance=0.5)` - Convierte curvas a polilínea usando subdivisión adaptativa

**Algoritmo de flattening (subdivisión adaptativa):**
1. Calcula `flatness(seg)` = desviación máxima del segmento respecto a la cuerda
2. Si `flatness <= tolerance`, añade `seg.p3` al output
3. Si no, subdivide el segmento en t=0.5 y recursa en ambas mitades

**totalLength()** - Longitud aproximada via flattening.

**VectorStroke:**
```cpp
struct VectorStroke {
    BezierPath path;
    Color color;
    float width = 2.0f;
    float opacity = 1.0f;
    BlendMode blend = BlendMode::Normal;
};
```

**createPathFromPoints(points, smoothing)** - Convierte puntos de stroke a Bézier path:
- Usa Catmull-Rom a Bézier: los puntos de control se derivan de los vecinos
- `tau = smoothing / 6.0` para puntos interiores
- `firstTau = smoothing / 3.0` para los extremos
- Si 1 punto: solo moveTo. Si 2: lineTo. Si 3+: curveTo con tangentes calculadas

#### vector_engine.hpp/cpp

**VectorEngine** - Colección de VectorStrokes con renderizado a raster.

**fillPolygonAA(poly, r, g, b, a, target, blend):**
Implementa **scanline rasterization con anti-aliasing**:
1. Construye edge table: para cada arista del polígono, calcula `dx = (p1.x - p0.x) / dy`
2. Para cada scanline y:
   - Añade edges que empiezan en y al active list
   - Elimina edges que terminan en y
   - Ordena active edges por x
   - Para cada par (i, i+1): calcula cobertura sub-píxel con `coverage = right - left`
   - Aplica `blendPixel` con alpha * coverage

**renderThickPolyline(pts, width, r, g, b, a, target, blend):**
- Para cada segmento de la polilínea:
  - Calcula la normal perpendicular: `(-dy/len, dx/len)`
  - Construye un quad: `[A+n*hw, A-n*hw, B-n*hw, B+n*hw]`
  - Rellena el quad con `fillPolygonAA`

**renderToRaster(target):**
- Itera todos los strokes, flattenea cada path, renderiza polilínea gruesa

---

### 4.4 brush/ - Motor de Pinceles

#### brush_engine.hpp/cpp

**BrushTip** - Definición de punta de pincel:
```cpp
struct BrushTip {
    std::string name, imagePath;
    float spacing = 0.15f;   // Espaciado entre dabs
    float hardness = 0.8f;   // Dureza del borde [0,1]
    float angle = 0.0f;      // Ángulo de rotación
    float roundness = 1.0f;  // Redondez [0,1] (1=círculo)
};
```

**BrushDynamics** - Comportamiento dinámico:
```cpp
struct BrushDynamics {
    bool pressure_opacity, pressure_size, pressure_flow;  // Sensibles a presión
    bool tilt_angle;                                       // Sensible a tilt
    float min_size, max_size;       // Rango de tamaño [0.1, 2.0] relativo
    float min_opacity, max_opacity;
    float min_flow, max_flow;
    float smoothing;                // Suavizado de trayectoria [0,1]
};
```

**BrushPreset** - Preset completo:
```cpp
struct BrushPreset {
    std::string name;
    BrushMode mode;
    BrushTip tip;
    BrushDynamics dynamics;
    Color color;
    float size, opacity, flow;
    BlendMode blend_mode;
    bool use_paper_texture;
    std::string paper_texture_path;
};
```

**BrushEngine:**
- `beginStroke(color, size)` - Inicia trazo
- `addPoint(StrokePoint)` - Añade punto
- `endStroke()` - Finaliza
- `loadPresets(path)` / `savePresets(path)` - Persistencia JSON con parser manual

**5 Presets predefinidos (brush_preset.cpp):**

| Preset | Size | Hardness | Roundness | Spacing | Smoothing |
|--------|------|----------|-----------|---------|-----------|
| Round Soft | 10 | 0.3 | 1.0 | 0.15 | 0.5 |
| Round Hard | 5 | 0.95 | 1.0 | 0.10 | 0.3 |
| Flat | 8 | 0.7 | 0.3 | 0.08 | 0.4 |
| Calligraphy | 12 | 0.6 | 0.15 | 0.05 | 0.6 |
| Eraser | 20 | 0.9 | 1.0 | 0.10 | 0.2 |

**Parser JSON manual (sin dependencia de librería JSON):**
El engine implementa un parser JSON ligero con funciones `json_detail::`:
- `find_key()`, `skip_to_value()` - Navegación
- `extract_string()`, `extract_float()`, `extract_int()`, `extract_bool()` - Extracción tipada
- `extract_float_array()` - Arrays de floats
- `extract_object()` - Objetos anidados (balance de llaves con soporte de strings)

**Formato de archivo de presets:**
```json
{
  "presets": [{
    "name": "Round Soft",
    "mode": 0,
    "size": 10.0,
    "opacity": 1.0,
    "color": [0.0, 0.0, 0.0, 1.0],
    "blend_mode": 0,
    "flow": 0.3,
    "tip": { "name": "round_soft", "spacing": 0.15, "hardness": 0.3, ... },
    "dynamics": { "pressure_opacity": true, "pressure_size": true, ... }
  }]
}
```

#### abr_importer.hpp/cpp

Importador de brushes **Photoshop .abr**:

**Formato del archivo:**
- Header: version (uint16 big-endian), subversion, numBrushes
- v1/v2: brush headers con bounding box, compresión (0=raw, 1=RLE)
- v6: lista de tamaños + datos RLE

**Estructuras (packed):**
```cpp
struct AbrHeader { uint16_t version; int32_t numBrushes; ... };
struct AbrBrushHeader { int32_t top, left, bottom, right; int8_t compression; int32_t dataSize; };
```

**decompressRle()** - Descompresión Run-Length Encoding:
- Lee pares (int16 count, ...)
- Si `n >= 0`: copia `n+1` bytes literales
- Si `n < 0`: repite el siguiente byte `-n+1` veces

**Limitaciones de seguridad:**
- `kMaxBrushes = 512` - Máximo de brushes a importar
- `kMaxBrushSize = 2500` - Dimensión máxima de brush
- Archivo máximo: 100MB

#### paper_texture.cpp

**PaperTexture** - Textura de papel para simulación de medios tradicionales.

Soporta:
- **PGM** (Portable Gray Map): P5 (binario) y P2 (ASCII), 8-bit y 16-bit
- **BMP** (Bitmap): 24-bit y 8-bit con paleta, top-down y bottom-up

**Muestreo bilineal:**
```cpp
float sample(float u, float v) const {
    // Coordenadas cíclicas (wrap)
    // Interpolación bilineal: top + (bottom - top) * ty
}
```

Convierte imágenes a escala de grises usando fórmula de luminancia:
`gray = (R*77 + G*150 + B*29) >> 8`

#### pencil_retouch.hpp/cpp

**PencilRetouchEngine** - Herramienta de retoque de lápiz post-dibujo.

```cpp
enum class RetouchMode {
    AdjustThickness,   // Modifica grosor de línea
    AdjustOpacity,     // Modifica opacidad
    SmoothLines,       // Suaviza bordes (blur local)
    RepaintColor       // Re-colorea
};
```

**retouchAt(layer, pos, pressure):**
- Calcula área de influencia circular con hardness
- Para cada píxel afectado, aplica la operación según el modo:
  - **AdjustThickness:** `alpha *= 1.0 + (intensity*2 - intensity) * 0.5`
  - **AdjustOpacity:** `alpha *= 1.0 + (intensity - 0.5) * 0.3`
  - **SmoothLines:** Promedia píxeles vecinos 3x3 con lerp
  - **RepaintColor:** lerp del color actual al color del brush

#### ruler_guide.hpp/cpp

**Sistema de guías con snapping:**

```cpp
enum class GuideType { Linear, Radial, Mirror, Perspective };
```

**LinearGuide** - Línea recta con snapping:
- Proyecta `pos` sobre la línea → punto más cercano
- Si hay 3+ puntos, considera 2 líneas (p0→p1, p0→p2)
- También hace snapping al punto reflejado (espejo sobre la línea): `pos - normal * 2*(pos·normal)`

**RadialGuide** - Líneas desde un centro:
- Define rays desde el centro
- Snapping: encuentra el ray más cercano, proyecta manteniendo distancia

**MirrorGuide** - Simetría bilateral:
- Define un eje (p1→p2)
- `mirror(pos) = pos - normal * 2*(pos·normal)` respecto al eje
- Snapping solo si la distancia al reflejo < threshold

**PerspectiveGuide** - Hasta 3 puntos de fuga:
- Snapping a vanishing points individuales
- Detección de líneas coincidentes (dot > 0.999)

**RulerTool** - Colección de guías:
- `snap(pos, threshold)` - Itera todas las guías, retorna el snap más cercano
- Respeta `snap_enabled_` y `enabled()` por guía

---

### 4.5 animation/ - Motor de Animación

#### audio_engine.hpp/cpp

**AudioEngine** - Soporte de audio para animación:
- Tracks de audio con clips
- Keyframes de volumen (automation)
- Sample rate configurable
- Playback sincronizado con timeline

#### tween_engine.hpp/cpp

**TweenEngine** - Interpolación entre keyframes:

```cpp
enum class EasingType { Linear, EaseIn, EaseOut, EaseInOut,
                        EaseInCubic, EaseOutCubic, EaseInOutCubic, Spring };
```

6 propiedades interpolables (Position, Scale, Rotation, Opacity, Color, Path).

**BreakdownPoseEngine** - Genera poses intermedias entre dos keyframes usando las curvas de easing.

#### onion_skin.cpp

**OnionSkinSettings:**
```cpp
int frames_before = 3;    // Frames previos a mostrar
int frames_after = 1;     // Frames posteriores
float opacity = 0.35f;    // Opacidad base
Color tint_before = {1.0, 0.15, 0.15, 1.0};  // Rojo para previos
Color tint_after = {0.15, 0.4, 1.0, 1.0};     // Azul para siguientes
```

**renderOnionSkin():**
- Para cada frame en rango [current-offset, current+offset]:
  - `distFactor = 1.0 - offset/(maxDist+1)` (atenúa con distancia)
  - `alpha = baseOpacity * distFactor`
  - Multiplica píxeles fuente por tint color
  - Compone sobre el output con alpha blending pre-multiplicado

#### playback.cpp

**PlaybackController** con 3 modos:
- **Forward:** frame++ hasta hi, luego loop a lo
- **Reverse:** frame-- hasta lo, luego loop a hi
- **PingPong:** rebota entre lo y hi, manteniendo dirección interna

#### timeline_engine.cpp

**TimelineEngine** - Control de tiempo basado en delta:
```cpp
void update(float deltaTime) {
    accumulated_ += deltaTime;
    while (accumulated_ >= frameDuration) {
        accumulated_ -= frameDuration;
        current_frame_++;
    }
}
```

#### frame_thumbnail.hpp/cpp

**FrameThumbnailCache** - Caché LRU de thumbnails:
- `getThumbnail(frame)` → thumbnail downscaled a tamaño fijo
- `preload()` para precarga asíncrona
- Invalidación por frame

**FrameViewerData** - Metadatos por frame (info de exposición, notas).

---

### 4.6 compositor/ - Composición

#### compositor.hpp/cpp

```cpp
void Compositor::composite(const GroupLayer& root, RasterEngine& target);
```

**compositeRasterLayer()** - Compone una RasterLayer sobre el target:
- Calcula intersección de bounds (con originX_/originY_)
- Para cada píxel en la intersección, aplica `blendPixel` con el blend mode de la capa
- Respeta `layer.opacity()` como multiplicador del alpha

**compositeLayer()** - Recursivo para GroupLayer (bottom-to-top via reverse iterator).

#### node_graph.hpp/cpp

Sistema de composición basado en **nodos**:

```cpp
enum class NodeType { Input, Output, Blend, Transform, Filter, Group };
```

**InputNode** - Fuente de datos (referencia a RasterLayer)
**OutputNode** - Destino final
**BlendNode** - Mezcla 2 entradas con blend mode configurable
**TransformNode** - Traslación, escala, rotación con pivot
**FilterNode** - Blur (3x3 kernel) e Invert
**GroupNode** - Agrupa múltiples entradas

**NodeGraph:**
- `connect(fromId, outputPort, toId, inputPort)`
- `evaluate(target)` - Evaluación top-down con caché por NodeId
- `evaluateNode()` recursivo con detección de ciclos (visited set)
- `findOutputNode()` busca el nodo Output

---

### 4.7 deformation/ - Deformación

#### deformation_engine.hpp/cpp

**DeformationEngine:**
- `createDefaultMesh(bounds, cols, rows)` - Crea mesh regular
- `deformRasterLayer(layer)` - Aplica mesh a capa raster
- `deformVectorLayer(layer)` - Aplica mesh a capa vectorial

`deformPixels()` - Mapea cada píxel destino a través de la mesh inversa con interpolación bilineal.

#### deformation_mesh.hpp/cpp

**DeformationMesh:**
```cpp
enum class MeshMode { Bilinear, Perspective, Rigid };
```
- `setBounds(rect)` - Define área de influencia
- `pointAt(col, row)` / `setPoint(col, row, pos)` - Manipulación de puntos de control
- `mapPoint(x, y)` - Mapea un punto a través de la deformación
- `cols()`, `rows()` - Dimensiones de la grilla

---

## 5. PIPELINE DE RENDERIZADO

### 5.1 El problema original

Durante 2 días de debugging (junio-julio 2026), 41 commits fueron creados y posteriormente revertidos. El síntoma: **artefactos de línea blanca** ("recuadro molesto") aparecían durante trazos de pincel en el `QOpenGLWidget`.

### 5.2 Causa raíz

**`QPainter` sobre `QOpenGLWidget` tiene bugs de composición con alpha premultiplicado.** Al dibujar una QImage ARGB32_Premultiplied sobre el fondo del widget, Qt internamente aplicaba blending incorrecto, produciendo bordes blancos o artefactos donde debería haber transparencia.

### 5.3 Solución final: Pipeline de 4 Buffers CPU

Se implementó una **separación estricta DATA/DISPLAY** con 4 buffers en CPU:

| Buffer | Dominio | Formato | Contenido | Escritura | Lectura |
|--------|---------|---------|-----------|-----------|---------|
| `RasterLayer::pixelBuffer_` | **DATA** | ARGB32_PM | Píxeles de capa (source of truth) | `setPixel()`, `blendPixel()`, `clear()`, `ensureContains()` | `buildBackgroundCache()`, `captureRect()`, undo snapshots |
| `strokeBuffer_` | **DATA** | ARGB32_PM | Dabs aislados del trazo actual | `drawBrushStamp()`, `drawLineStamps()` durante mouseMove | `paintEvent` como overlay |
| `backgroundCache_` | **DISPLAY** | ARGB32_PM | Fondo blanco + onion skin + capas compuestas | `buildBackgroundCache(rect)` | `paintEvent` |
| `paintEvent` QPainter | **DISPLAY** | Widget | backgroundCache + strokeBuffer overlay + grid + UI | Solo paintEvent | Solo pantalla |

### 5.4 Invariantes del pipeline

1. **`mouseReleaseEvent` NUNCA lee de display buffers** - Solo lee `strokeBuffer_` y `pixelBuffer_`
2. **Dabs aislados durante el trazo** - Se escriben a `strokeBuffer_` (transparente ARGB32_PM), no directamente a `pixelBuffer_`
3. **Composición al commit** - `commitStroke()` compone `strokeBuffer_` sobre `pixelBuffer_` usando `QPainter` con `CompositionMode_SourceOver`
4. **`buildBackgroundCache(rect)`** - Soporta reconstrucción parcial (dirty rect) con offset-awareness para capas con `originX_`/`originY_`
5. **Padding dinámico** - `brushSize/2 + 1` píxeles para cubrir bordes de feathering

### 5.5 Flujo de renderizado detallado

**Durante un trazo (mouseMoveEvent):**
```
1. drawBrushStamp(cx, cy)
   → QPainter sobre strokeBuffer_ (CompositionMode_Source)
   → Dibuja elipse/círculo con gradiente radial suave
   → Invalida strokeDirtyRect_ (unión de bounding boxes)
   
2. paintEvent()
   → Si !backgroundCacheValid_: buildBackgroundCache()
   → drawImage(0, 0, backgroundCache_)      // Capa base compuesta
   → drawImage(0, 0, strokeBuffer_)         // Overlay del trazo activo
   → Dibuja grid, cursores de forma, selección, UI overlays
```

**Al soltar (mouseReleaseEvent):**
```
1. commitStroke()
   → Captura snapshot de pixelBuffer_ ANTES (beforeSnapshot_)
   → QPainter sobre pixelBuffer_ (como QImage):
     drawImage(0, 0, strokeBuffer_) con SourceOver
   → Captura snapshot DESPUÉS (afterSnapshot_)
   → Crea PaintCommandV2 con ambos snapshots + dirtyRect
   → undoManager.pushApplied(paintCmd)
   
2. Limpia strokeBuffer_ (fill transparente)
3. strokeDirtyRect_ = QRect()
4. invalidateBackgroundCache()
5. update()
```

### 5.6 buildBackgroundCache() - Reconstrucción completa y parcial

**Reconstrucción COMPLETA (rect vacío):**
```cpp
1. backgroundCache_.fill(0xFFFFFFFF);  // Fondo blanco
2. QPainter p(&backgroundCache_);
3. // Onion skin PREV (rojo): frames [current-prevCount, current-1]
   alpha = onionOpacity/100 * (1 - (i-1)/prevCount)
   Para cada frame previo: tint rojo + DestinationIn + drawImage
4. // Onion skin NEXT (azul): frames [current+1, current+nextCount]
   alpha = onionOpacity/100 * (1 - (i-1)/nextCount)
   Para cada frame siguiente: tint azul + DestinationIn + drawImage
5. // Capas del frame actual (bottom → top con reverse iteration)
   Para cada RasterLayer visible:
     p.setOpacity(layer.opacity())
     p.setCompositionMode(toQtCompositionMode(layer.blendMode()))
     drawImage(layer.originX(), layer.originY(), pixelBuffer→QImage)
```

**Reconstrucción PARCIAL (dirty rect):**
```cpp
1. p.setCompositionMode(Source); p.fillRect(rect, white);  // Sobrescribe R
2. Onion skin: solo layers que intersectan R
   → inter = rect.intersected(layerBounds)
   → srcRect = inter.translated(-originX, -originY)
   → drawImage(inter.topLeft(), display, srcRect)
3. Capas actuales: mismas verificaciones de intersección
   → offset-aware: usa srcRect para extraer sub-región correcta
```

### 5.7 Gestión de undo en el pipeline

**PaintCommandV2** - Comando de undo/redo basado en snapshots:
```cpp
class PaintCommandV2 : public UndoCommand {
    LayerUid layerUid_;
    int frame_, layerIndex_;
    QImage beforePixels_, afterPixels_;  // Snapshots ARGB32_PM
    QRect dirtyRect_;
    std::function<RasterLayer*()> layerAccessor_;  // Resolución lazy
    
    void undo() { applySnapshot(beforePixels_); }
    void redo() { applySnapshot(afterPixels_); }
};
```

`applySnapshot()` copia los píxeles del snapshot al `pixelBuffer_` de la capa, respetando `originX_/originY_` y el `dirtyRect_`.

---

## 6. CAPA UI V2

### 6.1 CanvasWidgetV2 (3052 líneas)

**Archivo:** `src/ui_v2/canvas_widget_v2.hpp` (225 líneas) + `.cpp` (3052 líneas)

Es el componente central del programa. Hereda de `QWidget`.

#### 6.1.1 Sistema de coordenadas

```cpp
QPointF widgetToCanvas(const QPointF& pos) const {
    // Inversa de la transformación de renderizado:
    // 1. Deshacer translate(-docW/2, -docH/2)
    // 2. Deshacer scale(sx, zoom)
    // 3. Deshacer rotate
    // 4. Deshacer translate(width/2+offsetX, height/2+offsetY)
}

QPointF canvasToWidget(const QPointF& pos) const {
    // Transformación directa (usada para posicionar cursores)
}
```

La transformación del viewport se define en `paintEvent`:
```cpp
p.translate(width()/2 + offsetX_, height()/2 + offsetY_);
p.rotate(rotationAngle_);
p.scale(flippedH_ ? -zoom_ : zoom_, zoom_);
p.translate(-docW/2, -docH/2);
```

#### 6.1.2 TextEntry - Modelo de texto

```cpp
struct TextEntry {
    QPointF pos;           // Posición en coordenadas canvas
    QString text;          // Contenido
    QFont font;            // Fuente
    QColor color;          // Color
    
    QRectF boundingRect() const {
        // Calcula rect del texto usando QFontMetrics
        return QRectF(pos.x(), pos.y() - ascent,
                      horizontalAdvance, ascent + descent);
    }
};
```

Los textos se almacenan en `std::map<int, std::vector<TextEntry>> textEntries_` (por frame).

#### 6.1.3 Sistema de herramientas

Cada herramienta se implementa como una máquina de estados dentro de los handlers de mouse:

**Brush / Eraser:**
```
PRESS:  drawing_ = true, snapFullLayer (beforeSnapshot_), 
        strokeBuffer_ creada, drawBrushStamp(firstPos)
MOVE:   drawLineStamps(lastPos, currentPos) → estampas interpoladas
RELEASE: commitStroke() → composita strokeBuffer → pixelBuffer
```

**ColorPicker:**
```
PRESS:  lee pixelAt de la capa activa, emite colorPicked
MOVE:   muestrea continuamente
```

**Fill:**
```
PRESS:  doFill(cpos) → floodFill/floodFillFabric/floodFillRamp
```

Herramientas de **flood fill**:
- `floodFill()` - 4-directional, basado en color target + tolerance
- `floodFillByAlpha()` - Basado en alpha (para áreas transparentes)
- `floodFillFabric()` - Llena con patrón de tela
- `floodFillRamp()` - Llena con degradado

**Text:**
```
PRESS:  doText(cpos) → inicia edición (editingText_ = true)
        Si click sobre texto existente: loadTextEntry(idx)
KEY:    keyPressEvent → maneja inserción, borrado, cursores, selección
FOCUS OUT: commitTextEdit() → rasteriza texto a pixelBuffer_
```

La edición de texto es completamente in-canvas con:
- Caret blink via QTimer (500ms)
- Selección con Shift+flechas
- Cursor pos (textCursorPos_) y ancla de selección (textSelectionAnchor_)
- Commit al perder foco: rasteriza con QPainter::drawText sobre un QImage, luego copia al pixelBuffer_

**Line / Rectangle / Ellipse:**
```
PRESS:  shapeStart_ = cpos, drawing_ = true
MOVE:   shapeEnd_ = cpos → paintEvent dibuja preview con DashLine
RELEASE: drawShape() → dibuja la forma final rasterizada
```

**Move:**
```
PRESS:  captureRect del área visible → moveImage_, startMove
MOVE:   updateMove → mueve la imagen
RELEASE: commitMove() → copia moveImage_ al pixelBuffer_, borra original
```

**Select:**
```
PRESS:  Inicia selección rectangular (selState_ = Creating)
MOVE:   Actualiza selRect_
RELEASE: commitSelection() → captura selImage_
        Si se hace drag dentro: MovingContent
        Si se hace drag en bordes: DraggingHandle (8 handles)
```

**Selección flotante:**
- `floatingSelection_` - Imagen que flota sobre el canvas
- Move tool sobre floating: actualiza selRect_ + marching ants
- Escape o click fuera: `commitFloatingSelection()` → compone sobre pixelBuffer_

**Hand (pan):**
```
PRESS:  panning_ = true (middle-button también activa panning)
MOVE:   offsetX_ += dx/zoom_, offsetY_ += dy/zoom_
```

#### 6.1.4 Middle-button panning

Funciona **independientemente de la herramienta activa**. Si `event->button() == MiddleButton`:
- Guarda posición inicial
- En mouseMove: calcula delta, actualiza offsetX_/offsetY_
- Funciona incluso durante un trazo de pincel

#### 6.1.5 Sistema de snapshots para undo

```cpp
QImage snapFullLayer(RasterLayer* layer) {
    // Crea QImage desde pixelData(), copia profunda (detach)
}

void pushFullLayerUndo(RasterLayer* layer, const QImage& beforeSnap) {
    auto afterSnap = snapFullLayer(layer);
    auto cmd = make_unique<PaintCommandV2>(uid, frame, index, beforeSnap, afterSnap, fullRect);
    cmd->setLayerAccessor([this]() { return activeRasterLayer(); });
    appState_->document().undoManager().pushApplied(std::move(cmd));
}
```

#### 6.1.6 Brush stamp rendering

```cpp
QRect stampRect(int cx, int cy) const {
    int r = qCeil(brushSize_ / 2.0) + 1;  // +1 padding
    return QRect(cx - r, cy - r, r * 2, r * 2);
}

void drawBrushStamp(int cx, int cy) {
    QPainter p(&strokeBuffer_);
    p.setCompositionMode(QPainter::CompositionMode_Source);  // Sobrescribe
    p.setRenderHint(QPainter::Antialiasing, true);
    
    // Brush shape branching:
    if (brushShape_ == "Square") → fillRect
    else if (brushShape_ == "Flat") → elipse escalada horizontal
    else if (brushShape_ == "Calligraphy") → elipse rotada 45° + escalada
    else → elipse circular (Round, Pencil, Highlighter, Eraser)
    
    // Construye gradiente radial para suavizar bordes:
    QRadialGradient gradient(center, radius);
    gradient.setColorAt(0.0, color);           // Centro opaco
    gradient.setColorAt(hardness, color);      // Hasta dureza
    gradient.setColorAt(1.0, transparent);     // Borde transparente
}
```

#### 6.1.7 Caret blink timer

```cpp
caretTimer_ = new QTimer(this);
caretTimer_->setInterval(500);
connect(caretTimer_, &QTimer::timeout, [this]() {
    caretVisible_ = !caretVisible_;
    update();  // Redibuja para toggle del caret
});
```

---

### 6.2 MainWindowV2

**Archivo:** `src/ui_v2/main_window_v2.hpp` (52 líneas) + `.cpp` (629 líneas)

**Estructura de layout:**
```
┌─────────────────────────────────────────────────────┐
│ Top Bar: Undo, Redo, Fit, Flip H, Rotate, Grid      │
├──────────┬────────────────────────────┬─────────────┤
│ Toolbox  │                            │ Property    │
│ Panel    │     CanvasWidgetV2         │ Editor      │
│ (izq)    │     (área central)         │ (der)       │
│          │                            │             │
├──────────┤                            ├─────────────┤
│ Color    │                            │             │
│ Panel    │                            │             │
├──────────┴────────────────────────────┴─────────────┤
│ Timeline Panel (inferior)                            │
├─────────────────────────────────────────────────────┤
│ Status Bar                                           │
└─────────────────────────────────────────────────────┘
```

**Conexiones principales:**
```cpp
// Toolbox → Canvas
connect(toolbox, &ToolboxPanelV2::toolChanged, canvas, &CanvasWidgetV2::setTool);
connect(toolbox, &ToolboxPanelV2::colorChanged, canvas, &CanvasWidgetV2::setColor);

// Property Editor → Canvas
connect(propEd, &PropertyEditorV2::brushSizeChanged, canvas, &CanvasWidgetV2::setBrushSize);
connect(propEd, &PropertyEditorV2::brushShapeChanged, canvas, &CanvasWidgetV2::setBrushShape);

// Timeline → Canvas
connect(timeline, &TimelinePanelV2::frameChanged, canvas, &CanvasWidgetV2::setCurrentFrame);

// Layer Panel → Canvas, Timeline
connect(layerPanel, &LayerPanelV2::layerChanged, canvas, &CanvasWidgetV2::setCurrentLayer);
connect(layerPanel, &LayerPanelV2::layerDisplayPropertiesChanged, canvas, [this]() {
    canvas_->invalidateBackgroundCache(); canvas_->update();
});

// Atajos de teclado
QShortcut *undoSc = new QShortcut(QKeySequence::Undo, this);
connect(undoSc, &QShortcut::activated, canvas, &CanvasWidgetV2::undo);
// ... Redo, Save, Copy, Paste, Cut, Delete, etc.
```

**File operations:**
- `newProject()` - Crea Document fresco con 1 frame, 1 capa
- `openProjectDialog()` - QFileDialog → DocumentManager::load()
- `saveProject()` / `saveProjectAs()` - DocumentManager::save()
- `exportVideo()` - Diálogo de formato → exportVideo/exportGIF/exportPNGSequence

---

### 6.3 TimelinePanelV2

**Archivo:** `src/ui_v2/timeline_panel_v2.hpp` (100 líneas) + `.cpp` (591 líneas)

**Componentes:**
- Botones de transporte: Play/Pause, Stop, Prev Frame, Next Frame
- FPS spinbox
- Add/Duplicate/Delete frame buttons
- ScrollBar horizontal para navegar frames
- Área de dibujo custom con paintEvent

**Reproducción:**
```cpp
QTimer* playbackTimer_;  // 1000/fps ms interval
// En cada tick:
currentFrame_++;
if (currentFrame_ >= totalFrames_) currentFrame_ = 0;
emit frameChanged(currentFrame_);
update();
```

**Scrubbing:**
- `mousePressEvent` en el track → `scrubbing_ = true`
- `mouseMoveEvent` → calcula frame en posición X → emite `frameChanged`
- `mouseReleaseEvent` → `scrubbing_ = false`

**Thumbnails:**
- `paintEvent` dibuja thumbnails desde `FrameThumbnailCache`
- `invalidateFrameThumbnail(frame)` para refrescar tras edición

**Menú contextual:**
- Add Frame (inserta frame vacío, shiftea datos con `shiftFrameData`)
- Duplicate Frame (copia frame actual)
- Delete Frame (elimina con `removeFrameData`)

---

### 6.4 LayerPanelV2

**Archivo:** `src/ui_v2/layer_panel_v2.hpp` (60 líneas) + `.cpp` (519 líneas)

**Componentes:**
- QListWidget con filas custom (createLayerRow)
- Botones: Add Raster, Add Vector, Duplicate, Move Up, Move Down, Delete
- QComboBox de blend modes
- Visibilidad (ícono ojo) y bloqueo (ícono candado) por capa

**Cada fila de capa contiene:**
- QLabel con ícono de visibilidad (clicable para toggle)
- QLabel con nombre de capa (doble-click para rename inline)
- LayerLockButton (SVG toggle)
- Ícono de tipo de capa

**Renombrar inline:**
```cpp
void startRename(int index, QWidget* row) {
    auto* edit = new QLineEdit(layer->name());
    // Reemplaza temporalmente el QLabel
    connect(edit, &QLineEdit::editingFinished, [=]() {
        layer->setName(edit->text());
        refreshLayerList();
        emit layerDisplayPropertiesChanged();
    });
}
```

**Señal `layerDisplayPropertiesChanged()`** → Canvas invalida `backgroundCache_`.

**Detección de cambio de blend mode:**
```cpp
void onBlendModeChanged(int mode) {
    auto* layer = activeLayer();
    layer->setBlendMode(static_cast<BlendMode>(mode));
    emit layerDisplayPropertiesChanged();
}
```

---

### 6.5 ToolboxPanelV2

**Archivo:** `src/ui_v2/toolbox_panel_v2.hpp` (61 líneas) + `.cpp` (427 líneas)

**Componentes:**
- 11 botones de herramienta (QButtonGroup, exclusivos)
- ColorSwatchButtonV2 (botón grande mostrando color actual)
- Onion skin controls: enable checkbox, prev/next spinboxes, opacity slider
- Canvas size: width/height spinboxes

**Iconos:** Cargados desde `resources/icons/toolbar/` como PNG.

**Tool buttons:** Creados con `createToolButton(icon, tooltip, id)`, estilizados con QSS.

---

### 6.6 ColorPanelV2

**Archivo:** `src/ui_v2/color_panel_v2.hpp` (41 líneas) + `.cpp` (192 líneas)

**Componentes:**
- Swatch grande (botón mostrando color)
- 4 spinboxes (R, G, B, A) con rango 0-255
- 9 círculos de paleta (`palette_circles_`)

**Paleta dinámica:** `kMaxPalette = 9` colores predefinidos que se actualizan con `updatePalette()`.

---

### 6.7 PropertyEditorV2

**Archivo:** `src/ui_v2/property_editor_v2.hpp` (126 líneas) + `.cpp` (1074 líneas)

El panel más complejo. Muestra diferentes controles según la herramienta activa:

**Brush controls (siempre visibles para Brush/Eraser):**
- Size: QSlider (1-200) + QSpinBox sincronizados
- Opacity: QSlider (1-100%) + QSpinBox
- Hardness: QSlider (1-100%) + QSpinBox
- Shape: QComboBox (Round, Square, Flat, Calligraphy, Pencil, Highlighter)
- Stabilizer: QSlider (0-50) + QSpinBox
- Pressure Size checkbox
- Pressure Opacity checkbox

**Pick Color controls (visible con ColorPicker):**
- Color swatch + valor hex
- ComboBox de tipo de variación (Monocromática, Análoga, Triádica)
- Grid de 9 botones de variación de color

**Fill controls (visible con Fill):**
- Fill Type combo (Solid, Fabric, Ramp)

**Text controls (visible con Text) - CHARACTER panel:**
- Font picker button → QFontDialog
- Font style combo (Regular, Bold, Italic, Bold Italic)
- Font size spinbox
- Text color button
- Variables: leading, tracking, alignment, anti-aliasing, underline, strikethrough

**Control adapter pattern:**
```cpp
void showBrushControls() {
    brushGroup_->show();
    pickColorGroup_->hide();
    fillGroup_->hide();
    textGroup_->hide();
    placeholderWidget_->hide();
}

void showPickColorControls() { /* inverso */ }
void showFillControls() { /* ... */ }
void showTextControls() { /* ... */ }
```

**Generación de variaciones de color:**
```cpp
QColor generateVariation(const QColor& base, int index, int total) const {
    // Monocromática: varía lightness
    // Análoga: varía hue ±30°
    // Triádica: varía hue ±120°
}
```

**Sincronización bidireccional:**
- `syncFromToolState()` → lee ToolState y actualiza controles (con guard flag)
- `onToolChanged()` → muestra/oculta grupos + sync
- Cada cambio de control → actualiza ToolState vía setters

---

### 6.8 LayerLockButton

**Archivo:** `src/ui_v2/layer_lock_button.hpp` (36 líneas) + `.cpp` (71 líneas)

Botón toggle con íconos SVG inline (locked/unlocked):
- `kUnlockedIcon` - SVG candado abierto
- `kLockedIcon` - SVG candado cerrado
- Estilizado con QSS para estados hover/locked
- Tamaño fijo: 22x22px

---

## 7. CAPA IO

### 7.1 Formatos de archivo

El programa soporta **dos formatos**:

#### Formato FAP (legacy, basado en directorio)

**Versión 2**, `loadFAP()` / `saveFAP()` en `file_format.cpp`:

```
proyecto.fap/
├── document.json          # Metadatos: versión, canvas, fps, frames
└── frames/
    ├── F0_L00.png         # Frame 0, Capa 0 (PNG ARGB32)
    ├── F0_L01.png         # Frame 0, Capa 1
    ├── F1_L00.png         # Frame 1, Capa 0
    └── ...
```

**Estructura document.json v2:**
```json
{
  "version": 2,
  "canvas_w": 1920, "canvas_h": 1080,
  "fps": 24, "total_frames": 24,
  "frames": [{
    "index": 0,
    "layers": [
      { "name": "Layer 1", "type": "raster", "visible": true,
        "opacity": 1.0, "blend_mode": "normal", "locked": false,
        "width": 1920, "height": 1080 }
    ]
  }]
}
```

#### Formato FA2D (nuevo, ZIP container)

**Versión 1**, `DocumentManager` en `document_manager.cpp`:

```
proyecto.fa2d  (archivo ZIP)
├── manifest.json           # Metadatos del documento
├── timeline.json           # Estructura de frames + capas
└── layers/
    └── frame_0/
        ├── layer_00.json   # Metadatos de capa
        ├── layer_00.png    # Píxeles PNG
        ├── layer_01.json
        └── layer_01.png
```

**Ventajas del FA2D:**
- Archivo único (conveniente para distribución)
- Compresión ZIP
- Escritura atómica (tmp → rename)
- Thread-safe (QMutex)
- Deduplicación de pixel buffers compartidos (COW)
- Metadatos y píxeles separados

**Escritura atómica:**
```cpp
save(path) {
    tmpPath = path + ".tmp";
    writeZIP(tmpPath);
    if (QFile::exists(path)) QFile::remove(path);
    QFile::rename(tmpPath, path);  // Operación atómica en mismo filesystem
}
```

### 7.2 DocumentManager

Implementa el formato FA2D con miniz (single-file C library para ZIP).

**save():**
1. `prepareAtomicTarget()` → path + ".tmp"
2. `mz_zip_writer_init_file()` → inicia ZIP
3. `writeManifest()` → documentToJson() → zip
4. `writeTimeline()` → estructura frames/capas → zip
5. `writeLayerData()` → por cada capa raster:
   - Convierte pixelBuffer → QImage → PNG buffer → zip
   - Deduplica: si el mismo pixelBuffer se usa en múltiples capas, marca `shared_pixels: true`
6. `mz_zip_writer_finalize_archive()`
7. `commitAtomic()` → rename tmp → final

**load():**
1. `mz_zip_reader_init_file()`
2. `readManifest()` → extrae manifest.json → documentFromJson()
3. `readTimeline()` → extrae timeline.json → crea capas vía layerMetadataFromJson()
4. `readLayerData()` → extrae PNGs → decodifica → copia a pixelBuffer con ensureContains()

### 7.3 Serialization Common

**Archivo:** `serialization_common.hpp/cpp`

Funciones de conversión enum↔string:
- `layerTypeToString/FromString`: "raster", "vector", "group", "audio", "camera"
- `blendModeToString/FromString`: "normal", "multiply", "screen", etc.

### 7.4 Exportación de Video

**Archivo:** `video_export.cpp`

```cpp
bool exportVideo(doc, outputPath, format, fps);
bool exportGIF(doc, outputPath, fps);
bool exportPNGSequence(doc, outputDir);
```

**Flujo de exportación:**
1. Verifica que ffmpeg esté disponible (`ffmpeg -version`)
2. Crea directorio temporal (`QTemporaryDir`)
3. Renderiza cada frame:
   - Itera capas del frame 0 (rootLayer)
   - Para cada RasterLayer: dibuja pixelBuffer con opacidad y blend mode
4. Guarda frames como PNG en temp dir
5. Ejecuta ffmpeg:
   - Video: `libx264`, `yuv420p`, padding a dimensiones pares
   - GIF: Two-pass: `palettegen` → `paletteuse` con dithering, escala 320px ancho
6. Limpia temporales

### 7.5 Document I/O (compatibilidad)

`loadDocument()` / `saveDocument()` en `document_io.cpp`:
- Detecta formato por extensión/directorio
- Si `.fa2d` → delega a DocumentManager (ZIP)
- Si `.fap` → delega a loadFAP/saveFAP (directorio)
- Caída a `.fap` si el formato solicitado no está disponible

---

## 8. CAPA PLATFORM

### 8.1 InputManager

**Archivo:** `src/platform/input_manager.hpp` (50 líneas) + `.cpp` (~90 líneas)

Adaptador entre eventos Qt y el engine:

```cpp
struct InputEvent {
    float x, y;            // Coordenadas canvas
    float pressure;        // 0.0 - 1.0
    float tiltX, tiltY;    // Wacom tilt
    float rotation;        // Wacom rotation
    bool isEraser;         // Punta de borrador
    InputEventType type;   // Press, Move, Release
};
```

**processMouseEvent():**
- Usa `canvas_transform_.inverted()` para convertir screen→canvas
- Mapea MouseButtonPress → Press (pressure=0.5), Release → Release (0.0)
- Move con botón presionado → Move (pressure=0.5)

**processTabletEvent():**
- Presión real del estilete Wacom
- Tilt X/Y (ángulo del lápiz)
- Rotación (barrel rotation)
- Detección de eraser via `QPointingDevice::PointerType::Eraser`

### 8.2 TabletHandler

**Archivo:** `src/platform/tablet_handler.hpp` (38 líneas) + `.cpp` (~50 líneas)

```cpp
struct TabletState {
    bool active;
    float pressure, tiltX, tiltY, rotation;
    bool isEraser;
};
```

Mantiene el estado actual del dispositivo Wacom. `handleEvent()` actualiza el estado desde `QTabletEvent`.

---

## 9. SISTEMA DE BUILD

### 9.1 CMakeLists.txt (264 líneas)

**Configuración:**
```cmake
cmake_minimum_required(VERSION 3.20)
project(FreeAnimation2dStyle VERSION 2.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

**Dependencias Qt (6.5+):**
```
Qt6::Core  Qt6::Gui  Qt6::Widgets  Qt6::OpenGL
Qt6::OpenGLWidgets  Qt6::Multimedia  Qt6::Svg
```

**FetchContent:**
- `miniz` - Biblioteca ZIP single-file (usada por DocumentManager)
- `googletest` - Framework de testing

**Target principal:**
```cmake
add_executable(free-animation-2d-style
    src/main.cpp
    src/core/*.cpp
    src/engine/raster/*.cpp
    src/engine/vector/*.cpp
    src/engine/brush/*.cpp
    src/engine/animation/*.cpp
    src/engine/compositor/*.cpp
    src/engine/deformation/*.cpp
    src/ui_v2/*.cpp          # ACTIVE UI
    src/io/*.cpp
    src/platform/*.cpp
)
```

**NOTA:** `src/ui/` (legacy V1) **NO** está en el build actual.

**Target de tests:**
```cmake
add_executable(fap-tests
    tests/*.cpp
    src/core/*.cpp
    src/engine/raster/*.cpp
    # ... mismos engine/io/platform que el main
    # PERO SIN src/ui_v2/ ni main.cpp
)
```

**Post-build (Windows):**
```cmake
add_custom_command(TARGET free-animation-2d-style POST_BUILD
    COMMAND windeployqt --release --no-translations $<TARGET_FILE:...>)
```
Despliega DLLs de Qt automáticamente.

**Resources:**
```cmake
qt_add_resources(free-animation-2d-style resources/resources.qrc)
```

### 9.2 Dependencias externas

| Dependencia | Uso | Tipo |
|-------------|-----|------|
| Qt 6.5+ | UI, Core, Gui, Widgets, OpenGL, Multimedia, Svg | Required |
| miniz | Compresión ZIP para FA2D | FetchContent |
| GoogleTest | Tests unitarios | FetchContent |
| FFmpeg | Exportación video/GIF | Runtime (PATH) |

---

## 10. HISTORIAL DE ERRORES Y SOLUCIONES

### 10.1 Sesión crítica: Julio 1-2, 2026 (41 commits revertidos)

**Documentado en:** `docs/session-report-2026-07-02.md` (293 líneas)

#### El problema

Durante trazos de pincel, aparecía un **"recuadro molesto"** (artefacto de línea blanca rectangular) alrededor del área donde se dibujaba. El artefacto era visible durante el trazo y a veces persistía después.

#### Cronología de debugging (2 días)

1. **Hipótesis inicial:** Problema de `setPixel` vs `blendPixel` en los dabs del pincel → Se verificó que la lógica de alpha blending era correcta
2. **Hipótesis 2:** Problema de `ensureUnique()` no llamado → Se añadió en todos los puntos de escritura
3. **Hipótesis 3:** Problema de `QImage` compartiendo datos con `pixelBuffer_` → Se hicieron copias profundas
4. **Hipótesis 4:** Problema en la conversión ARGB32 ↔ ARGB32_Premultiplied → Se estandarizó el formato
5. **Hipótesis 5:** Problema del `QPainter` en `QOpenGLWidget` → **CAUSA REAL**

#### Causa raíz confirmada

`QPainter` dibujando sobre `QOpenGLWidget` realiza composición alpha internamente de forma diferente a como lo hace sobre `QWidget` normal. Al dibujar imágenes con alpha premultiplicado, Qt aplica transformaciones que producen artefactos visuales (bordes blancos, halos, rectángulos).

#### Solución implementada

El **pipeline de 4 buffers CPU** descrito en la Sección 5. Se eliminó toda dependencia de QOpenGLWidget y se implementó renderizado puramente en CPU con QWidget estándar.

#### Branch de respaldo

`backup-fixes-20260702-083912` contiene los 41 commits revertidos para referencia histórica.

### 10.2 Otros bugs resueltos (documentados en README.md)

**Desync de señales en Layer Panel:**
- **Problema:** Cambiar blend mode en LayerPanel no refrescaba el canvas
- **Solución:** Añadir señal `layerDisplayPropertiesChanged()` y conectarla a `invalidateBackgroundCache()` + `update()`

**Middle-button panning:**
- **Problema:** Solo funcionaba con herramienta Hand activa
- **Solución:** Variable `panning_` independiente, chequeada antes del switch de herramienta en mousePressEvent

**Tríada de bugs del Text Tool:**
1. **Ghost text** - Texto rasterizado no se borraba al hacer commit → `clearTextRaster()` antes de rasterizar nuevo
2. **Cursor mal posicionado** - Cursor visual no coincidía con posición de inserción → Corrección en `boundingRect()` para incluir ascent offset
3. **Selección de texto** - No funcionaba con Shift+flechas → Añadido `textSelectionAnchor_` y lógica de selección

**Pick color visual feedback:**
- **Problema:** Al muestrear color no se veía preview
- **Solución:** `lastSamplePos_` y muestreo continuo en mouseMove con actualización del swatch

**Color variation auto-refresh:**
- **Problema:** Paleta de variaciones no se actualizaba al cambiar tipo
- **Solución:** `updateColorVariations()` llamado en `onColorVariationTypeChanged()` y `onFillTypeComboChanged()`

**Layer rename:**
- **Problema:** Doble-click para renombrar no funcionaba consistentemente
- **Solución:** `eventFilter` en el QListWidget para detectar doble-click en el QLabel del nombre

**Custom font picker y CHARACTER panel:**
- **Problema:** Sin soporte para selección de fuente en texto
- **Solución:** `QFontDialog` + panel CHARACTER en PropertyEditor con font, style, size, color, leading, tracking, alignment

**in-place text editing:**
- **Problema:** Texto se editaba en un diálogo separado
- **Solución:** Edición completamente in-canvas con caret blink, cursor positioning, y commit en focusOut

### 10.3 Problemas conocidos (no resueltos aún)

- `VectorLayer` y `VectorEngine` existen en el modelo pero no están integrados en la UI actual
- `AudioEngine`, `TweenEngine`, `DeformationEngine` están implementados en engine pero sin UI
- `NodeGraph` está implementado pero no expuesto en la UI
- El formato FA2D (ZIP) tiene código duplicado con file_format.cpp (FAP directory-based)
- `src/ui/` legacy V1 sigue en el repositorio pero no se compila

---

## 11. TESTS

### 11.1 Estructura

17 archivos de test en `tests/`, usando **GoogleTest**.

| Archivo | # Tests | Qué prueba |
|---------|---------|------------|
| `test_document.cpp` | ~8 | Creación de documento, canvas size, FPS, frames, shiftFrameData, removeFrameData |
| `test_timeline.cpp` | ~10 | Playback state, frame navigation, keyframes, looping, callbacks |
| `test_layer.cpp` | ~12 | RasterLayer create/clone/resize/clear/pixelAccess, VectorLayer add/remove, GroupLayer hierarchy |
| `test_layer_memory.cpp` | ~8 | UID uniqueness, buffer epoch, pointer stability, COW ensureUnique, crash sequence |
| `test_brush_engine.cpp` | ~10 | Default preset loading, stroke cycle begin/addPoint/end, save/load presets JSON |
| `test_bezier.cpp` | ~8 | Empty path, line segments, evaluate, tangent, flatten, curve via points |
| `test_file_format.cpp` | ~6 | Save FAP, load FAP, roundtrip (save→load→verify), canvas metadata, layer count, pixel integrity |
| `test_compositor.cpp` | ~8 | Composite with opacity, visibility toggle, layer order, blend modes, GroupLayer hierarchy, empty layers |
| `test_undo.cpp` | ~8 | Undo/redo raster, undo/redo vector, stack overflow (>128 entries), clear, canUndo/canRedo state |
| `test_deformation.cpp` | ~6 | Mesh creation, bilinear mapPoint, point manipulation, deformRaster, deformVector |
| `test_node_graph.cpp` | ~6 | Graph construction, evaluate, Input→Blend→Output chain, transform, filter blur |
| `test_audio_engine.cpp` | ~6 | Track add/remove, clip management, volume keyframes, sample rate |
| `test_ruler_guide.cpp` | ~8 | Linear snap on line, radial snap, mirror snap, perspective VP snap, RulerTool multi-guide |
| `test_tween_engine.cpp` | ~8 | Linear interpolation, EaseIn/EaseOut/EaseInOut, cubic easings, spring, breakdown poses |
| `test_pencil_retouch.cpp` | ~6 | AdjustThickness, AdjustOpacity, SmoothLines, RepaintColor, brush alpha calc |
| `test_abr_importer.cpp` | ~6 | IsAbrFile false, load invalid file, empty brushes, invalid index handling, max values |
| `test_memory_stress.cpp` | ~15 | Large canvas, many layers, many frames, COW stress, rapid undo/redo, 10000 strokes, buffer expansion, origin offset extremes, concurrent buffer access, memory limit boundaries |

**Total: 139 tests en 29 test suites, todos pasando (~663ms total).**

### 11.2 Ejecución

```bash
cmake --build build --target fap-tests
./build/Release/fap-tests.exe
```

---

## 12. PROTOTIPO PYTHON

**Directorio:** `src_py/` (9 archivos)

Implementación de referencia en **Python con PySide6** (Qt for Python). Sirvió como prototipo antes de la implementación C++.

| Archivo | Líneas | Propósito |
|---------|--------|-----------|
| `main_window.py` | 517 | Ventana principal completa con project I/O, ZIP-based .fap, export video FFmpeg |
| `canvas.py` | ~200 | Widget canvas Qt con dibujo de pincel |
| `document.py` | ~150 | Modelo de documento (capas, frames) |
| `brush_engine.py` | ~200 | Motor de pinceles con presión |
| `panels.py` | ~200 | ToolPanel, LayerPanel |
| `timeline.py` | ~200 | TimelineWidget, TimelineControls |
| `tools.py` | ~150 | Implementaciones de herramienta |
| `types_enums.py` | ~100 | Tipos y enumeraciones |
| `__init__.py` | ~5 | Package init |

El prototipo Python implementa un flujo completo: crear documento, dibujar con pincel sensible a presión, gestionar capas y frames, exportar a video con FFmpeg.

---

## 13. GLOSARIO TÉCNICO

| Término | Definición |
|---------|------------|
| **COW (Copy-on-Write)** | Patrón donde múltiples objetos comparten datos hasta que uno necesita modificarlos. Usado en `RasterLayer::pixelBuffer_` (shared_ptr) |
| **Premultiplied Alpha** | Formato donde RGB ya viene multiplicado por alpha. Usado internamente en ARGB32 (`r*a, g*a, b*a, a`) |
| **DATA domain** | Buffers que contienen la fuente de verdad de los píxeles (pixelBuffer_, strokeBuffer_) |
| **DISPLAY domain** | Buffers que contienen resultados pre-compuestos para pintado rápido (backgroundCache_) |
| **Dab** | Una estampa individual de pincel (círculo/elipse) durante un trazo |
| **ensureUnique()** | Método COW que crea copia profunda del pixelBuffer si es compartido |
| **buffer_epoch_** | Contador de modificaciones del buffer para invalidación de cachés |
| **Easing** | Curva de interpolación temporal para animaciones (Linear, EaseIn, EaseOut, Spring, etc.) |
| **Tweening** | Interpolación automática entre keyframes de animación |
| **Onion skinning** | Visualización semi-transparente de frames adyacentes durante la animación |
| **Flattening** | Conversión de curvas Bézier a segmentos de línea recta para renderizado |
| **Snapping** | Atracción magnética del cursor a guías/ejes durante el dibujo |
| **Scrubbing** | Arrastrar el cabezal del timeline para navegar frames |
| **Marching ants** | Borde animado (línea discontinua móvil) de una selección activa |
| **Floating selection** | Selección que "flota" sobre el canvas sin estar fusionada a la capa |
| **Dirty rect** | Región del buffer que necesita ser re-renderizada |
| **ensureContains()** | Método que expande dinámicamente el buffer de capa para incluir nuevas coordenadas |
| **PaintCommandV2** | Comando de undo/redo basado en snapshots before/after de píxeles |

---

## 14. APÉNDICES

### 14.1 Árbol completo de directorios fuente

```
src/
├── main.cpp
├── core/
│   ├── app_state.hpp        ├── app_state.cpp
│   ├── canvas.hpp           ├── canvas.cpp
│   ├── document.hpp         ├── document.cpp
│   ├── layer.hpp            ├── layer.cpp
│   ├── stroke.hpp           ├── stroke.cpp
│   ├── timeline.hpp         ├── timeline.cpp
│   ├── tool_state.hpp       ├── tool_state.cpp
│   ├── types.hpp            ├── types.cpp
│   ├── undo_manager.hpp     ├── undo_manager.cpp
│   └── project.cpp
├── engine/
│   ├── common/
│   │   └── blend_utils.hpp
│   ├── raster/
│   │   ├── raster_engine.hpp    ├── raster_engine.cpp
│   │   └── raster_stroke.cpp
│   ├── vector/
│   │   ├── bezier_path.hpp      ├── bezier_path.cpp
│   │   ├── vector_engine.hpp    ├── vector_engine.cpp
│   │   └── vector_stroke.cpp
│   ├── brush/
│   │   ├── abr_importer.hpp     ├── abr_importer.cpp
│   │   ├── brush_engine.hpp     ├── brush_engine.cpp
│   │   ├── brush_preset.cpp
│   │   ├── paper_texture.cpp
│   │   ├── pencil_retouch.hpp   ├── pencil_retouch.cpp
│   │   └── ruler_guide.hpp      └── ruler_guide.cpp
│   ├── animation/
│   │   ├── audio_engine.hpp     ├── audio_engine.cpp
│   │   ├── frame_thumbnail.hpp  ├── frame_thumbnail.cpp
│   │   ├── onion_skin.cpp
│   │   ├── playback.cpp
│   │   ├── timeline_engine.cpp
│   │   └── tween_engine.hpp     ├── tween_engine.cpp
│   ├── compositor/
│   │   ├── compositor.hpp       ├── compositor.cpp
│   │   └── node_graph.hpp       ├── node_graph.cpp
│   └── deformation/
│       ├── deformation_engine.hpp  ├── deformation_engine.cpp
│       └── deformation_mesh.hpp    ├── deformation_mesh.cpp
├── ui/           [LEGACY V1 - NO COMPILADO]
│   ├── main_window.hpp/cpp, canvas_widget.hpp/cpp, timeline_panel.hpp/cpp
│   ├── opengl_canvas_widget.hpp/cpp, toolbox_panel.hpp/cpp
│   ├── undo_commands.hpp/cpp, icons.hpp
│   ├── layer_panel.hpp/cpp, color_panel.hpp/cpp, property_editor.hpp/cpp
├── ui_v2/        [ACTIVO]
│   ├── canvas_widget_v2.hpp/cpp  (3052 líneas)
│   ├── main_window_v2.hpp/cpp    (629 líneas)
│   ├── timeline_panel_v2.hpp/cpp (591 líneas)
│   ├── layer_panel_v2.hpp/cpp    (519 líneas)
│   ├── property_editor_v2.hpp/cpp (1074 líneas)
│   ├── toolbox_panel_v2.hpp/cpp  (427 líneas)
│   ├── color_panel_v2.hpp/cpp    (192 líneas)
│   └── layer_lock_button.hpp/cpp (71 líneas)
├── io/
│   ├── document_manager.hpp/cpp
│   ├── serialization_common.hpp/cpp
│   ├── document_io.cpp, file_format.cpp, video_export.cpp
└── platform/
    ├── input_manager.hpp/cpp
    └── tablet_handler.hpp/cpp
```

### 14.2 Estadísticas del código

| Métrica | Valor |
|---------|-------|
| Archivos .hpp | ~28 |
| Archivos .cpp | ~32 |
| Archivos en engine/ | 25 |
| Archivos en core/ | 10 |
| Archivos en ui_v2/ | 16 |
| Archivos en io/ | 7 |
| Archivos en platform/ | 4 |
| Widgets Qt | 8 |
| Clases del modelo | 15+ |
| Enums | 10 |
| Structs de datos | 15+ |
| Tests totales | 139 |
| Suites de test | 29 |
| Tiempo total de tests | ~663ms |
| Blend modes implementados | 12 |
| Herramientas de dibujo | 17 |
| Presets de pincel | 5 |
| Tipos de capa | 5 |
| Guías de dibujo | 4 tipos |
| Modos de easing | 8 |
| Filtros de nodo | 2 (Blur, Invert) |
| Formatos de archivo | 2 (.fap, .fa2d) |
| Formatos de exportación | 3 (MP4, GIF, PNG sequence) |
| Formatos de textura | 2 (PGM, BMP) |
| Versiones de ABR soportadas | 3 (v1, v2, v6) |

### 14.3 Mapa de blend modes (fap ↔ QPainter)

| fap::BlendMode | QPainter::CompositionMode |
|----------------|--------------------------|
| Normal | SourceOver |
| Multiply | Multiply |
| Screen | Screen |
| Overlay | Overlay |
| Add | Plus |
| Subtract | Exclusion |
| Darken | Darken |
| Lighten | Lighten |
| ColorBurn | ColorBurn |
| ColorDodge | ColorDodge |
| SoftLight | SoftLight |
| HardLight | HardLight |

### 14.4 Mapa de herramientas (index ↔ ToolType)

| Index | ToolType | Hotkey | Icono |
|-------|----------|--------|-------|
| 0 | Brush | B | Pincel |
| 1 | Eraser | E | Borrador |
| 2 | ColorPicker | I (eyedropper) | Cuentagotas |
| 3 | Fill | G (bucket) | Cubo de pintura |
| 4 | Text | T | Texto |
| 5 | Line | L | Línea |
| 6 | Rectangle | R | Rectángulo |
| 7 | Ellipse | O | Elipse |
| 8 | Move | V | Mover |
| 9 | Select | S | Selección |
| 10 | Hand | H | Mano |

### 14.5 Valores por defecto de ToolState

| Propiedad | Valor default |
|-----------|---------------|
| activeTool | Eraser |
| primaryColor | #000000 (negro) |
| brushSize | 20 |
| brushOpacity | 100% |
| brushHardness | 80% |
| brushShape | "Round" |
| pressureSize | false |
| pressureOpacity | false |
| stabilizerLevel | 0 |
| canvasWidth | 1920 |
| canvasHeight | 1080 |
| onionEnabled | true |
| onionPrevFrames | 3 |
| onionNextFrames | 1 |
| onionOpacity | 35% |
| fillType | Solid (0) |
| colorVariationType | Monochromatic (0) |
| colorVariationCount | 9 |
| textAntiAliasing | true |

### 14.6 Ciclo de vida de un trazo de pincel

```
1. mousePressEvent(QMouseEvent*)
   ├── Verifica herramienta activa (Brush/Eraser)
   ├── botón izquierdo → drawing_ = true
   ├── Crea strokeBuffer_ (QImage ARGB32_PM, tamaño canvas)
   ├── beforeSnapshot_ = snapFullLayer(activeLayer)
   ├── drawBrushStamp(firstPos) → strokeBuffer_
   └── update()

2. mouseMoveEvent(QMouseEvent*)
   ├── while drawing_:
   │   ├── Calcula virtualCursorPos_ (widgetToCanvas)
   │   ├── drawLineStamps(prevPos, currentPos) → strokeBuffer_
   │   │   └── Para cada punto en la línea interpolada:
   │   │       drawBrushStamp(cx, cy) → QPainter sobre strokeBuffer_
   │   └── update()

3. mouseReleaseEvent(QMouseEvent*)
   ├── commitStroke()
   │   ├── QPainter p sobre pixelBuffer_ QImage
   │   ├── p.drawImage(0, 0, strokeBuffer_) con SourceOver
   │   ├── afterSnapshot_ = snapFullLayer(activeLayer)
   │   ├── Crea PaintCommandV2
   │   └── undoManager.pushApplied(cmd)
   ├── drawing_ = false
   ├── strokeBuffer_ = QImage() (limpia)
   ├── strokeDirtyRect_ = QRect()
   ├── invalidateBackgroundCache()
   └── update()
```

### 14.7 Notas finales

- **El programa no usa OpenGL para renderizado** (se eliminó QOpenGLWidget en favor de QWidget + CPU rendering)
- **Todo el renderizado es CPU** a través de QPainter sobre QImage (ARGB32_Premultiplied)
- **El engine es totalmente independiente de Qt** y podría reutilizarse con otras UI toolkits
- **La separación DATA/DISPLAY** es la lección arquitectónica más importante aprendida durante el desarrollo
- **Los 41 commits revertidos** representan ~16 horas de trabajo que resultaron ser innecesarios una vez identificada la causa raíz
- **139 tests** garantizan que el pipeline de renderizado, el modelo de datos, y los motores funcionan correctamente

---

*Documento generado el 2 de Julio de 2026 a partir del análisis línea por línea del código fuente completo.*
