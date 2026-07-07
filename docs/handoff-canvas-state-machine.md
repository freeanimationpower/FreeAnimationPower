# Documento de Traspaso de Proyecto -- Maquina de Estados del Canvas

## Proyecto: Free Animation Power (FreeAnimation2dStyle)
## Alcance: Interaccion Seleccion Flotante, Renderizado, Herramientas, Pila Undo/Redo y Motor de Capas
## Fecha: 2026-06-19 (actualizado Jul 2026)
## Destinatario: IA de continuidad (Gemini) y desarrolladores del equipo

> **Jul 2026:** La arquitectura actual usa V2 UI (`src/ui_v2/`) como interfaz principal. `CanvasWidgetV2` implementa el canvas central con `drawBrushStamp`, `drawLineStamps`, `commitStroke` y `PaintCommandV2` para undo/redo. El viejo `CanvasWidget` (`src/ui/`) es referencia heredada. Para trabajo actual, enfocarse en `canvas_widget_v2.cpp`.

---

## 0. Indice General

1. [Resumen de la Arquitectura del Canvas](#1-resumen-de-la-arquitectura-del-canvas)
2. [Historial de Bugs Resueltos](#2-historial-de-bugs-resueltos-causa-raiz-y-solucion)
3. [Bloques de Codigo Critico Actualizado](#3-bloques-de-codigo-critico-actualizado-c20--qt-65)
4. [Recomendaciones Arquitectonicas](#4-recomendaciones-arquitectonicas-next-steps)
5. [Guia Completa del Motor de Renderizado](#5-guia-completa-del-motor-de-renderizado)
6. [Sistema de Coordenadas y Transformaciones](#6-sistema-de-coordenadas-y-transformaciones)
7. [Guia Exhaustiva de Cada Herramienta](#7-guia-exhaustiva-de-cada-herramienta)
8. [Sistema de Doble Pila Undo/Redo](#8-sistema-de-doble-pila-undoredo)
9. [Modelo de Capas en Profundidad](#9-modelo-de-capas-en-profundidad)
10. [Sistema de Trazos Vectoriales (RawStroke)](#10-sistema-de-trazos-vectoriales-rawstroke)
11. [Pipeline de Entrada (Tablet, Mouse, Teclado)](#11-pipeline-de-entrada-tablet-mouse-teclado)
12. [Motor de Pincel (BrushEngine)](#12-motor-de-pincel-brushengine)
13. [Sistema de Modos de Fusion (Blend Modes)](#13-sistema-de-modos-de-fusion-blend-modes)
14. [Sistema de Epochs y Cacheo de Buffers](#14-sistema-de-epochs-y-cacheo-de-buffers)
15. [Operaciones de Portapapeles y Atajos de Teclado](#15-operaciones-de-portapapeles-y-atajos-de-teclado)
16. [Conexion entre Paneles UI y Canvas](#16-conexion-entre-paneles-ui-y-canvas)
17. [Apendices](#apendices)

---

## 1. Resumen de la Arquitectura del Canvas

### 1.1 Archivo de Cabecera y Miembros

La clase `CanvasWidget` esta declarada en `src/ui/canvas_widget.hpp:38` e implementada en `src/ui/canvas_widget.cpp` (1838 lineas). Hereda de `QWidget` y actua como el componente central de renderizado e interaccion de toda la aplicacion.

**Todos los miembros de datos privados declarados en el header** (`canvas_widget.hpp:110-218`):

```
DOCUMENTO Y DEPENDENCIAS EXTERNAS
  Document* doc_          = nullptr;   // Modelo de datos (capas, frames, tamano)
  BrushEngine* brush_     = nullptr;   // Motor de pincel (presets, stroke processing)
  LayerPanel* layerPanel_ = nullptr;   // Panel UI de capas (para consultar capa activa)
  QFont font_;                          // Fuente para herramienta Texto

GEOMETRIA DE VISTA (VIEWPORT)
  double zoom_        = 1.0;           // Factor de zoom [0.02, 64.0]
  double offX_ = 0, offY_ = 0;         // Offset de pan en pixeles de pantalla
  int rotation_       = 0;             // Rotacion en grados [0, 360), paso 15
  bool flipH_         = false;         // Espejo horizontal
  bool showGrid_      = false;         // Mostrar reticula
  int gridSize_       = 64;            // Tamano de celda de reticula en px canvas

COLOR Y HERRAMIENTA ACTIVA
  QColor bgColor_{30, 30, 30};         // Fondo del area fuera del canvas
  int tool_           = 0;             // Indice de herramienta activa (0-11)
  QColor color_{0, 0, 0, 255};        // Color activo (RGBA)

LINEA DE TIEMPO Y CAPAS
  int currentFrame_   = 0;             // Frame actual (0-indexado)
  int totalFrames_    = 1;             // Total de frames en el documento
  int currentLayerIdx_ = 0;            // Indice de capa activa en el modelo
  LayerUid currentLayerUid_ = 0;       // UID de capa activa (alternativa a idx)

ONION SKIN
  int onionPrev_ = 3, onionNext_ = 1;  // Frames anteriores/siguientes visibles
  float onionOpacity_ = 0.35f;         // Opacidad del onion skin
  bool onionEnabled_ = true;           // Onion skin activado

ESTADO DE DIBUJO Y PAN
  bool playing_       = false;         // Reproduccion activa
  QPointF cursorPos_;                  // Ultima posicion del cursor en screen coords
  bool drawing_       = false;         // Gesto de dibujo en curso
  bool panning_       = false;         // Gesto de pan en curso

ESTADO DE SELECCION
  enum class SelectionState : uint8_t { None, Creating, MovingContent, DraggingHandle };
  SelectionState selState_ = SelectionState::None;
  int selHandleIdx_   = -1;            // Handle activo (-1 = ninguno, 0-7)
  QPointF selDragStart_;               // Origen del arrastre en canvas coords

SELECCION FLOTANTE  [SECCION CRITICA]
  bool floatingActive_ = false;        // TRUE si hay seleccion flotante activa
  QImage floatingSelection_;           // Copia de trabajo modificable
  QImage originalFloatingSelection_;   // Copia inmutable (fuente de verdad)
  QElapsedTimer marchingTimer_;        // Animacion de marching ants
  bool moving_ = false;                // TRUE durante arrastre con Move tool

TRANSFORM
  bool transformActive_ = false;
  int activeHandle_    = -1;           // Handle de transform activo (0-7 corner/edge, 8=centro)
  qreal scaleX_ = 1.0, scaleY_ = 1.0;
  QPointF translate_;                  // Traslacion acumulada en canvas coords
  QRectF transformBounds_;             // Bounding box de la region a transformar
  QPointF transformDragOrigin_;
  qreal dragScaleRefX_ = 1.0, dragScaleRefY_ = 1.0;
  QPointF dragTranslateRef_;
  QPointF handleFixedPoint_;           // Punto fijo durante escala por handle
  bool handleScaleX_ = false, handleScaleY_ = false;

GEOMETRIA DE SELECCION
  QPointF selStart_, moveStart_;       // Origen del gesto de seleccion/move
  QRectF selRect_;                     // Rectangulo de seleccion (canvas coords)
  QImage selImage_;                    // Snapshot composite en selRect_

DIBUJO DE FORMAS
  QPointF shapeStart_{-1,-1}, shapeEnd_{-1,-1};  // Inicio/fin de forma geometrica

TABLET
  bool tabletActive_   = false;
  bool tabletEraser_   = false;        // TRUE si el stylus esta en modo borrador
  float tabletPressure_ = 1.0f;

DATOS DE TRAZO EN CURSO
  std::vector<StrokePoint> strokePoints_;      // Puntos acumulados del trazo actual
  std::vector<QPointF> stabilizerBuffer_;      // Buffer para suavizado
  QImage beforeImage_;                         // NO USADO -- reservado

ESTADO DE MOVE TOOL
  Layer* moveLayer_   = nullptr;
  QImage moveImage_;
  QPointF moveOffset_;
  QRectF moveSrcRect_;
  bool moveModeSelection_ = false;      // TRUE si se mueve una seleccion, FALSE si capa entera

PILA DE UNDO (Qt)
  QUndoStack undoStack_;                // Pila de comandos undo/redo de Qt

TRAZOS VECTORIALES POR FRAME
  std::map<int, std::vector<RawStroke>> vectorStrokes_;
  std::map<LayerUid, uint64_t> rasterEpochs_;  // Cache de version de buffer raster
```

### 1.2 Modelo de Renderizado en Profundidad

El canvas NO utiliza un buffer de composicion intermedio global. Cada invocacion de `paintEvent(QPaintEvent*)` (`canvas_widget.cpp:153-346`) reconstruye la escena completa desde cero. El flujo detallado es:

```
PASO 0: PREPARACION DEL QPainter
  - fillRect(rect(), QColor(100,100,100))          -> fondo gris del widget
  - Calcular screenRect = QRectF(ox, oy, sw, sh)   -> area del canvas en pantalla
  - fillRect(screenRect, Qt::white)                 -> fondo blanco del canvas

PASO 1: TRANSFORMACIONES DE VISTA
  - p.translate(ox, oy)                             -> origen del canvas
  - p.scale(zoom_, zoom_)                           -> zoom
  - Si flipH_: translate(cw,0) + scale(-1,1)        -> espejo horizontal
  - Si rotation_ != 0: translate al centro + rotate + translate inverso

PASO 2: CAPA DE FONDO (GRID)
  - Si showGrid_: paintGrid() dibuja reticula con QPen verde semi-transparente

PASO 3: ONION SKIN
  - Para cada frame anterior (onionPrev_): renderTintedFrame() con tinte rojo
  - Para cada frame siguiente (onionNext_): renderTintedFrame() con tinte azul
  - Cada frame se compone primero (fusion de todas sus capas raster) y luego se
    tinie usando CompositionMode_DestinationIn sobre un QImage del color tinte

PASO 4: CAPAS DEL FRAME ACTUAL (EN ORDEN Z)
  - Itera layers de currentRootLayer().layers() en orden (primero = fondo)
  - Para cada capa visible:
      p.setOpacity(layer->opacity())
      p.setCompositionMode(toQtCompositionMode(layer->blendMode()))
      Si RasterLayer: wrapRasterLayer() produce QImage desde pixels raw, p.drawImage()
      Si VectorLayer: renderVectorStrokes() dibuja RawStrokes del frame para ese LayerUid

PASO 5: TRAZO EN VIVO (LIVE STROKE)
  - Si drawing_ y strokePoints_ no vacio:
      Si tool_==1 (Eraser): dibuja circulos blancos semi-transparentes como preview
      Si tool_!=1 (Brush): dibuja QPainterPath con el color activo y grosor del pincel

PASO 6: PREVIEW DE FORMA GEOMETRICA
  - Si shapeStart_ y shapeEnd_ validos y tool_ en [5,6,7]:
      Rectangulo/Elipse/Linea con QPen azul dashed

PASO 7: RECTANGULO DE SELECCION EN CREACION
  - Si selState_==Creating y selRect_ valido: rectangulo azul semi-transparente

PASO 8: SELECCION FLOTANTE (MARCHING ANTS)
  - Si tool_==9, floatingActive_, originalFloatingSelection_ no nulo, selRect_ valido:
      Dibujar originalFloatingSelection_ escalado al tamano actual de selRect_
      Dibujar borde con "marching ants" (dos QPen dashados, blanco y negro,
      offset animado por marchingTimer_)
      Dibujar 8 handles cuadrados (4 esquinas + 4 puntos medios de aristas)

PASO 9: SELECCION NO FLOTANTE (DASHED ESTATICO)
  - Si tool_==9, (floatingActive_ O (selRect_ valido y selImage_ no nulo)):
      Borde azul dashed estatico (sin handles ni marching ants)

PASO 10: MOVE TOOL PREVIEW
  - Si moving_ y moveImage_ no nulo: dibujar moveImage_ con opacidad 0.7

PASO 11: TRANSFORM PREVIEW
  - Si tool_==11 y transformActive_:
      Dibujar la imagen fuente con QTransform (escala + traslacion) con opacidad 0.5
      Dibujar 8 handles cuadrados + bounding box

PASO 12: SOMBRA Y BORDE DEL CANVAS
  - drop shadow negro + borde gris alrededor del screenRect

PASO 13: CURSOR DE PINCEL
  - Si no drawing_ ni panning_, tool_ es Brush(0) o Eraser(1):
      Circulo blanco semi-transparente del diametro del pincel

PASO 14: OVERLAY DE INFORMACION
  - paintInfoOverlay(): coordenadas, zoom%, frame actual/total, rotacion, flip

PASO 15: RE-DISPARO PARA ANIMACION
  - Si tool_==9 y floatingActive_: llamar update() para siguiente frame de marching ants
    (esto crea un bucle de repintado continuo mientras hay seleccion flotante)
```

**Funciones auxiliares de renderizado usadas en paintEvent:**

- `wrapRasterLayer(RasterLayer*)` (`linea 472`): Crea un `QImage` que referencia directamente la memoria raw de `RasterLayer::pixelData()` usando el constructor `QImage(uchar*, w, h, bpl, Format_ARGB32_Premultiplied)`. Utiliza `rasterEpochs_` como cache para evitar recrear el wrapper si el buffer no cambio.
- `renderVectorStrokes(QPainter&, int frame, LayerUid filterUid)` (`linea 1623`): Itera `vectorStrokes_[frame]` y renderiza cada `RawStroke` cuyo `layerUid` coincida con el filtro.
- `renderVectorStroke(QPainter&, const RawStroke&)` (`linea 1638`): Si `eraser==true` usa `CompositionMode_DestinationOut` con circulos negros. Si la opacidad es <0.99, renderiza a una capa offscreen intermedia para aplicar opacidad por grupo (evitando artefactos de superposicion). Si no, dibuja directamente segmentos de linea con grosor variable por presion.
- `paintOnionSkin(QPainter&)` (`linea 375`): Itera frames anteriores (rojo) y siguientes (azul). Para cada uno llama a `renderTintedFrame()`.
- `renderTintedFrame(QPainter&, int frameIdx, QColor tint)` (`linea 395`): Compone todas las capas raster de un frame en un `QImage` combinado, luego aplica el tinte con `CompositionMode_DestinationIn`.
- `paintGrid(QPainter&, const QRectF&)` (`linea 366`): Dibuja lineas verdes verticales y horizontales espaciadas por `gridSize_`.
- `paintLiveStroke(QPainter&)` (`linea 421`): Dibuja el trazo en curso (preview) con el color y grosor actuales.
- `paintInfoOverlay(QPainter&)` (`linea 455`): Barra inferior con coordenadas del cursor, zoom%, frame, rotacion, flip.

### 1.3 Sistema de Capas en el Renderizado

El orden Z de las capas esta determinado por el orden en `GroupLayer::layers()` (un `std::deque<std::unique_ptr<Layer>>`). La primera capa en el deque es la del fondo (se dibuja primero) y la ultima es la del frente (se dibuja encima de todas). Cada capa se dibuja con su propio `opacity` y `blendMode`, configurados en el `QPainter` antes de dibujar esa capa especifica. Tras dibujar todas las capas, ambos se resetean a `SourceOver` y `1.0`.

**IMPORTANTE:** El orden es secuencial plano. No hay grupos anidados en el renderizado actual. `GroupLayer` es una coleccion plana de `Layer*`; su propiedad `opacity` y `blendMode` no se evaluan durante el renderizado (solo se evaluan las de cada capa hoja).

### 1.4 Variables Criticas de Estado de Seleccion

```cpp
// Declaradas en canvas_widget.hpp:171-175, 189-203
bool floatingActive_ = false;          // TRUE si hay una seleccion flotante activa
QImage floatingSelection_;            // Copia de trabajo (puede ser escalada/modificada)
QImage originalFloatingSelection_;    // Copia original inalterada (fuente de verdad para stamp)
QElapsedTimer marchingTimer_;         // Animacion de marching ants

QRectF selRect_;                      // Rectangulo de seleccion en coordenadas de canvas
QImage selImage_;                     // Snapshot del composite en el area de seleccion (para move tool)
bool moving_ = false;                 // TRUE durante arrastre de move tool
```

**Diferencia semantica entre las tres QImage de seleccion:**

| Variable | Quien la crea | Cuando se modifica | Proposito |
|----------|--------------|-------------------|-----------|
| `floatingSelection_` | `commitSelection()` via `layerPixels.copy(bufRect)` | Durante redimensionado de handles (NO implementado aun) | Buffer de trabajo para operaciones intermedias |
| `originalFloatingSelection_` | `commitSelection()`: copia de `floatingSelection_` al crearse | **Jamas** tras el cut inicial | Fuente de verdad inmutable; se usa en `paintEvent` linea 225 y en `commitFloatingSelection()` linea 1421 |
| `selImage_` | `commitSelection()` via `compositeFrame().copy(selRect)` | Nunca tras crearse | Snapshot del composite completo en el area; usado por Move tool (`startMove` linea 1269) para mover pixeles visuales de todas las capas |

**Maquina de estados de seleccion:**

```cpp
enum class SelectionState : uint8_t { None, Creating, MovingContent, DraggingHandle };
SelectionState selState_ = SelectionState::None;
```

**Transiciones de estado de seleccion:**

```
None
  |-- inputPress sobre area vacia (tool=9) --> Creating
  |-- inputPress sobre handle (tool=9, floatingActive_) --> DraggingHandle (selHandleIdx_ = 0..7)
  |-- inputPress dentro de selRect_ (tool=9, floatingActive_) --> MovingContent

Creating
  |-- inputMove --> actualiza selRect_
  |-- inputRelease (selRect_ < 3x3 px) --> None (descarta)
  |-- inputRelease (selRect_ >= 3x3 px) --> llama commitSelection(), luego None

MovingContent
  |-- inputMove --> traslada selRect_ por delta
  |-- inputRelease --> None

DraggingHandle
  |-- inputMove --> redimensiona selRect_ segun handle (esquinas mueven 2 ejes, bordes mueven 1)
  |-- inputRelease --> None
```

---

## 2. Historial de Bugs Resueltos (Causa Raiz y Solucion)

### 2.1 Bug #1: Borrado Masivo por Fuga de CompositionMode (CompositionMode Leak)

**Sintoma:** Al usar la herramienta Goma (Eraser, tool=1) con una seleccion flotante activa, toda la capa se borraba en lugar de solo los trazos de la goma.

**Causa Raiz:** La funcion `commitFloatingSelection()` utilizaba originalmente `QPainter::CompositionMode_Clear` para estampar la seleccion flotante de vuelta a la capa. Qt 6 propaga el ultimo `CompositionMode` seteado en un `QPainter` activo a traves de multiples llamadas a `begin()`/`end()` sobre el mismo dispositivo de pintado. El flujo era:

1. `commitFloatingSelection()` seteaba `CompositionMode_Clear` en el `QPainter` de la imagen de capa.
2. `doErase()` abria un nuevo `QPainter` sobre la misma imagen de capa sin re-setear explicitamente el modo de composicion (asumiendo que `QPainter::begin()` reseteaba a `SourceOver`).
3. La goma, que debia usar `DestinationOut` para borrar selectivamente, heredaba el `CompositionMode_Clear` del paso anterior. `Clear` en Qt significa "poner todos los canales (incluyendo alpha) a cero para cada pixel del viewport completo", lo que resultaba en un lienzo completamente transparente.

**Solucion Aplicada:** En `commitFloatingSelection()` (`canvas_widget.cpp:1418`), se cambio `CompositionMode_Clear` por `CompositionMode_SourceOver`. La operacion "SourceOver" estampa la seleccion flotante encima del contenido existente respetando el canal alpha, que es el comportamiento correcto para finalizar una seleccion flotante. El "cut" inicial (borrado del area seleccionada) se realiza exclusivamente en `commitSelection()` (linea 1363), que SI debe usar `CompositionMode_Clear`.

**Blindaje adicional:** `doErase()` (linea 1039) y `renderVectorStroke()` (linea 1642) ahora setean explicitamente `QPainter::CompositionMode_DestinationOut` cada vez que abren un `QPainter`, sin asumir estado heredado.

### 2.2 Bug #2: Perdida de Datos en Transicion de Herramienta (Transform -> Eraser)

**Sintoma:** Al cambiar de la herramienta Transform (11) a Goma (1) mientras habia una seleccion flotante activa, el buffer de la seleccion flotante se destruia sin ser comprometido a la capa, perdiendo irreversiblemente los pixeles seleccionados.

**Causa Raiz:** La version original de `setTool(int tool)` ejecutaba una limpieza incondicional del estado flotante (`floatingSelection_ = QImage()`, etc.) para TODOS los cambios de herramienta, sin validar si habia datos pendientes de commit.

**Solucion Aplicada:** `setTool()` (`canvas_widget.cpp:105-138`) ahora implementa una logica de guardia en dos fases:

1. **Fase de commit condicional (linea 106):** Si `floatingActive_` es true, `originalFloatingSelection_` no es nulo, y `selRect_` es valido, Y ademas la herramienta destino NO es Seleccion (9) ni Move (8), entonces se llama a `commitFloatingSelection()` para estampar la seleccion antes de cambiar de herramienta. Esto preserva los datos.
2. **Fase de limpieza dura (linea 117-123):** Tras el commit (o si el commit no fue necesario), se ejecuta una limpieza completa del estado flotante si la nueva herramienta no es 9 ni 8. Esto garantiza que no queden artefactos de seleccion flotante al entrar a herramientas de dibujo.

Las herramientas 8 (Move) y 9 (Select) son las unicas que pueden operar sobre una seleccion flotante sin necesidad de commit previo, por eso se excluyen de la guardia.

### 2.3 Bug #3: Duplicacion (Ghosting) y Fondo Gris en Seleccion Flotante

**Sintoma:** Al crear una seleccion flotante, aparecia un rectangulo gris opaco detras de la seleccion y la imagen se duplicaba (el contenido original permanecia visible debajo de la copia flotante).

**Causa Raiz (dos problemas simultaneos):**

**3a. Fondo Gris:** El `QImage` usado para `floatingSelection_` y `originalFloatingSelection_` se creaba con `QImage::Format_ARGB32_Premultiplied` pero sin llamar a `fill(Qt::transparent)`. En Qt 6, un `QImage` recien construido con `Format_ARGB32_Premultiplied` contiene memoria no inicializada (no se zero-fill automaticamente). Al copiar unicamente la region del `selRect_` sobre este buffer (via `QImage::copy()`), los pixeles fuera de la region de copia contenian basura de memoria, que el pintor interpretaba como pixeles grises semi-transparentes u opacos.

**3b. Ghosting/Duplicacion:** `commitSelection()` (`canvas_widget.cpp:1325`) originalmente NO ejecutaba el "Cut" (borrado de los pixeles originales de la capa en la region seleccionada). Simplemente copiaba los pixeles a `floatingSelection_` y los dejaba intactos en la capa, causando que al dibujar la seleccion flotante encima (via `paintEvent` linea 225) se viera duplicada la imagen.

**Solucion Aplicada:**

Para **3a**, se anadio `fill(Qt::transparent)` en todas las construcciones de `QImage` temporales con formato alpha (ej. `readLayerPixels` linea 494, `dst` en `commitTransform` linea 1458, `readRasterRect` linea 55 de `undo_commands.cpp`, etc.).

Para **3b**, `commitSelection()` ahora ejecuta el corte real (linea 1361-1367), borrando los pixeles originales del area seleccionada usando `CompositionMode_Clear` (que pone RGBA=(0,0,0,0) en cada pixel del rectangulo).

### 2.4 Bug #4: Crash por Offset Fantasma y Segfault en Ctrl+Z

**Sintoma:** Al iniciar la aplicacion, el canvas aparecia desplazado a la coordenada `(0, height_del_documento)` sin interaccion del usuario. Ademas, al presionar Ctrl+Z con una seleccion flotante en un estado intermedio, la aplicacion crasheaba con Access Violation (0xC0000005).

**Causa Raiz (dos problemas independientes):**

**4a. Offset Fantasma:** Las variables miembro `offX_` y `offY_` (desplazamiento de pan) se declaraban en el header (`canvas_widget.hpp:146`) pero NO se inicializaban en el constructor. En el codigo original, el constructor (`canvas_widget.cpp:47`) solo inicializaba `doc_`, `brush_` y algunas variables de UI, pero omitia `offX_` y `offY_`. Esto dejaba estas variables con valores indeterminados de stack/heap, que frecuentemente resultaban en `offY_ = doc_->height()` (el valor de una variable adyacente en memoria o basura aleatoria), causando un desplazamiento vertical masivo del viewport.

**4b. Segfault en Undo:** La funcion `pushFullLayerUndo()` (`canvas_widget.cpp:592`) originalmente no validaba si `beforeSnap` era un `QImage` nulo antes de pasarlo al constructor de `PaintCommand`. En flujos como `commitFloatingSelection()` cuando la capa activa era nula o el `readLayerPixels` fallaba, `snapCurrentLayer()` retornaba un `QImage` nulo (construido por defecto). Este `QImage` nulo se almacenaba en `PaintCommand::beforePixels_`. Al ejecutar `PaintCommand::undo()` (`undo_commands.cpp:91`), la funcion intentaba acceder a `beforePixels_.bits()` indirectamente a traves de `writeRasterRect`, que iteraba sobre las scanlines. Un `QImage` nulo tiene `bits() == nullptr` y `scanLine()` retorna punteros invalidos, causando un Access Violation al desreferenciar.

**Solucion Aplicada:**

Para **4a**, el constructor ahora inicializa explicitamente todas las variables de estado geometrico en la lista de inicializacion (`canvas_widget.cpp:51-57`). Ademas, `offX_` y `offY_` ya se inicializan en el header (`canvas_widget.hpp:146`: `double offX_ = 0, offY_ = 0;`) mediante inicializacion in-class (C++11), lo cual es suficiente.

Para **4b**, `pushFullLayerUndo()` ahora tiene un guard clause temprano (`canvas_widget.cpp:594`): `if (!layer || beforeSnap.isNull()) return;`. Ademas, `commitFloatingSelection()` valida el estado antes de llamar a `pushFullLayerUndo()` y verifica que `readLayerPixels()` no retorne nulo antes de proceder al stamp.

---

## 3. Bloques de Codigo Critico Actualizado (C++20 / Qt 6.5+)

### 3.1 Constructor con Inicializacion Defensiva Completa

```cpp
// canvas_widget.cpp:47-69
CanvasWidget::CanvasWidget(Document* doc, BrushEngine* brush, QWidget* parent)
    : QWidget(parent)
    , doc_(doc)
    , brush_(brush)
    // --- Blindaje: toda variable de estado geometrico/de seleccion se
    //     inicializa explicitamente para evitar valores indeterminados
    //     que causen offsets fantasmas o imagenes corruptas. ---
    , floatingActive_(false)
    , floatingSelection_()           // QImage nulo por defecto
    , originalFloatingSelection_()   // QImage nulo por defecto
    , selRect_()                     // QRectF nulo (isNull() == true)
    , selImage_()                    // QImage nulo por defecto
    , selState_(SelectionState::None)
    , selHandleIdx_(-1)
{
    setMinimumSize(400, 300);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TabletTracking, true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(false);
    setStyleSheet("background-color: #1e1e1e;");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::CrossCursor);
    font_ = QFont("Arial", 24);
}
```

**Notas sobre la inicializacion:**
- Las variables declaradas en el header con inicializadores in-class (`double offX_ = 0`, `bool drawing_ = false`, etc.) NO necesitan repetirse en la lista de inicializacion del constructor. C++11 garantiza que el inicializador in-class se ejecuta si el miembro no aparece en la lista del constructor.
- Se eligio inicializar explicitamente en el constructor las variables del subsistema de seleccion (`floatingActive_`, `floatingSelection_`, `originalFloatingSelection_`, `selRect_`, `selImage_`, `selState_`, `selHandleIdx_`) porque son las mas propensas a causar crashes si contienen basura. La inicializacion in-class en el header seria igualmente valida, pero la inclusion en el constructor las hace visibles para auditoria.
- `Qt::WA_TabletTracking` es necesario para recibir eventos `QTabletEvent` incluso cuando el lapiz no toca la superficie.
- `Qt::WA_OpaquePaintEvent = false` indica que el widget tiene regiones transparentes (el area fuera del canvas).

### 3.2 commitFloatingSelection() -- Estampado Final con SourceOver

```cpp
// canvas_widget.cpp:1377-1431
void CanvasWidget::commitFloatingSelection()
{
    // Lambda de limpieza total del estado flotante.
    // Centraliza el reset para evitar estados zombie (mitad limpios, mitad no).
    auto clearFloatingState = [&]() {
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        selImage_ = QImage();
        selRect_ = QRectF();
        floatingActive_ = false;
    };

    // Guardia 1: estado invalido -> limpiar y salir sin tocar la capa
    if (!floatingActive_ || originalFloatingSelection_.isNull() || selRect_.isEmpty()) {
        clearFloatingState();
        update();
        return;
    }

    Layer* layer = resolveCurrentLayer();
    // Guardia 2: capa nula o bloqueada -> no modificar, solo limpiar UI
    if (!layer || layer->locked()) {
        qWarning() << "[commitFloatingSelection] Layer null or locked, aborting";
        clearFloatingState();
        update();
        return;
    }

    qreal ox = 0, oy = 0;
    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        ox = r->originX(); oy = r->originY();
    }

    auto beforeSnap = snapCurrentLayer();
    QImage img = readLayerPixels(layer);
    // Guardia 3: imagen de capa no legible -> abortar sin modificar
    if (img.isNull()) {
        qWarning() << "[commitFloatingSelection] readLayerPixels returned null, aborting stamp";
        clearFloatingState();
        update();
        return;
    }

    {
        QPainter layerPainter(&img);
        // CRITICO: SourceOver estampa respetando alpha, no borra el fondo.
        // NO usar Clear aqui -- causaria el Bug #1 (Borrado Masivo).
        layerPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        layerPainter.setRenderHint(QPainter::SmoothPixmapTransform);
        layerPainter.translate(-ox, -oy);
        layerPainter.drawImage(selRect_, originalFloatingSelection_);
        layerPainter.end();
    }

    writeLayerPixels(layer, img);
    // pushFullLayerUndo tiene blindaje interno contra beforeSnap.isNull()
    pushFullLayerUndo(layer, beforeSnap, QStringLiteral("Commit Selection"));

    // Limpieza completa post-commit: sin estado residual
    clearFloatingState();
    update();
    emit canvasUpdated();
}
```

**Analisis de las tres guardias:**
1. **Guardia 1:** Estado inconsistente. Si el booleano `floatingActive_` esta a true pero las imagenes son nulas o el rectangulo esta vacio, el estado es corrupto y debe limpiarse sin tocar la capa. Esto puede ocurrir si una operacion anterior fallo a medio camino.
2. **Guardia 2:** Capa no modificable. Si la capa activa es nula (se elimino entre que se creo la seleccion y se hizo commit) o esta bloqueada, no se debe escribir. Se limpia el estado flotante para que la UI no quede colgada.
3. **Guardia 3:** Error de lectura. `readLayerPixels()` puede retornar un `QImage` nulo si la capa raster no tiene buffer de pixels asignado (caso borde: capa vectorial pura o capa vacia).

**Por que se usa `originalFloatingSelection_` y no `floatingSelection_`:**
`originalFloatingSelection_` es la copia inmutable extraida en el momento del cut. `floatingSelection_` puede haber sido modificada (ej. por redimensionado o transformacion de la seleccion). Usar `originalFloatingSelection_` garantiza que se estampa exactamente lo que se corto, sin alteraciones intermedias.

### 3.3 setTool() -- Guardia de Commit Antes de Cambio de Herramienta

```cpp
// canvas_widget.cpp:105-138
void CanvasWidget::setTool(int tool) {
    // Fase 1: Si hay seleccion flotante con datos validos y el destino NO es
    //         Seleccion (9) ni Move (8), comprometer antes de cambiar.
    //         Esto evita el Bug #2 (Perdida de Datos en Transicion).
    if (floatingActive_ && !originalFloatingSelection_.isNull()
        && selRect_.isValid() && tool != 9 && tool != 8) {
        commitFloatingSelection();
    } else if (floatingActive_) {
        // Estado flotante inconsistente (active=true pero sin datos):
        // limpiar sin commit.
        floatingActive_ = false;
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        selImage_ = QImage();
        selRect_ = QRectF();
    }

    tool_ = tool;
    selState_ = SelectionState::None;
    selHandleIdx_ = -1;

    // Fase 2: Si la nueva herramienta no es Seleccion ni Move,
    //         limpiar cualquier estado flotante residual.
    if (tool != 9 && tool != 8) {
        originalFloatingSelection_ = QImage();
        floatingSelection_ = QImage();
        selImage_ = QImage();
        selRect_ = QRectF();
        floatingActive_ = false;
    }

    // Preparar herramienta Seleccion sin datos previos
    if (tool == 9 && !floatingActive_) {
        selRect_ = QRectF();
        selImage_ = QImage();
    }

    // Resetear estado de Transform
    transformActive_ = false;
    activeHandle_ = -1;

    if (tool == 11) {
        scaleX_ = 1.0; scaleY_ = 1.0; translate_ = QPointF(0, 0);
        transformActive_ = true;
        ensureTransformBounds();
        setCursor(Qt::CrossCursor);
    } else if (tool == 10) setCursor(Qt::OpenHandCursor);
    else if (tool == 2) setCursor(Qt::CrossCursor);
    else if (tool == 8) setCursor(Qt::ArrowCursor);
    else if (tool == 4) setCursor(Qt::IBeamCursor);
    else if (tool == 9) setCursor(Qt::CrossCursor);
    else setCursor(Qt::CrossCursor);
    update();
}
```

**Analisis del flujo logico de setTool:**

```
setTool(nuevoTool)
  |
  +--> floatingActive_ && datos_validos?
  |      |
  |      +-- SI, nuevoTool NO es 9 ni 8 --> commitFloatingSelection() [PRESERVA DATOS]
  |      +-- SI, nuevoTool ES 9 u 8 -----> no hacer commit (la seleccion sigue viva)
  |      +-- floatingActive_ inconsistente -> limpiar sin commit
  |
  +--> Fase 2: nuevoTool NO es 9 ni 8?
  |      |
  |      +-- SI --> limpieza dura del estado flotante
  |      +-- NO --> conservar estado (para Move y Select)
  |
  +--> tool_ = nuevoTool
  +--> Resetear selState_ y selHandleIdx_
  +--> Configurar cursor segun herramienta
  +--> update()
```

### 3.4 commitSelection() -- El "Cut" Inicial con Clear y Qt::transparent

```cpp
// canvas_widget.cpp:1325-1375
void CanvasWidget::commitSelection() {
    // Guardia: rectangulo demasiado pequenio -> descartar sin efectos
    if (!selRect_.isValid() || selRect_.width() < 3 || selRect_.height() < 3) {
        selRect_ = QRectF();
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        update();
        return;
    }

    // selImage_: snapshot del composite (todas las capas visibles combinadas)
    // para que la herramienta Move pueda mover el contenido visual completo.
    QImage composite = compositeFrame(currentFrame_);
    selImage_ = composite.copy(selRect_.toAlignedRect());

    Layer* layer = resolveCurrentLayer();
    if (!layer || layer->locked()) {
        qWarning() << "[commitSelection] Layer null or locked, aborting";
        update();
        return;
    }

    qreal ox = 0, oy = 0;
    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        ox = r->originX(); oy = r->originY();
    }

    QImage layerPixels = readLayerPixels(layer);
    if (layerPixels.isNull()) {
        qWarning() << "[commitSelection] readLayerPixels returned null, aborting cut";
        update();
        return;
    }

    // Extraer los pixeles del area seleccionada para la seleccion flotante
    QRectF bufRect = selRect_.translated(-ox, -oy);
    floatingSelection_ = layerPixels.copy(bufRect.toAlignedRect());
    originalFloatingSelection_ = floatingSelection_;  // Fuente de verdad inmutable

    auto beforeSnap = snapCurrentLayer();

    // --- EL CUT: Borrar pixeles originales de la capa ---
    // CompositionMode_Clear pone (R=0, G=0, B=0, A=0) en cada pixel del rect.
    // Qt::transparent se usa como color de relleno (es ARGB(0,0,0,0)).
    // Esto evita el Bug #3b (ghosting/duplicacion).
    {
        QPainter cp(&layerPixels);
        cp.setCompositionMode(QPainter::CompositionMode_Clear);
        cp.translate(-ox, -oy);
        cp.fillRect(selRect_, Qt::transparent);
        cp.end();
    }

    writeLayerPixels(layer, layerPixels);
    pushFullLayerUndo(layer, beforeSnap, QStringLiteral("Cut Selection"));

    floatingActive_ = true;
    marchingTimer_.start();
    update();
    emit canvasUpdated();
}
```

**Detalle del sistema de coordenadas en el cut:**

`RasterLayer` tiene un sistema de coordenadas con origen `(originX_, originY_)`. Los pixeles se almacenan en un buffer lineal que cubre desde `(originX_, originY_)` hasta `(originX_ + width_, originY_ + height_)`. Para acceder al pixel en coordenadas de canvas `(cx, cy)`, se usa `(cx - originX_, cy - originY_)` como indice dentro del buffer.

- `selRect_` esta en coordenadas de canvas (espacio global del documento).
- `layerPixels` es un `QImage` que representa el buffer raster alineado a su origen. Es decir, el pixel `(0,0)` de `layerPixels` corresponde a `(originX_, originY_)` en coordenadas de canvas.
- Para copiar la region seleccionada: `bufRect = selRect_.translated(-ox, -oy)` convierte de coordenadas canvas a coordenadas locales del buffer.
- Para el cut: el `QPainter` sobre `layerPixels` tambien se translada `(-ox, -oy)` para que el `fillRect(selRect_, ...)` opere en las coordenadas correctas del buffer.

### 3.5 Blindaje en Guardado para Undo/Redo -- Evitar QImage Nulos

```cpp
// canvas_widget.cpp:592-608
void CanvasWidget::pushFullLayerUndo(Layer* layer, const QImage& beforeSnap,
                                     const QString& desc) {
    // BLINDAJE CRITICO: QImage nulo causa segfault en PaintCommand::undo()
    // cuando intenta acceder a scanLine() sobre bits() == nullptr.
    // Bug #4b resuelto con este early return.
    if (!layer || beforeSnap.isNull()) return;

    auto& strokes = vectorStrokes_[currentFrame_];
    auto beforeStrokes = strokes;
    QImage afterSnap = snapCurrentLayer();
    auto afterStrokes = strokes;

    QRect fullRect(0, 0, doc_->width(), doc_->height());
    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        fullRect = QRect(r->originX(), r->originY(), r->width(), r->height());
    }

    int modelIdx = currentRootLayer().indexOfLayer(layer);
    undoStack_.push(new PaintCommand(this, currentFrame_, modelIdx,
        layer->uid(), beforeSnap, std::move(afterSnap), fullRect,
        std::move(beforeStrokes), std::move(afterStrokes), desc));
}
```

**Por que `pushFullLayerUndo` captura el estado "after" dentro de la misma funcion:**

La funcion toma un snapshot `beforeSnap` ANTES de que la operacion modifique la capa, y captura `afterSnap` DESPUES. Esto significa que `pushFullLayerUndo` DEBE llamarse inmediatamente despues de `writeLayerPixels()` en el codigo llamante. El snapshot "after" incluye tanto los pixeles raster como los trazos vectoriales del frame actual. Esto permite que el comando `PaintCommand` restaure ambos aspectos simultaneamente durante undo/redo.

---

## 4. Recomendaciones Arquitectonicas (Next Steps)

### 4.1 Refactorizar el Despacho de inputPress/Move/Release

El metodo `inputPress()` (`canvas_widget.cpp:721`) contiene 170 lineas de despacho manual por `tool_` y `selState_` con logica anidada de handles, transform, seleccion, dibujo, y move. Este patron es fragil:

- Las transiciones entre herramientas durante un gesto activo no estan protegidas. Si el usuario cambia de herramienta via teclado mientras `drawing_ == true`, el estado de `strokePoints_` queda huerfano.
- La deteccion de handles de seleccion y transform esta duplicada (lineas 763-785 para seleccion, lineas 796-851 para transform).

**Recomendacion:** Extraer una jerarquia de clases `ToolHandler` (BrushHandler, EraserHandler, SelectHandler, TransformHandler, etc.) usando el patron Strategy/State. Cada handler recibe `press/move/release` y gestiona su propio estado interno. Esto eliminaria el `switch` implicito sobre `tool_` y haria testeable cada herramienta de forma aislada.

### 4.2 Auditoria de Ciclo de Vida de QImage en la Pila de Undo

`PaintCommand` (`undo_commands.hpp:32`) almacena `QImage` por valor (copia profunda gracias a COW implicito de Qt). Sin embargo, `MoveCommand` (linea 55) tambien almacena `QImage contentImage_` por valor. Si el tamano del documento crece (4K, 8K), cada entrada de undo puede consumir ~32-64 MB (1920x1080x4 bytes x 2 imagenes). Con el limite de 128 entradas en `UndoManager` (`core/undo_manager.hpp:17`), el consumo de RAM podria exceder 4 GB.

**Recomendacion:** Considerar compresion RLE o delta-encoding para `PaintCommand::beforePixels_`/`afterPixels_` cuando el dirtyRect cubre mas del 50% del canvas. Para `MoveCommand`, evaluar si `contentImage_` puede reemplazarse por una referencia al buffer original mas un rect de origen (evitando la copia).

### 4.3 Sincronizacion de Estado entre Capa Vectorial y Raster

El sistema hibrido vector+raster tiene un punto de friccion: `vectorStrokes_` se almacena en `CanvasWidget` (UI layer) pero `RasterLayer` vive en `core/`. Cuando se ejecuta `undo()` sobre un `PaintCommand`, se restauran tanto los pixels raster como el vector de `RawStroke`. Sin embargo, no hay garantia de que los `rasterEpochs_` se actualicen correctamente tras un undo/redo, lo que puede causar que `wrapRasterLayer()` devuelva una imagen stale.

**Recomendacion:** Mover `vectorStrokes_` al `Document` o a cada `GroupLayer` por frame, y hacer que `PaintCommand::undo()/redo()` fuerce un `bufferEpochTick()` explicito en la capa raster. Esto eliminaria la dependencia circular UI<->core y haria el undo/redo deterministico.

---

## 5. Guia Completa del Motor de Renderizado

### 5.1 Formato de Pixel y Profundidad de Color

Todo el pipeline de renderizado usa exclusivamente `QImage::Format_ARGB32_Premultiplied`. Este formato:

- Almacena 32 bits por pixel: 8 bits por canal (A, R, G, B) en orden dependiente de la plataforma.
- Los valores de color estan **premultiplicados** por alpha: `R_stored = R_original * A`, etc.
- Es el formato nativo de `QPainter` para composicion; usar otros formatos causa conversiones implicitas costosas.

**IMPORTANTE:** Las funciones que construyen `QImage` desde el buffer raw de `RasterLayer` (`wrapRasterLayer`, `readLayerPixels`) asumen que el buffer raw ya esta en formato `Format_ARGB32_Premultiplied`. El motor de capas (`RasterLayer`) almacena los pixeles como `std::vector<uint32_t>` donde cada `uint32_t` es un pixel ARGB premultiplied. La correspondencia es directa sin conversion.

### 5.2 Funciones de Lectura/Escritura de Capas

**`wrapRasterLayer(RasterLayer*)` (linea 472):**
Wrapper ligero que crea un `QImage` apuntando directamente a `rasterLayer->pixelData()` sin copia. Usa el constructor `QImage(const uchar*, int w, int h, int bytesPerLine, Format)`. Esto significa que el `QImage` resultante es una VISTA sobre la memoria de la capa; cualquier modificacion al `QImage` modifica la capa directamente. Usado exclusivamente en `paintEvent` para lectura (no se modifica).

Cachea el `bufferEpoch` de la capa en `rasterEpochs_[uid]`. Si el epoch no cambio desde la ultima lectura, reutiliza el mismo `QImage` (evitando recrear el wrapper). Si el epoch cambio, actualiza la entrada del cache.

```cpp
QImage CanvasWidget::wrapRasterLayer(RasterLayer* rasterLayer) {
    if (!rasterLayer) return QImage();
    const uint32_t* pixels = rasterLayer->pixelData();
    if (!pixels) return QImage();
    int w = rasterLayer->width();
    int h = rasterLayer->height();
    if (w <= 0 || h <= 0) return QImage();
    uint64_t epoch = rasterLayer->bufferEpoch();
    LayerUid uid = rasterLayer->uid();
    auto it = rasterEpochs_.find(uid);
    if (it != rasterEpochs_.end() && it->second != epoch) {
        it->second = epoch;
    } else if (it == rasterEpochs_.end()) {
        rasterEpochs_[uid] = epoch;
    }
    int bpl = w * static_cast<int>(sizeof(uint32_t));
    return QImage(reinterpret_cast<const uchar*>(pixels), w, h, bpl,
                  QImage::Format_ARGB32_Premultiplied);
}
```

**`readLayerPixels(Layer*)` (linea 491):**
Copia defensiva. Crea un `QImage` nuevo y copia los datos con `.copy()` al final (si la capa tiene pixels). Si la capa es nula o no tiene datos, retorna un `QImage` vacio relleno con `Qt::transparent` del tamano del documento. Usado por todas las operaciones de modificacion (`doErase`, `commitStroke`, `commitSelection`, `commitFloatingSelection`, etc.).

```cpp
QImage CanvasWidget::readLayerPixels(Layer* layer) {
    if (!layer || !doc_) {
        QImage empty(doc_ ? doc_->width() : 1920, doc_ ? doc_->height() : 1080,
                     QImage::Format_ARGB32_Premultiplied);
        empty.fill(Qt::transparent);
        return empty;
    }
    if (layer->type() == LayerType::Raster) {
        auto* raster = static_cast<RasterLayer*>(layer);
        if (raster->pixelData() && raster->pixelCount() > 0) {
            return QImage(reinterpret_cast<const uchar*>(raster->pixelData()),
                         raster->width(), raster->height(),
                         raster->width() * static_cast<int>(sizeof(uint32_t)),
                         QImage::Format_ARGB32_Premultiplied).copy();
        }
    }
    QImage empty(doc_->width(), doc_->height(), QImage::Format_ARGB32_Premultiplied);
    empty.fill(Qt::transparent);
    return empty;
}
```

**`writeLayerPixels(Layer*, QImage)` (linea 553):**
Escribe un `QImage` de vuelta al buffer raw de `RasterLayer`. Convierte el `QImage` a `Format_ARGB32_Premultiplied`, expande la region de la capa con `ensureContains()`, y copia los pixeles linea por linea.

```cpp
void CanvasWidget::writeLayerPixels(Layer* layer, const QImage& img) {
    if (!layer || img.isNull()) return;
    if (layer->type() == LayerType::Raster) {
        auto* raster = static_cast<RasterLayer*>(layer);
        if (img.width() <= 0 || img.height() <= 0) return;
        int ox = raster->originX();
        int oy = raster->originY();
        raster->ensureContains(ox, oy, img.width(), img.height());
        QImage converted = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        int dstX = ox - raster->originX();
        int dstY = oy - raster->originY();
        int w = std::min(converted.width(), raster->width() - dstX);
        int h = std::min(converted.height(), raster->height() - dstY);
        for (int y = 0; y < h; ++y) {
            const uint32_t* src = reinterpret_cast<const uint32_t*>(converted.constScanLine(y));
            if (!src) continue;
            uint32_t* dst = raster->pixelData();
            if (!dst) continue;
            std::copy(src, src + w, dst + (dstY + y) * raster->width() + dstX);
        }
    }
}
```

**`readRasterRect(RasterLayer*, QRect)` (linea 511) y `writeRasterRect(RasterLayer*, QRect, QImage)` (linea 533):**
Versiones de lectura/escritura por region rectangular. Usadas por `commitStroke()` para guardar solo el dirtyRect en el `PaintCommand` (en lugar de la capa entera). La version de escritura tiene una duplicada en `undo_commands.cpp:30` (funcion libre `writeRasterRect`) que usa `PaintCommand` durante undo/redo.

### 5.3 Funcion compositeFrame()

```cpp
// canvas_widget.cpp:614-636
QImage CanvasWidget::compositeFrame(int frameNum) const {
    if (!doc_) return QImage();
    QImage result(doc_->width(), doc_->height(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter p(&result);
    auto& layers = currentRootLayer(frameNum).layers();
    for (auto it = layers.begin(); it != layers.end(); ++it) {
        Layer* layer = it->get();
        if (!layer->visible()) continue;
        p.setOpacity(layer->opacity());
        p.setCompositionMode(toQtCompositionMode(layer->blendMode()));
        if (layer->type() == LayerType::Raster) {
            RasterLayer* rasterLayer = static_cast<RasterLayer*>(layer);
            QImage img = const_cast<CanvasWidget*>(this)->wrapRasterLayer(rasterLayer);
            if (!img.isNull()) p.drawImage(rasterLayer->originX(), rasterLayer->originY(), img);
        } else if (layer->type() == LayerType::Vector) {
            const_cast<CanvasWidget*>(this)->renderVectorStrokes(p, frameNum, layer->uid());
        }
    }
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setOpacity(1.0);
    p.end();
    return result;
}
```

Esta funcion compone todas las capas de un frame en un solo `QImage`. Se usa para:
- `commitSelection()`: obtener el snapshot visual de todas las capas combinadas (`selImage_`).
- `doPickColor()`: obtener el color compuesto bajo el cursor.
- `copySelection()`: copiar el contenido visual de la seleccion al portapapeles.

Notar el uso de `const_cast` para llamar a metodos no-const desde un metodo const. Esto es un code smell que deberia resolverse haciendo que `wrapRasterLayer` y `renderVectorStrokes` sean metodos const.

---

## 6. Sistema de Coordenadas y Transformaciones

### 6.1 Dos Espacios de Coordenadas

El sistema maneja dos espacios de coordenadas distintos:

| Espacio | Unidad | Origen | Uso |
|---------|--------|--------|-----|
| **Screen** (pantalla) | Pixeles del widget | Esquina superior izquierda del `CanvasWidget` | Eventos de mouse/tablet, posicion del cursor |
| **Canvas** (documento) | Pixeles del documento (1920x1080 por defecto) | Esquina superior izquierda del documento | Coordenadas de dibujo, seleccion, capas |

**Funciones de conversion:**

```cpp
QPointF CanvasWidget::screenToCanvas(QPointF sp) const {
    return (sp - QPointF(offX_, offY_)) / zoom_;
}

QPointF CanvasWidget::canvasToScreen(QPointF cp) const {
    return cp * zoom_ + QPointF(offX_, offY_);
}
```

**Cadena de transformacion completa (screen -> canvas con rotacion y flip):**

En `paintEvent`, el `QPainter` se configura asi:

```
p.translate(offX_, offY_)         // Mueve origen al centro del canvas
p.scale(zoom_, zoom_)             // Aplica zoom
[si flipH_]
  p.translate(cw, 0)              // Mueve origen a borde derecho
  p.scale(-1.0, 1.0)             // Invierte eje X
[si rotation_ != 0]
  p.translate(cw/2, ch/2)        // Mueve origen al centro del documento
  p.rotate(rotation_)             // Rota
  p.translate(-cw/2, -ch/2)      // Restaura origen
```

**IMPORTANTE:** Las funciones `screenToCanvas` y `canvasToScreen` NO aplican rotacion ni flip. Solo manejan zoom y offset. La rotacion y el flip se aplican unicamente a nivel de `QPainter` durante el renderizado. Esto significa que las coordenadas de dibujo y seleccion estan siempre en el espacio del documento sin rotacion, y el renderizado se encarga de la transformacion visual.

### 6.2 Ajuste Automatico de Vista (fit)

```cpp
void CanvasWidget::fit() {
    if (width() == 0 || height() == 0 || !doc_) return;
    double pad = 1.0;
    double zw = static_cast<double>(width() - pad * 2) / doc_->width();
    double zh = static_cast<double>(height() - pad * 2) / doc_->height();
    zoom_ = std::min(zw, zh);
    offX_ = std::round((width() - doc_->width() * zoom_) / 2.0);
    offY_ = std::round((height() - doc_->height() * zoom_) / 2.0);
    update();
}
```

Calcula el zoom maximo que permite ver el documento completo dentro del widget (manteniendo relacion de aspecto), y centra el canvas horizontal y verticalmente. Se llama en `resizeEvent`, `showEvent`, y con la tecla `F`.

### 6.3 Zoom con Rueda del Mouse

```cpp
void CanvasWidget::wheelEvent(QWheelEvent* ev) {
    double factor = 1.12;
    QPointF mpos = ev->position();
    QPointF oldCpos = screenToCanvas(mpos);
    zoom_ = std::clamp(ev->angleDelta().y() > 0 ? zoom_ * factor : zoom_ / factor, 0.02, 64.0);
    QPointF newCpos = screenToCanvas(mpos);
    offX_ = std::round(offX_ + (newCpos.x() - oldCpos.x()) * zoom_);
    offY_ = std::round(offY_ + (newCpos.y() - oldCpos.y()) * zoom_);
    update();
}
```

El zoom se realiza alrededor del punto bajo el cursor. La formula ajusta `offX_/offY_` para que el punto en coordenadas de canvas bajo el cursor permanezca en el mismo lugar en pantalla despues del zoom. El rango de zoom es [0.02, 64.0].

### 6.4 Pan (Hand Tool y Middle Button)

El pan se activa con:
- Herramienta Hand (10) + boton izquierdo
- Boton central del mouse en cualquier herramienta

```cpp
// En mousePressEvent (linea 656-659):
if (ev->button() == Qt::MiddleButton || (tool_ == 10 && ev->button() == Qt::LeftButton)) {
    panning_ = true;
    panStart_ = ev->position();
    panOffStart_ = QPointF(offX_, offY_);
    setCursor(Qt::ClosedHandCursor);
    return;  // No llama a inputPress()
}

// En mouseMoveEvent (linea 667):
if (panning_) {
    QPointF delta = ev->position() - panStart_;
    offX_ = std::round(panOffStart_.x() + delta.x());
    offY_ = std::round(panOffStart_.y() + delta.y());
    update();
    return;  // No llama a inputMove()
}

// En mouseReleaseEvent (linea 675):
if (panning_) {
    panning_ = false;
    setCursor(tool_ == 10 ? Qt::OpenHandCursor : Qt::CrossCursor);
    return;
}
```

El pan se aplica directamente modificando `offX_/offY_`. Como estos offsets estan en coordenadas de pantalla, el desplazamiento es 1:1 con el movimiento del mouse (independiente del zoom).

---

## 7. Guia Exhaustiva de Cada Herramienta

### 7.0 Tabla de Herramientas

| Indice | Herramienta | Tecla | Tipo de operacion | Usa capa | Genera Undo |
|--------|-------------|-------|-------------------|----------|-------------|
| 0  | Brush        | B | Dibujo a mano alzada | Si (capa activa) | Si (PaintCommand) |
| 1  | Eraser       | E | Borrado a mano alzada | Si (capa activa) | Si (PaintCommand) |
| 2  | PickColor    | I | Seleccion de color | No (lee composite) | No |
| 3  | Fill         | G | Relleno flood-fill | Si (capa activa) | Si (PaintCommand) |
| 4  | Text         | T | Texto | Si (capa activa) | Si (PaintCommand) |
| 5  | Line         | L | Linea recta | Si (capa activa) | Si (PaintCommand) |
| 6  | Rectangle    | U | Rectangulo | Si (capa activa) | Si (PaintCommand) |
| 7  | Ellipse      | Y | Elipse | Si (capa activa) | Si (PaintCommand) |
| 8  | Move         | M | Desplazar capa/seleccion | Si (capa activa) | Si (MoveCommand) |
| 9  | Select       | S | Seleccion rectangular + flotante | Si (capa activa) | Si (PaintCommand en cut/commit) |
| 10 | Hand         | H | Pan (desplazar vista) | No | No |
| 11 | Transform    | W | Escala/traslacion libre | Si (capa activa) | Si (PaintCommand) |

### 7.1 Brush (0) -- Pincel

**Flujo completo:**
1. `inputPress`: Verifica que haya capa activa y no este bloqueada. Si hay seleccion flotante activa, la compromete primero (`commitFloatingSelection()`). Configura `drawing_ = true`, llama a `brush_->beginStroke()` con el color y grosor actuales, toma snapshot `beforeSnap`, y anade el primer `StrokePoint`.
2. `inputMove`: Aplica suavizado (stabilizer) si esta configurado, filtra puntos demasiado cercanos (< 0.5 px), acumula puntos en `strokePoints_` y en el `BrushEngine`.
3. `inputRelease`: Llama a `brush_->endStroke()`, luego `commitStroke()`.
4. `commitStroke()`: Calcula el dirty rect expandido, guarda `beforePixels` (region raster), asegura que la capa contenga todos los puntos del trazo (`ensureContains`), crea un `RawStroke` con todos los parametros (presion, opacidad, tamano), lo anade a `vectorStrokes_[currentFrame_]`, rasteriza el trazo sobre la capa usando `renderVectorStroke()`, guarda `afterPixels`, y empuja un `PaintCommand` a la pila undo.

**Parametros del pincel controlados por `BrushPreset`:**
- `size`: grosor base del pincel (px)
- `opacity`: opacidad global del trazo
- `color`: color base (se multiplica por opacity)
- `dynamics.pressure_size`: si es true, el grosor varia con la presion
- `dynamics.pressure_opacity`: si es true, la opacidad varia con la presion
- `dynamics.smoothing`: nivel de suavizado (0.0 a 1.0)

### 7.2 Eraser (1) -- Goma

**Flujo completo:**
1. `inputPress`: Similar a Brush, pero no llama a `brush_->beginStroke()` (la goma no usa el motor de pincel).
2. `inputMove`: Acumula puntos en `strokePoints_`. El preview en `paintLiveStroke` dibuja circulos blancos semi-transparentes.
3. `inputRelease`: Llama a `doErase()`.
4. `doErase()` (`canvas_widget.cpp:988`): Toma snapshot antes. Lee la capa raster. Renderiza los trazos vectoriales existentes sobre la imagen de capa (para tener el estado completo actual). Luego abre un `QPainter` con `CompositionMode_DestinationOut` y dibuja circulos/lineas con `QPen(Qt::black, sz)`. `DestinationOut` usa el alpha del source (negro = alpha=1) para reducir el alpha del destino, efectivamente "borrando" los pixeles. Finalmente elimina los `RawStroke` de la capa del mapa `vectorStrokes_`, escribe los pixeles modificados de vuelta a la capa, y empuja `PaintCommand`.

**IMPORTANTE:** `doErase()` opera sobre la capa raster directamente. Los trazos vectoriales se eliminan del mapa `vectorStrokes_` (linea 1053-1058) porque la goma los ha rasterizado y borrado. Esto significa que tras un borrado, los trazos borrados no se pueden reeditar como vectores.

**Diferencia con Brush:** La goma usa `CompositionMode_DestinationOut` en lugar de `SourceOver`. Esto reduce el canal alpha del destino proporcionalmente al alpha de la fuente, creando un efecto de borrado. NO usa `CompositionMode_Clear` porque `Clear` borraria el rectangulo completo del viewport, no solo el trazo.

### 7.3 PickColor (2) -- Cuentagotas

**Flujo:**
1. `inputPress`: Llama a `doPickColor(cpos)`.
2. `doPickColor()` (`canvas_widget.cpp:1206`): Compone el frame actual (`compositeFrame`), lo dibuja sobre fondo blanco (para eliminar transparencia y obtener el color real percibido), lee el color del pixel bajo el cursor, actualiza `color_` y `BrushEngine::preset().color`, emite `colorPicked(color)` y `colorCommittedToRecent(color)`.

**IMPORTANTE:** El color se lee del composite COMPLETO (todas las capas combinadas), no de la capa activa individualmente.

### 7.4 Fill (3) -- Relleno (Flood Fill)

**Flujo:**
1. `inputPress`: Verifica capa activa no bloqueada, llama a `doFill(cpos)`.
2. `doFill()` (`canvas_widget.cpp:1177`): Lee la capa raster activa. Obtiene el color objetivo en el punto clickeado. Si el alpha del objetivo es < 20, usa `floodFillByAlpha` (relleno por region transparente). Si no, usa `floodFill` (relleno por similitud de color con tolerancia 32). Finalmente escribe la capa modificada y empuja `PaintCommand`.

**Algoritmo de flood fill (`floodFill`, linea 1731):**
Implementacion iterativa con stack (no recursiva, evita stack overflow). Usa el algoritmo clasico de scanline: para cada pixel semilla, expande horizontalmente a izquierda y derecha mientras los pixeles sean similares al color objetivo (tolerancia por canal RGBA), y apila los pixeles de las filas superior e inferior para continuar el relleno.

**Algoritmo de flood fill por alpha (`floodFillByAlpha`, linea 1762):**
Similar pero usa un umbral de alpha (`boundaryAlpha=100`) en lugar de comparacion de color. Rellena todos los pixeles con alpha < boundaryAlpha.

### 7.5 Text (4) -- Texto

**Flujo:**
1. `inputPress`: Muestra `QInputDialog::getMultiLineText` para ingresar el texto, luego `QFontDialog::getFont` para elegir la fuente.
2. `doText()` (`canvas_widget.cpp:1233`): Si el usuario confirmo ambos dialogos, renderiza el texto sobre la capa raster activa y empuja `PaintCommand`.

### 7.6 Line (5), Rectangle (6), Ellipse (7) -- Formas Geometricas

**Flujo:**
1. `inputPress`: Guarda `shapeStart_ = cpos`, `shapeEnd_ = cpos`, `drawing_ = true`.
2. `inputMove`: Actualiza `shapeEnd_ = cpos`. El `paintEvent` muestra un preview con `paintShapePreview` (borde azul dashed).
3. `inputRelease`: Llama a `drawShape()`.
4. `drawShape()` (`canvas_widget.cpp:1149`): Toma snapshot antes, asegura que la capa contenga la forma, renderiza la forma geometrica sobre la imagen de capa, escribe, y empuja `PaintCommand`.

**IMPORTANTE:** A diferencia de Brush, las formas geometricas NO generan `RawStroke`. Se rasterizan directamente sobre la capa. Son operaciones puramente raster, no editables como vectores.

### 7.7 Move (8) -- Desplazar

**Flujo:**
1. `inputPress`: Llama a `startMove(cpos)`.
2. `startMove()` (`canvas_widget.cpp:1262`): Determina el modo:
   - Si hay `selImage_` y `selRect_` validos: modo seleccion (`moveModeSelection_=true`). Mueve solo el contenido de la seleccion.
   - Si no: modo capa completa (`moveModeSelection_=false`). Mueve toda la capa activa.
   - Guarda `moveImage_`, `moveSrcRect_`, activa `moving_=true`.
3. `inputMove`: Calcula `moveOffset_` y llama a `update()`.
4. `inputRelease`: Llama a `commitMove()`.
5. `commitMove()` (`canvas_widget.cpp:1292`): Clampa el delta de movimiento a [-40000, 40000], crea un `MoveCommand` con el rectangulo origen, el delta, la imagen de contenido y el flag `isSelection_`, lo empuja a la pila undo, y limpia el estado.

**MoveCommand::redo()** (`undo_commands.cpp:175`):
1. Asegura que la capa contenga tanto el rect origen como el destino (`united`).
2. Crea un `QImage` copia del buffer raster completo.
3. Usa `CompositionMode_DestinationOut` para borrar los pixeles en el rect destino.
4. Usa `CompositionMode_SourceOver` para dibujar el contenido en el rect origen.
5. Copia el resultado de vuelta al buffer de la capa.

**MoveCommand::undo():**
Inverso: borra el rectangulo movido y restaura el contenido en el rectangulo origen.

### 7.8 Select (9) -- Seleccion

**Flujo completo detallado:**

**Fase 1 - Creacion del rectangulo (Creating):**
1. `inputPress` (tool=9, sin floatingActive_): Si el click esta sobre un handle de seleccion existente, entra en `DraggingHandle`. Si esta dentro del `selRect_`, entra en `MovingContent`. Si esta fuera, inicia `Creating` (guarda `selStart_`).
2. `inputMove` (Creating): Actualiza `selRect_ = QRectF(selStart_, cpos).normalized()`.
3. `inputRelease` (Creating): Si `selRect_ < 3x3`, descarta. Si no, llama a `commitSelection()`.

**Fase 2 - Seleccion flotante activa (floatingActive_=true):**
El usuario puede:
- Mover la seleccion (MovingContent): arrastrar dentro del `selRect_`, traslada `selRect_` y actualiza.
- Redimensionar (DraggingHandle): arrastrar un handle, modifica los bordes del `selRect_`.
- Comprometer (Enter): llama a `commitFloatingSelection()`.
- Click fuera del selRect_: llama a `commitFloatingSelection()` y empieza nueva seleccion.

**Teclas activas durante seleccion flotante:**
- `Ctrl+C`: Copia la seleccion flotante al portapapeles.
- `Ctrl+X`: Corta (copia al portapapeles + borra de la capa con undo).
- `Ctrl+V`: Pega el portapapeles como nueva seleccion flotante.
- `Delete`: Borra la seleccion flotante (sin copiar al portapapeles).
- `Enter`: Compromete la seleccion flotante a la capa.

### 7.9 Hand (10) -- Mano (Pan)

No llama a `inputPress/Move/Release`. El pan se maneja directamente en `mousePressEvent/Move/Release` (ver seccion 6.4).

### 7.10 Transform (11) -- Escala y Traslacion Libre

**Flujo:**
1. Al activar la herramienta (`setTool(11)`), `transformActive_=true`, `scaleX_=scaleY_=1.0`, `translate_=(0,0)`, y se llama a `ensureTransformBounds()`.
2. `ensureTransformBounds()`: Si hay `floatingActive_`, `transformBounds_ = selRect_`. Si no, usa el rect de la capa raster activa.
3. `inputPress`:
   - Hit test contra 8 handles transformados + centro.
   - Si golpea un handle (0-7): configura `handleFixedPoint_`, `handleScaleX_`, `handleScaleY_`.
   - Si golpea el centro (8): prepara para traslacion (`activeHandle_=8`).
4. `inputMove` (activeHandle_ >= 0):
   - Handle 8 (traslacion): `translate_ = dragTranslateRef_ + (cpos - transformDragOrigin_)`.
   - Handle 0-7 (escala): calcula nueva escala basada en la distancia al punto fijo. Permite escala negativa (reflejo).
5. `inputRelease`: `activeHandle_ = -1`.
6. Compromiso (Enter): `commitTransform()`.

**commitTransform()** (`canvas_widget.cpp:1448`):
1. Lee la capa raster activa.
2. Crea un `QImage` destino transparente del mismo tamano.
3. Aplica `QTransform` con traslacion al centro + escala + traslacion inversa.
4. Dibuja la imagen fuente a traves del transform.
5. Escribe el resultado y empuja `PaintCommand`.
6. Resetea parametros de transform.

---

## 8. Sistema de Doble Pila Undo/Redo

El proyecto mantiene DOS sistemas de undo/redo independientes:

### 8.1 QUndoStack de Qt (UI Layer)

Ubicado en `CanvasWidget::undoStack_` (`canvas_widget.hpp:205`). Es la pila principal usada por todas las operaciones del canvas. Los comandos son:

| Comando | id() | Proposito | Datos almacenados |
|---------|------|-----------|-------------------|
| `PaintCommand` | 1 | Deshacer/rehacer operaciones de dibujo | beforePixels, afterPixels, dirtyRect, beforeStrokes, afterStrokes |
| `MoveCommand` | 2 | Deshacer/rehacer movimiento de capa/seleccion | srcRect, delta, contentImage, isSelection |
| `DeleteLayerCommand` | 3 | Deshacer/rehacer eliminacion de capa | unique_ptr<Layer> storedLayer |
| `RenameLayerCommand` | 4 | Deshacer/rehacer renombre de capa | oldName, newName |
| `ToggleVisibilityCommand` | 5 | Deshacer/rehacer visibilidad de capa | layerUid |
| `SetBlendModeCommand` | 6 | Deshacer/rehacer modo de fusion | oldMode, newMode |

**Jerarquia de herencia:**
```
QUndoCommand
  +-- CanvasCommand (base: canvas_, frame_, layerModelIdx_)
        +-- PaintCommand
        +-- MoveCommand
        +-- DeleteLayerCommand
        +-- RenameLayerCommand
        +-- ToggleVisibilityCommand
        +-- SetBlendModeCommand
```

**CanvasCommand** (`undo_commands.hpp:16`): Clase base que proporciona acceso al canvas, el frame y el indice de capa. Sus metodos `root()`, `activeLayer()`, y `repaint()` son usados por los comandos derivados.

**PaintCommand::mergeWith()** (`undo_commands.cpp:113`): Soporta compresion de comandos consecutivos. Si el usuario hace multiples trazos seguidos con la misma herramienta sobre la misma capa, los comandos se fusionan en uno solo (actualizando `afterPixels_` y expandiendo `dirtyRect_` con `united()`). Esto evita que la pila crezca descontroladamente con trazos pequenos.

**CanvasWidget::pushFullLayerUndo()** (`canvas_widget.cpp:592`): Metodo helper que captura el estado completo de la capa (pixeles raster + trazos vectoriales) antes y despues de una operacion, y empuja un `PaintCommand` con `dirtyRect` igual al rectangulo completo de la capa. Usado por operaciones que afectan a toda la capa (seleccion, transform, relleno, texto, formas).

**CanvasWidget::undo() y redo()** (`canvas_widget.cpp:1799-1811`): Simples wrappers que invocan `undoStack_.undo()` / `undoStack_.redo()` y emiten `canvasUpdated()`.

### 8.2 UndoManager del Core (Engine Layer)

Ubicado en `Document::undo_manager_` (`core/undo_manager.hpp:22`). Es un sistema independiente de Qt, diseniado para el motor de capas. Almacena hasta 128 entradas.

```cpp
struct UndoEntry {
    int layerIndex = 0;
    int frame = 0;
    std::vector<uint32_t> rasterData;   // Buffer raster completo (copia)
    int rasterWidth = 0;
    int rasterHeight = 0;
    std::vector<VectorStroke> vectorData; // Trazos vectoriales
    bool hasRaster = false;
    bool hasVector = false;
};
```

**IMPORTANTE:** Este sistema de undo engine-level NO esta integrado con el `QUndoStack` del UI. Actualmente, el `UndoManager` del core parece ser un remanente de una version anterior o una funcionalidad planeada pero no conectada. Las operaciones del canvas usan exclusivamente el `QUndoStack`. Si se planea unificar, hay que decidir cual de los dos sistemas prevalece.

---

## 9. Modelo de Capas en Profundidad

### 9.1 Jerarquia de Clases

```
Layer (abstracta)
  +-- RasterLayer
  +-- VectorLayer
  +-- GroupLayer
```

### 9.2 Layer (Clase Base) -- `core/layer.hpp:17`

Propiedades comunes a todas las capas:

| Propiedad | Tipo | Default | Setter/Getter |
|-----------|------|---------|---------------|
| `uid_` | `LayerUid` (uint64_t) | generado | `uid()` |
| `name_` | `std::string` | "" | `setName()` / `name()` |
| `type_` | `LayerType` | segun subclase | `type()` |
| `visible_` | `bool` | true | `setVisible()` / `visible()` |
| `opacity_` | `float` | 1.0f | `setOpacity()` / `opacity()` |
| `blend_mode_` | `BlendMode` | Normal | `setBlendMode()` / `blendMode()` |
| `locked_` | `bool` | false | `setLocked()` / `locked()` |

Cada capa tiene un `uid_` unico generado por `generateLayerUid()` (probablemente un contador atomico o random). El UID se usa para identificar capas de forma estable a traves de reordenamientos y para asociar `RawStroke` a capas especificas.

`Layer` hereda de `NonCopyable` (constructor de copia y operador de asignacion eliminados). Para duplicar capas, se usa el metodo virtual `clone()`.

### 9.3 RasterLayer -- `core/layer.hpp:48`

Capa basada en pixeles. Sus datos principales:

```cpp
int width_;                          // Ancho logico del buffer
int height_;                         // Alto logico del buffer
int originX_ = 0;                    // Coordenada X del pixel (0,0) en espacio canvas
int originY_ = 0;                    // Coordenada Y del pixel (0,0) en espacio canvas
std::vector<uint32_t> pixels_;       // Buffer de pixeles ARGB32 premultiplied
uint64_t buffer_epoch_ = 0;          // Contador de version (incrementa en cada modificacion)
```

**Layout de memoria del buffer:**
Los pixeles se almacenan en orden row-major: `pixels_[y * width_ + x]`. Cada `uint32_t` codifica un pixel en formato ARGB (el orden de bytes depende de la plataforma; Qt lo abstrae con `Format_ARGB32_Premultiplied`).

**Origen variable (originX_, originY_):**
A diferencia de un canvas tradicional donde el buffer siempre empieza en (0,0), `RasterLayer` permite que el origen del buffer este desplazado. Esto es necesario porque `ensureContains()` puede expandir el buffer hacia coordenadas negativas o mas alla de los limites originales. Por ejemplo, si el usuario dibuja en la posicion (-100, 50), la capa se expande para incluir esa region, desplazando el origen.

**`ensureContains(int x, int y, int w, int h)` (`core/layer.hpp:69`):**
Garantiza que el buffer de la capa contenga el rectangulo especificado. Si el rectangulo se sale de los limites actuales, redimensiona el buffer y ajusta `originX_/originY_`. Esta funcion se llama antes de cualquier operacion de escritura.

**`bufferEpochTick()` / `bufferEpoch()`:**
Sistema de versionado. Cada vez que se modifica el buffer, se incrementa `buffer_epoch_`. El `CanvasWidget` usa este epoch en `rasterEpochs_` para cachear wrappers `QImage` y evitar recrearlos si el buffer no cambio.

### 9.4 VectorLayer -- `core/layer.hpp:83`

Capa basada en trazos vectoriales Bezier. Almacena `std::vector<VectorStroke>` donde cada `VectorStroke` contiene un `BezierPath`, `Color`, `width`, `opacity` y `BlendMode`.

**IMPORTANTE:** `VectorLayer` NO se usa actualmente en el pipeline de renderizado principal. El sistema hibrido actual almacena los trazos vectoriales en `CanvasWidget::vectorStrokes_` (como `RawStroke`) y los renderiza directamente sobre las capas raster. `VectorLayer` parece ser una abstraccion planeada para el futuro donde las capas vectoriales sean entidades independientes, no vinculadas a una capa raster.

### 9.5 GroupLayer -- `core/layer.hpp:101`

Capa compuesta que contiene una coleccion ordenada de subcapas. Es la raiz de la jerarquia de capas para cada frame.

```cpp
std::deque<std::unique_ptr<Layer>> layers_;
```

**Operaciones de gestion:**
- `addLayer(unique_ptr<Layer>)`: Agrega al final (capa mas al frente).
- `removeLayer(int index)`: Elimina por indice.
- `moveLayer(int from, int to)`: Reordena.
- `duplicateLayer(int index)`: Clona una capa via `clone()` y la agrega.
- `layerByUid(LayerUid)`: Busqueda por UID (O(n)).
- `indexOfLayer(Layer*)`: Busca el indice de un puntero (O(n)).

**Estructura del documento (Document, `core/document.hpp:14`):**
```cpp
std::map<int, std::unique_ptr<GroupLayer>> frames_;
```
Cada frame tiene su propio `GroupLayer` raiz. El frame 0 siempre existe. Los frames intermedios se crean bajo demanda. Las funciones `rootLayerForFrame(int)` y `peekRootLayerForFrame(int)` acceden al `GroupLayer` de un frame especifico.

---

## 10. Sistema de Trazos Vectoriales (RawStroke)

### 10.1 Estructura RawStroke

Definida en `canvas_widget.hpp:27-36`:

```cpp
struct RawStroke {
    LayerUid layerUid = 0;              // UID de la capa a la que pertenece
    std::vector<StrokePoint> points;    // Puntos del trazo
    QColor color;                       // Color del trazo
    float baseWidth = 10.0f;            // Grosor base en px
    float opacity = 1.0f;              // Opacidad global
    bool pressureSize = true;           // Variar grosor con presion
    bool pressureOpacity = false;       // Variar opacidad con presion
    bool eraser = false;                // TRUE si es un trazo de goma
};
```

### 10.2 Estructura StrokePoint

Definida en `core/types.hpp:154-161`:

```cpp
struct StrokePoint {
    Vec2 position;          // Coordenadas (x, y) en espacio canvas
    float pressure = 1.0f;  // Presion del stylus [0.0, 1.0]
    float tilt_x = 0.0f;    // Inclinacion X (no usado actualmente)
    float tilt_y = 0.0f;    // Inclinacion Y (no usado actualmente)
    float rotation = 0.0f;  // Rotacion del stylus (no usado actualmente)
    float timestamp = 0.0f; // Timestamp (no usado actualmente)
};
```

### 10.3 Almacenamiento y Ciclo de Vida

Los `RawStroke` se almacenan en `std::map<int, std::vector<RawStroke>> vectorStrokes_`, indexados por numero de frame. Cada frame tiene su propio vector de trazos. Los trazos se identifican como pertenecientes a una capa especifica mediante `layerUid`.

**Cuando se crean:**
- `commitStroke()` (Brush): crea un `RawStroke` con `eraser=false` y lo agrega a `vectorStrokes_[currentFrame_]`.
- `doErase()` (Eraser): NO crea `RawStroke`. En su lugar, elimina los `RawStroke` existentes de la capa objetivo.

**Cuando se eliminan:**
- `doErase()`: elimina todos los `RawStroke` cuyo `layerUid` coincida con la capa borrada.
- `flattenVectorStrokes(int frame)`: consolida los trazos vectoriales en la capa raster y los elimina del mapa.
- `shiftFrameData(int from, int delta)`: reorganiza al insertar/eliminar frames.
- `removeFrameData(int frameIdx)`: elimina todos los trazos de un frame.

### 10.4 Renderizado de Trazos Vectoriales

`renderVectorStroke(QPainter& p, const RawStroke& vs)` (`canvas_widget.cpp:1638`):

**Caso 1: Trazo de goma (`eraser=true`):**
```cpp
p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
p.setPen(Qt::NoPen);
p.setBrush(Qt::black);
float r = vs.baseWidth * 0.5f;
for (const auto& pt : vs.points)
    p.drawEllipse(QPointF(pt.position.x, pt.position.y), r, r);
```
Dibuja circulos negros con `DestinationOut`, lo que reduce el alpha del destino. Efecto de borrado.

**Caso 2: Trazo con opacidad < 0.99 y presion por opacidad desactivada:**
Renderiza a una capa offscreen intermedia para aplicar opacidad de grupo. Esto es necesario porque dibujar segmentos de linea translucidos directamente causaria artefactos en las intersecciones (doble opacidad). La capa offscreen se dibuja con opacidad completa y luego se compone con la opacidad global.

**Caso 3: Trazo normal:**
Dibuja segmentos de linea directamente con `QPen(color, grosor)`. El grosor varia por punto segun la presion si `pressureSize=true`. La opacidad varia si `pressureOpacity=true`.

---

## 11. Pipeline de Entrada (Tablet, Mouse, Teclado)

### 11.1 Jerarquia de Eventos

```
QEvent
  +-- QMouseEvent    -> mousePressEvent, mouseMoveEvent, mouseReleaseEvent
  +-- QTabletEvent   -> tabletEvent
  +-- QWheelEvent    -> wheelEvent
  +-- QKeyEvent      -> keyPressEvent
  +-- QResizeEvent   -> resizeEvent
  +-- QShowEvent     -> showEvent
```

### 11.2 Eventos de Tablet

`tabletEvent(QTabletEvent* ev)` (`canvas_widget.cpp:638`):

```cpp
void CanvasWidget::tabletEvent(QTabletEvent* ev) {
    ev->accept();
    tabletActive_ = true;
    tabletPressure_ = static_cast<float>(ev->pressure());
    tabletEraser_ = (ev->pointerType() == QPointingDevice::PointerType::Eraser);
    switch (ev->type()) {
    case QEvent::TabletPress: inputPress(ev->position()); break;
    case QEvent::TabletMove:
        if (drawing_ || panning_ || moving_ || tool_ == 2 || selState_ != SelectionState::None)
            inputMove(ev->position());
        cursorPos_ = ev->position();
        break;
    case QEvent::TabletRelease:
        if (drawing_ || moving_ || tool_ == 2 || selState_ != SelectionState::None)
            inputRelease();
        tabletActive_ = false;
        break;
    default: break;
    }
    update();
}
```

**Logica de precedencia Tablet vs Mouse:**
- Cuando la tablet genera eventos, `tabletActive_ = true`.
- `mousePressEvent` verifica `if (tabletActive_ && !drawing_) { tabletActive_ = false; }`. Si la tablet esta activa pero no se esta dibujando (posible falso positivo), resetea el flag.
- `if (tabletActive_) return;` en `mousePressEvent` y `mouseMoveEvent`: si la tablet esta activa, los eventos de mouse se ignoran completamente. Esto evita doble procesamiento (la tablet genera eventos de mouse sinteticos en algunos sistemas).

**IMPORTANTE:** `tabletEraser_` se detecta pero NO se usa actualmente en el codigo. Seria el mecanismo para cambiar automaticamente a la herramienta Eraser cuando el usuario gira el lapiz.

### 11.3 Eventos de Mouse

`mousePressEvent(QMouseEvent* ev)` (`canvas_widget.cpp:652`):
```cpp
if (tabletActive_ && !drawing_) { tabletActive_ = false; }
if (tabletActive_) return;  // Ignorar si tablet activa
cursorPos_ = ev->position();

// Pan con boton central o Hand tool
if (ev->button() == Qt::MiddleButton || (tool_ == 10 && ev->button() == Qt::LeftButton)) {
    panning_ = true; panStart_ = ev->position(); panOffStart_ = QPointF(offX_, offY_);
    setCursor(Qt::ClosedHandCursor); return;
}

// Dibujo con boton izquierdo
if (ev->button() == Qt::LeftButton) inputPress(ev->position());
```

`mouseMoveEvent(QMouseEvent* ev)` (`canvas_widget.cpp:663`):
```cpp
if (tabletActive_ && !drawing_) { tabletActive_ = false; }
if (tabletActive_) return;
cursorPos_ = ev->position();
if (panning_) { /* actualizar offX_, offY_ */ return; }
if (drawing_ || moving_ || tool_ == 2 || selState_ != SelectionState::None)
    inputMove(ev->position());
update();
```

`mouseReleaseEvent(QMouseEvent* ev)` (`canvas_widget.cpp:672`):
```cpp
if (tabletActive_) return;
cursorPos_ = ev->position();
if (panning_) { panning_ = false; /* restaurar cursor */ return; }
if (drawing_ || moving_ || tool_ == 2 || selState_ != SelectionState::None)
    inputRelease();
```

### 11.4 Despacho Centralizado: inputPress / inputMove / inputRelease

Estas tres funciones (`canvas_widget.cpp:721-986`) son el corazon del sistema de herramientas. Reciben coordenadas en espacio SCREEN y las convierten a canvas mediante `screenToCanvas()`.

**Estructura de inputPress:**
```
1. Si tool==10 (Hand) -> return (el pan se maneja en mousePressEvent)
2. Si tool==2 (PickColor) -> doPickColor(cpos), return
3. Si (Brush o Eraser) y floatingActive_: commitFloatingSelection() primero
4. Resolver capa activa (desde LayerPanel o resolveCurrentLayer)
5. Si no hay capa activa: emit statusMessage, return
6. Si capa bloqueada: emit statusMessage, return
7. Switch por tool_:
   - 3 (Fill): doFill(cpos)
   - 4 (Text): doText(cpos)
   - 8 (Move): startMove(cpos)
   - 9 (Select): hit-test handles/selRect -> DraggingHandle/MovingContent/Creating
   - 11 (Transform): hit-test handles/centro -> configurar activeHandle_
   - 5,6,7 (Formas): shapeStart_=cpos, shapeEnd_=cpos, drawing_=true
   - 0,1 (Brush/Eraser)(default): iniciar trazo, acumular primer StrokePoint
```

**Estructura de inputMove:**
```
1. Si tool==2 (PickColor) -> doPickColor(cpos), return
2. Switch por selState_:
   - Creating: actualizar selRect_
   - MovingContent: trasladar selRect_
   - DraggingHandle: redimensionar selRect_
3. Si tool==11 y activeHandle_>=0: actualizar scaleX_/scaleY_ o translate_
4. Si moving_: updateMove(cpos)
5. Si tool en [5,6,7] y drawing_: actualizar shapeEnd_
6. Si drawing_ (Brush/Eraser): suavizado, acumular StrokePoint
```

**Estructura de inputRelease:**
```
1. Si tool==2: emitir colorCommittedToRecent, return
2. Switch por selState_:
   - Creating: si selRect_ >= 3x3 -> commitSelection(), si no -> descartar
   - MovingContent: selState_=None
   - DraggingHandle: selState_=None
3. Si tool==11 y activeHandle_>=0: activeHandle_=-1
4. Si moving_: commitMove()
5. Si tool en [5,6,7] y drawing_: drawShape()
6. Si tool==1 (Eraser): doErase()
7. Si brush_ y strokePoints_ no vacio: commitStroke()
8. Limpiar strokePoints_, tabletActive_=false, emit canvasUpdated()
```

### 11.5 Eventos de Teclado

`keyPressEvent(QKeyEvent* ev)` (`canvas_widget.cpp:690`):

Mapa completo de atajos procesados (en orden de evaluacion):

| Atajo | Accion |
|-------|--------|
| `Ctrl+Z` | `undo()` |
| `Ctrl+Y` | `redo()` |
| `Ctrl+0` | Reset zoom/offset/rotacion + `fit()` |
| `Ctrl+C` | `copySelection()` |
| `Ctrl+X` | `cutSelection()` |
| `Ctrl+V` | `pasteClipboard()` |
| `Enter` | `commitTransform()` (si tool=11) o `commitFloatingSelection()` (si tool=9) |
| `W` | Cambiar a Transform (11) |
| `B` | Cambiar a Brush (0) |
| `E` | Cambiar a Eraser (1) |
| `I` | Cambiar a PickColor (2) |
| `G` | Cambiar a Fill (3) |
| `T` | Cambiar a Text (4) |
| `L` | Cambiar a Line (5) |
| `U` | Cambiar a Rectangle (6) |
| `Y` | Cambiar a Ellipse (7) |
| `M` | Cambiar a Move (8) |
| `S` | Cambiar a Select (9) |
| `H` | Cambiar a Hand (10) |
| `Ctrl+H` | Toggle flip horizontal |
| `R` | Rotar +15 grados |
| `'` (apostrofe) | Toggle grid |
| `Flecha Izquierda` | Frame anterior |
| `Flecha Derecha` | Frame siguiente |
| `F` | `fit()` |
| `Espacio` | Toggle play/pause |
| `Delete / Backspace` | `deleteSelection()` |

**IMPORTANTE: Conflicto de teclas -- `Ctrl+Y` vs `Y` (Ellipse):**
La linea 693 evalua `Ctrl+Y` como `redo()`, pero la linea 707 evalua `Y` (sin Ctrl) como Ellipse. El chequeo de `Ctrl+Y` (linea 693) ocurre antes que el de `Y` (linea 707), por lo que `Ctrl+Y` correctamente hace redo y `Y` solo cambia a Ellipse.

**IMPORTANTE: Conflicto `Ctrl+S` no implementado:**
La linea 709 evalua `S` (sin Ctrl) como Select. Pero `Ctrl+S` no esta implementado. En muchas aplicaciones, `Ctrl+S` es "Guardar". Actualmente, si el usuario presiona `Ctrl+S`, la condicion `k == Qt::Key_S && !ctrl` (linea 709) es falsa, por lo que el evento se ignora (no se guarda, no se selecciona).

---

## 12. Motor de Pincel (BrushEngine)

### 12.1 Estructura del Motor

`BrushEngine` (`src/engine/brush/brush_engine.hpp:48`) es una clase independiente de Qt que gestiona presets de pincel y el estado de un trazo en curso.

**Miembros:**
```cpp
BrushPreset current_preset_;               // Preset activo
std::vector<BrushPreset> presets_;         // Coleccion de presets cargados
std::vector<StrokePoint> stroke_points_;   // Puntos acumulados del trazo actual
bool stroking_ = false;                    // TRUE durante un trazo
```

### 12.2 BrushPreset (Configuracion de Pincel)

```cpp
struct BrushPreset {
    std::string name;
    BrushMode mode = BrushMode::Raster;    // Raster, Vector, o Hybrid
    BrushTip tip;                          // Punta del pincel
    BrushDynamics dynamics;                // Dinamicas (presion, tamano, etc.)
    Color color = {0,0,0,1};
    float size = 10.0f;
    float opacity = 1.0f;
    float flow = 0.3f;
    BlendMode blend_mode = BlendMode::Normal;
    bool use_paper_texture = false;
    std::string paper_texture_path;
};
```

### 12.3 BrushDynamics (Comportamiento Dinamico)

```cpp
struct BrushDynamics {
    bool pressure_opacity = true;     // Opacidad varia con presion
    bool pressure_size = true;        // Grosor varia con presion
    bool pressure_flow = true;        // Flujo varia con presion
    bool tilt_angle = false;          // Angulo varia con inclinacion (no implementado)
    float min_size = 0.1f;            // Factor minimo de tamano (relativo a size)
    float max_size = 2.0f;            // Factor maximo de tamano
    float min_opacity = 0.1f;         // Opacidad minima
    float max_opacity = 1.0f;         // Opacidad maxima
    float min_flow = 0.05f;
    float max_flow = 0.3f;
    float smoothing = 0.5f;           // Suavizado [0.0, 1.0]
};
```

### 12.4 Integracion con CanvasWidget

El `BrushEngine` se usa exclusivamente como almacen de configuracion y procesador de puntos de trazo. El `CanvasWidget`:

1. Consulta `brush_->preset().size`, `brush_->preset().opacity`, `brush_->preset().color` para configurar el dibujo.
2. Consulta `brush_->preset().dynamics.pressure_size` y `pressure_opacity` para determinar como varian grosor/opacidad por presion.
3. Consulta `brush_->preset().dynamics.smoothing` para el estabilizador.
4. Llama a `brush_->beginStroke(color, size)` al iniciar un trazo.
5. Llama a `brush_->addPoint(pt)` por cada punto del trazo.
6. Llama a `brush_->endStroke()` al finalizar.

**IMPORTANTE:** El `BrushEngine` NO realiza el renderizado de trazos. Solo almacena los puntos y la configuracion. El renderizado real lo hace `CanvasWidget::renderVectorStroke()` usando `QPainter`.

---

## 13. Sistema de Modos de Fusion (Blend Modes)

### 13.1 Enumeracion y Conversion a Qt

```cpp
enum class BlendMode {
    Normal, Multiply, Screen, Overlay, Add, Subtract,
    Darken, Lighten, ColorBurn, ColorDodge, SoftLight, HardLight
};
```

Conversion a `QPainter::CompositionMode` (`canvas_widget.cpp:29-45`):

| BlendMode | QPainter::CompositionMode | Formula |
|-----------|--------------------------|---------|
| Normal | `CompositionMode_SourceOver` | src over dst (alpha blending estandar) |
| Multiply | `CompositionMode_Multiply` | src * dst |
| Screen | `CompositionMode_Screen` | 1 - (1-src)*(1-dst) |
| Overlay | `CompositionMode_Overlay` | HardLight con src/dst intercambiados |
| Add | `CompositionMode_Plus` | src + dst (aditivo) |
| Subtract | `CompositionMode_Exclusion` | dst - src (sustractivo) |
| Darken | `CompositionMode_Darken` | min(src, dst) |
| Lighten | `CompositionMode_Lighten` | max(src, dst) |
| ColorBurn | `CompositionMode_ColorBurn` | oscurece por quemado de color |
| ColorDodge | `CompositionMode_ColorDodge` | aclara por sobreexposicion |
| SoftLight | `CompositionMode_SoftLight` | luz suave |
| HardLight | `CompositionMode_HardLight` | luz dura |

### 13.2 Implementacion Software (Engine Layer)

`blend_utils.hpp` (`src/engine/common/blend_utils.hpp:1-77`) contiene implementaciones de software de los modos de fusion para el motor de capas (independiente de Qt).

```cpp
FloatPixel unpackPixel(uint32_t p);     // uint32_t -> float RGBA
uint32_t packPixel(const FloatPixel&);   // float RGBA -> uint32_t
float blendChannel(float src, float dst, BlendMode mode);  // Formula de blend
void blendPixel(uint32_t* pixels, int w, int h, int x, int y,
                float r, float g, float b, float a, BlendMode mode);
```

**IMPORTANTE:** Estas funciones del engine layer NO se usan en el renderizado del canvas (que usa `QPainter` y sus `CompositionMode` nativos). Estan disponibles para operaciones de procesamiento de imagen en el motor que no dependan de Qt.

---

## 14. Sistema de Epochs y Cacheo de Buffers

### 14.1 Proposito

El sistema de epochs evita recrear `QImage` wrappers sobre el buffer de `RasterLayer` cuando el buffer no ha cambiado. En `paintEvent`, que se llama 60+ veces por segundo, crear un `QImage` nuevo en cada frame seria costoso.

### 14.2 Mecanismo

```cpp
// canvas_widget.hpp:218
std::map<LayerUid, uint64_t> rasterEpochs_;
```

Cada `RasterLayer` tiene un contador `buffer_epoch_` que se incrementa con `bufferEpochTick()` cada vez que el buffer es modificado.

En `wrapRasterLayer()`:
```cpp
uint64_t epoch = rasterLayer->bufferEpoch();
LayerUid uid = rasterLayer->uid();
auto it = rasterEpochs_.find(uid);
if (it != rasterEpochs_.end() && it->second != epoch) {
    it->second = epoch;  // Epoch cambio: actualizar cache
} else if (it == rasterEpochs_.end()) {
    rasterEpochs_[uid] = epoch;  // Primera vez: registrar
}
// ... crear QImage wrapper ...
```

El `QImage` se recrea siempre (el wrapper es ligero, solo guarda un puntero). El proposito del epoch no es evitar la creacion del wrapper, sino proporcionar un mecanismo de invalidation para futuro cacheo mas agresivo.

### 14.3 Donde se Incrementa el Epoch

`bufferEpochTick()` se llama en:
- `PaintCommand::undo()` y `redo()` (`undo_commands.cpp:96, 107`): Cuando se restaura un estado anterior de la capa.
- `MoveCommand::undo()` y `redo()` (`undo_commands.cpp:169, 209`): Cuando se mueve contenido.

**IMPORTANTE:** Las funciones que modifican el buffer directamente (`writeLayerPixels`, `writeRasterRect`) NO llaman a `bufferEpochTick()`. Esto es una inconsistencia: el epoch solo se actualiza via comandos de undo/redo, no via operaciones directas. Si se usa `wrapRasterLayer` despues de una operacion que no paso por la pila de undo, el wrapper podria devolver datos stale.

---

## 15. Operaciones de Portapapeles y Atajos de Teclado

### 15.1 copySelection() -- `canvas_widget.cpp:1482`

Prioridad de fuentes para la copia:
1. `floatingSelection_` (si `floatingActive_` y no nula): copia la seleccion flotante activa.
2. `selImage_` (si no nula): copia el snapshot del composite en el area de seleccion.
3. `selRect_` valido: lee la capa activa, copia la region del `selRect_`.

El resultado se coloca en `QApplication::clipboard()->setImage()`.

### 15.2 cutSelection() -- `canvas_widget.cpp:1505`

1. Llama a `copySelection()` para copiar al portapapeles.
2. Si habia `floatingActive_`: empuja undo (snapshot antes del borrado), limpia el estado flotante.
3. Si habia `selRect_` valido sin floating: usa `CompositionMode_DestinationOut` para borrar la region de la capa activa, empuja undo, limpia el rectangulo de seleccion.

**IMPORTANTE:** El "Cut" de una seleccion flotante NO usa `CompositionMode_Clear` como `commitSelection()`, porque la seleccion flotante ya fue extraida de la capa en el momento del cut inicial. Simplemente descarta la seleccion flotante y registra el undo (aunque la capa no se modifica porque ya fue modificada en el cut inicial... esto parece ser un bug o redundancia).

### 15.3 pasteClipboard() -- `canvas_widget.cpp:1554`

1. Obtiene `QApplication::clipboard()->image()`.
2. Si hay seleccion flotante activa, la compromete primero.
3. Convierte la imagen del portapapeles a `Format_ARGB32_Premultiplied`.
4. Establece `floatingSelection_` y `originalFloatingSelection_` con la imagen pegada.
5. Centra el `selRect_` en el viewport actual.
6. Activa `floatingActive_ = true`.
7. Cambia a herramienta Select (9) y emite `toolChangedByKey(9)`.

### 15.4 deleteSelection() -- `canvas_widget.cpp:1578`

1. Si `floatingActive_`: empuja undo (snapshot antes de borrar), limpia estado flotante.
2. Si `selRect_` valido sin floating: usa `CompositionMode_DestinationOut` para borrar la region de la capa, empuja undo, limpia seleccion.

**Diferencia con cutSelection():** `deleteSelection()` NO copia al portapapeles antes de borrar.

---

## 16. Conexion entre Paneles UI y Canvas

### 16.1 ToolboxPanel -> CanvasWidget

`ToolboxPanel` (`src/ui/toolbox_panel.hpp:28`) emite:
- `toolChanged(int toolIndex)`: Conectado a `canvas_->setTool()` en `MainWindow`.
- `colorChanged(const QColor&)`: Conectado a `canvas_->setColor()`.
- `settingsChanged()`: Actualiza parametros de pincel (via `BrushEngine`).
- `canvasResized(int, int)`: Conectado a `canvas_->resizeCanvas()`.

`CanvasWidget` emite:
- `toolChangedByKey(int tool)`: Conectado a `toolboxPanel_->setActiveTool()` para sincronizar la UI cuando el usuario cambia de herramienta por teclado.
- `colorPicked(QColor)`: Conectado a `toolboxPanel_->setColor()` para actualizar el color en la UI cuando se usa el cuentagotas.

### 16.2 LayerPanel -> CanvasWidget

`CanvasWidget` tiene un puntero a `LayerPanel` (`layerPanel_`). En `inputPress`, consulta `layerPanel_->currentLayer()` para determinar la capa activa. La comunicacion inversa (cambios en el panel que afectan al canvas) se maneja mediante senales de Qt conectadas en `MainWindow`.

### 16.3 MainWindow -> CanvasWidget

`MainWindow` (`src/ui/main_window.cpp`) es el orquestador central que conecta todos los paneles. Las conexiones relevantes incluyen:
- `timelinePanel_->frameChanged(int)` -> `canvas_->setCurrentFrame(int)`
- `timelinePanel_->totalFramesChanged(int)` -> `canvas_->setTotalFrames(int)`
- `layerPanel_->layerChanged(int)` -> `canvas_->setCurrentLayer(int)`
- Senales de onion skin, grid, flip, rotacion desde menus/botones al canvas.

---

## 17. Apendices

### Apendice A: Mapa de Herramientas (tool_ index)

| Indice | Herramienta | Tecla | Tipo de operacion | Usa capa | Genera Undo |
|--------|-------------|-------|-------------------|----------|-------------|
| 0  | Brush        | B | Dibujo a mano alzada | Si | Si (PaintCommand) |
| 1  | Eraser       | E | Borrado a mano alzada | Si | Si (PaintCommand) |
| 2  | PickColor    | I | Seleccion de color | No (lee composite) | No |
| 3  | Fill         | G | Relleno flood-fill | Si | Si (PaintCommand) |
| 4  | Text         | T | Texto | Si | Si (PaintCommand) |
| 5  | Line         | L | Linea recta | Si | Si (PaintCommand) |
| 6  | Rectangle    | U | Rectangulo | Si | Si (PaintCommand) |
| 7  | Ellipse      | Y | Elipse | Si | Si (PaintCommand) |
| 8  | Move         | M | Desplazar capa/seleccion | Si | Si (MoveCommand) |
| 9  | Select       | S | Seleccion rectangular + flotante | Si | Si (PaintCommand en cut/commit) |
| 10 | Hand         | H | Pan | No | No |
| 11 | Transform    | W | Escala/traslacion libre | Si | Si (PaintCommand) |

### Apendice B: Archivos Clave del Modulo

| Archivo | Lineas | Proposito |
|---------|--------|-----------|
| `src/ui/canvas_widget.hpp` | 221 | Declaracion de CanvasWidget, todas las variables de estado, RawStroke |
| `src/ui/canvas_widget.cpp` | 1838 | Implementacion completa: render, eventos, 11 herramientas, seleccion, undo |
| `src/ui/undo_commands.hpp` | 138 | Declaracion de 6 comandos QUndoCommand (Paint, Move, Delete, Rename, ToggleVis, BlendMode) |
| `src/ui/undo_commands.cpp` | 313 | Implementacion de undo/redo con raster rect read/write helpers |
| `src/ui/main_window.hpp` | 68 | Declaracion de MainWindow |
| `src/ui/main_window.cpp` | 763 | Orquestador: conexion de senales entre paneles y canvas |
| `src/ui/toolbox_panel.hpp` | 70 | Panel de herramientas (11 botones, color, parametros de pincel) |
| `src/ui/toolbox_panel.cpp` | 283 | Implementacion del panel de herramientas |
| `src/ui/layer_panel.hpp` | 51 | Panel de capas (lista, visibilidad, nombre, bloqueo, blend mode) |
| `src/ui/layer_panel.cpp` | 497 | Implementacion del panel de capas con drag-and-drop |
| `src/ui/timeline_panel.hpp` | 88 | Panel de linea de tiempo (thumbnails, navegacion, playback) |
| `src/core/types.hpp` | 199 | Tipos fundamentales: Color, Vec2, Rect, StrokePoint, BlendMode, etc. |
| `src/core/layer.hpp` | 124 | Modelo de capas: Layer, RasterLayer, VectorLayer, GroupLayer |
| `src/core/document.hpp` | 58 | Documento: tamano canvas, FPS, frames (map<int, GroupLayer>), UndoManager |
| `src/core/undo_manager.hpp` | 43 | Motor de undo engine-level (128 entradas, independiente de Qt) |
| `src/engine/brush/brush_engine.hpp` | 73 | Motor de pincel: presets, dinamicas, stroke points |
| `src/engine/vector/bezier_path.hpp` | 65 | Trayectorias Bezier, VectorStroke, flattening |
| `src/engine/common/blend_utils.hpp` | 77 | Modos de fusion por software (unpack, blendChannel, pack) |
| `src/engine/raster/raster_engine.hpp` | 29 | Motor raster simple (buffer width/height/pixels) |

### Apendice C: Glosario de Modos de Composicion Qt Usados

| Modo Qt | Efecto | Donde se usa |
|---------|--------|-------------|
| `CompositionMode_SourceOver` | Alpha blending normal (src sobre dst) | Renderizado de capas, stamp de seleccion flotante, brush strokes |
| `CompositionMode_Clear` | Pone todos los canales a 0 (transparente) | Cut inicial de seleccion (`commitSelection`) |
| `CompositionMode_DestinationOut` | Reduce alpha del destino segun alpha de la fuente | Goma (`doErase`), renderizado de trazos eraser, MoveCommand (borrar region) |
| `CompositionMode_DestinationIn` | Reduce alpha del destino al alpha de la fuente | Onion skin (aplicar tinte) |
| `CompositionMode_Multiply` | Multiplica src y dst | Blend mode Multiply de capas |
| `CompositionMode_Screen` | Inverso de Multiply | Blend mode Screen de capas |
| `CompositionMode_Overlay` | Combinacion de Multiply y Screen | Blend mode Overlay de capas |
| `CompositionMode_Plus` | Suma aditiva de canales | Blend mode Add de capas |
| `CompositionMode_Exclusion` | Diferencia | Blend mode Subtract de capas |

### Apendice D: Estados de la Maquina de Seleccion Flotante

```
ESTADO: Sin seleccion
  floatingActive_ = false
  selImage_ = QImage() (nulo)
  selRect_ = QRectF() (nulo)
  floatingSelection_ = QImage() (nulo)
  originalFloatingSelection_ = QImage() (nulo)
  selState_ = None

CREAR SELECCION (tool=9, arrastrar en area vacia, soltar con >= 3x3 px)
  -> commitSelection()
  -> Estado: Seleccion flotante activa

ESTADO: Seleccion flotante activa
  floatingActive_ = true
  selImage_ = composite.copy(selRect_)     [snapshot del composite]
  selRect_ = rect del usuario
  floatingSelection_ = layerPixels.copy()  [pixeles de la capa activa]
  originalFloatingSelection_ = floatingSelection_ [copia inmutable]
  selState_ = None
  Acciones posibles:
    - Mover (arrastrar dentro) -> MovingContent -> traslada selRect_
    - Redimensionar (arrastrar handle) -> DraggingHandle -> modifica selRect_
    - Enter -> commitFloatingSelection() -> estampa a capa -> vuelve a Sin seleccion
    - Cambiar a otra herramienta -> commit condicional -> vuelve a Sin seleccion

ESTADO: Seleccion no flotante (rectangulo dashed, sin marching ants)
  floatingActive_ = false
  selImage_ != null (de un commitSelection previo que fallo? o de cutSelection?)
  selRect_ valido
  selState_ = None
  Acciones posibles:
    - Copy/Cut/Delete operan sobre el contenido del selRect_
    - Move tool (8) puede mover la region
```

### Apendice E: Diagrama de Flujo de Datos en una Operacion de Dibujo

```
Usuario presiona y arrastra con Brush
  |
  v
mousePressEvent / tabletEvent(TabletPress)
  |
  v
inputPress(screenPos)
  |-- Convierte screen -> canvas (screenToCanvas)
  |-- Resuelve capa activa (layerPanel_ o resolveCurrentLayer)
  |-- Verifica capa no bloqueada
  |-- Si hay floatingActive_: commitFloatingSelection() primero
  |-- brush_->beginStroke(color, size)
  |-- strokePoints_.push_back(primer punto)
  |-- drawing_ = true
  |
  v
[Usuario mueve el mouse/lapiz]
  |
  v
mouseMoveEvent / tabletEvent(TabletMove)
  |
  v
inputMove(screenPos)
  |-- Convierte a canvas
  |-- Aplica suavizado (stabilizer buffer)
  |-- Filtra puntos muy cercanos (< 0.5 px)
  |-- strokePoints_.push_back(nuevo punto)
  |-- brush_->addPoint(nuevo punto)
  |-- update() -> paintEvent -> paintLiveStroke() muestra preview
  |
  v
[Usuario suelta]
  |
  v
mouseReleaseEvent / tabletEvent(TabletRelease)
  |
  v
inputRelease()
  |-- drawing_ = false
  |-- brush_->endStroke()
  |-- commitStroke()
  |     |-- Calcula dirtyRect (bounds de strokePoints + padding)
  |     |-- readRasterRect() -> beforePixels (solo la region afectada)
  |     |-- Asegura que la capa contenga el dirtyRect (ensureContains)
  |     |-- Crea RawStroke con parametros y lo agrega a vectorStrokes_
  |     |-- readLayerPixels() -> imagen completa de la capa
  |     |-- renderVectorStroke() sobre la imagen
  |     |-- writeLayerPixels() vuelca a buffer raw
  |     |-- readRasterRect() -> afterPixels
  |     |-- undoStack_.push(new PaintCommand(before, after, dirtyRect, ...))
  |-- strokePoints_.clear()
  |-- emit canvasUpdated()
```

---

*Documento generado el 2026-06-19. Version 2.0 (completa).*

*Proyecto: Free Animation Power -- Motor de animacion 2D con arquitectura hibrida raster/vectorial.*

*Contacto para dudas tecnicas: revisar AGENTS.md en la raiz del proyecto.*
