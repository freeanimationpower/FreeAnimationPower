# Report: Efecto Acetato + Timeline NLE Compacto

**Fecha**: 2026-07-03

---

## 1. Renderizado Multi-Sequence (Efecto Acetato)

### Problema
`buildBackgroundCache` solo componia capas de la secuencia activa. Las demas secuencias eran invisibles en el canvas.

### Solucion
Envolver el bucle de composicion de capas con un bucle exterior que itera todas las secuencias del documento.

### Archivo: `src/ui_v2/canvas_widget_v2.cpp`

Tres ubicaciones modificadas con el mismo patron:

```cpp
// Antes (solo secuencia activa):
auto& root = doc.rootLayerForFrame(currentFrame_);
for (size_t li = ...) { /* compositar capas */ }

// Ahora (todas las secuencias, bottom-to-top):
for (size_t si = 0; si < doc.sequenceCount(); ++si) {
    if (!doc.sequenceAt(si).visible()) continue;
    const auto& root = doc.sequenceAt(si).rootLayerForFrame(currentFrame_);
    for (size_t li = 0; li < root.layerCount(); ++li) {
        const Layer* layer = root.layerAt(li);
        if (!layer || !layer->visible() || layer->type() != LayerType::Raster) continue;
        // compositar con SourceOver + opacity + blendMode
    }
}
```

#### Ubicaciones exactas

| Metodo | Tipo de rebuild |
|---|---|
| `buildBackgroundCache` (full) | Cache completa: itera todas las secuencias |
| `buildBackgroundCache` (partial) | Dirty rect: offset-aware, solo capas que intersectan el rect |
| `render()` | Exportacion: mismo orden bottom-to-top |

#### Orden de Composicion
Secuencia `[0]` = fondo, secuencia `[N]` = capa superior. `QPainter::CompositionMode_SourceOver` por defecto.

#### Rendimiento
Sin impacto en la latencia de dibujo:
- `strokeBuffer_` se compone solo contra `activeRasterLayer()` en `commitStroke()`
- El multi-sequence overlay se hornea en `backgroundCache_` (cache valida hasta cambio de frame, secuencia o propiedad de capa)
- La frecuencia de `buildBackgroundCache` no cambia

#### Onion Skin
Se mantiene aplicado solo a la secuencia activa (es una herramienta de edicion, no de composicion final).

---

## 2. Visibilidad de Secuencia

### Archivo: `src/core/sequence.hpp`

```cpp
// Añadido a la clase Sequence:
bool visible_ = true;
bool visible() const { return visible_; }
void setVisible(bool v) { visible_ = v; }
```

### Archivo: `src/core/sequence.cpp`

```cpp
// En clone():
seq->visible_ = visible_;  // preserva visibilidad en copia
```

### Uso en Canvas
El bucle de renderizado verifica `doc.sequenceAt(si).visible()` antes de componer. Secuencias ocultas se saltan completamente.

---

## 3. Timeline Compacto NLE

### Archivos: `src/ui_v2/timeline_panel_v2.hpp`, `src/ui_v2/timeline_panel_v2.cpp`

#### Constantes Redisenadas

| Constante | Antes | Ahora | Razon |
|---|---|---|---|
| `kHeaderWidth` | 76px | 120px | Mas espacio para nombre + botones |
| `kTrackHeight` | 30px | 28px | Mas tracks visibles en menos espacio |
| `kCellWidth` | 36px | 32px | Grid mas denso |
| `kCellSpacing` | 2px | 1px | Continuidad visual |
| `kRulerHeight` | 20px | 18px | Proporcional al resto |

#### SequenceTrackWidget V3

**Header (120px):**
- QLineEdit **permanente** (no requiere double-click)
  - Estilo: `background:transparent; color:#8B8FA3/#FF6B4A; border:none; font-size:8px`
  - Senales: `editingFinished` + `returnPressed` → `panel_->onRenameTrack()`
- Barra acento activa: `2px solid #FF6B4A` en el borde izquierdo
- Botones dup/del: solo visibles en hover (Unicode ⚌ y ✕)

**Body (celdas):**
- Tamano celda: 32x24px (dentro de track 28px, padding 3px arriba)
- Estados visuales:
  - Celda vacia: `#161920` (sin contenido)
  - Celda con contenido: `#2E333D` (tiene layers)
  - Celda activa (frame actual): borde acento `#FF6B4A`
  - Celda activa + contenido: `#3A2018` (tono acento oscuro)
- Borde: `#1E2128`, 1px
- Numero de frame cada 5 celdas (fuente JetBrains Mono 5px)
- Boton "+" al final para añadir frames

**Playhead:**
- Linea vertical `1px #FF6B4A` solo en la pista activa
- RulerWidget dibuja el triangulo ▼ en la regla superior

**Colores del tema oscuro:**
```
kTrackBg       = #121418   (fondo pista inactiva)
kTrackActiveBg = #191D26   (fondo pista activa)
kTrackHoverBg  = #161A22   (hover sobre pista)
kHeaderBg      = #0D1014   (fondo header inactivo)
kHeaderActiveBg= #11131B   (fondo header activo)
kBorderColor   = #1E2128   (lineas divisorias)
kAccentColor   = #FF6B4A   (acento naranja)
```

#### frameHasContent Corregido

**Bug**: El metodo usaba `activeSequence()` para verificar contenido de frames en TODOS los tracks, mostrando incorrectamente los mismos datos en todas las pistas.

**Fix**:
```cpp
// Antes (bug):
const GroupLayer* root = appState_->document().activeSequence().peekRootLayerForFrame(frame);

// Ahora:
const auto* seq = &appState_->document().sequenceAt(static_cast<size_t>(seqIndex_));
const GroupLayer* root = seq->peekRootLayerForFrame(frame);
```

#### Sincronizacion de Scroll Horizontal
- `QScrollBar` padre → `sharedScrollOffset_` → `update()` en RulerWidget + todos los SequenceTrackWidgets
- Cada track en su `paintEvent` calcula `x = headerWidth + (frame * cellTotal) - sharedScrollOffset`
- Sin bucles de retroalimentacion de eventos

---

## 4. Use-After-Free Fix

### Problema
La app se cerraba al interactuar con las pistas. Causa raiz: `SequenceTrackWidget::mousePressEvent` llamaba a `panel_->onActivateTrack()` que emitia `sequenceChanged`, y `MainWindowV2` respondia con `rebuildTracks()` que borraba el widget que estaba ejecutando el evento.

### Soluciones (3 niveles de defensa)

**Nivel 1 — Activar track no borra widgets:**
```cpp
// main_window_v2.cpp — sequenceChanged ya NO llama rebuildTracks:
connect(timeline_panel_, &TimelinePanelV2::sequenceChanged, [this](int) {
    updateUIState();  // solo botones undo/redo
});
```
Los tracks se actualizan inline en `onActivateTrack` via `widget->setActive(bool)`.

**Nivel 2 — Operaciones estructurales diferidas:**
```cpp
// timeline_panel_v2.cpp — dup/del/new usan QTimer::singleShot:
void TimelinePanelV2::onDupTrack(int seqIndex) {
    appState_->duplicateSequence(seqIndex);
    QTimer::singleShot(0, this, [this]() { rebuildTracks(); });
}
```
El `rebuildTracks()` se ejecuta en el siguiente ciclo del event loop, cuando el widget invocante ya retorno de `mousePressEvent`.

**Nivel 3 — Proteccion de QLineEdit:**
```cpp
void TimelinePanelV2::rebuildTracks() {
    QWidget* focused = QApplication::focusWidget();
    if (focused && tracksContainer_->isAncestorOf(focused)) {
        focused->clearFocus();  // previene editingFinished en widget borrado
    }
    // ... borrar widgets ...
}
```

---

## 5. Resultado Final

- **Build**: 0 errores, 0 warnings
- **Tests**: 150/150 pasan
- **App**: estable, sin crashes al interactuar con pistas
- **Canvas**: renderiza todas las secuencias simultaneamente como acetatos
- **Timeline**: layout compacto profesional estilo NLE
- **Core**: intacto, sin regresiones
