"""
Free Animation Power v0.2 — Prototype funcional
Motor hibrido vector + raster para animacion 2D
Soporte Wacom/tableta + mouse | PySide6 + QPainter
"""

import sys
import os
from enum import IntEnum
from dataclasses import dataclass, field
from collections import deque

from PySide6.QtCore import (
    Qt, QPointF, QRectF, QSize, QTimer, Signal, QLineF, QPoint, QMimeData
)
from PySide6.QtGui import (
    QImage, QPainter, QPen, QBrush, QColor, QPixmap,
    QTabletEvent, QMouseEvent, QWheelEvent, QKeyEvent,
    QPainterPath, QFont, QAction, QTransform,
    QLinearGradient, QRadialGradient, QConicalGradient,
    QCursor, QDrag, QIcon
)
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QSplitter, QPushButton, QSlider, QLabel, QSpinBox,
    QStatusBar, QMenuBar, QMenu, QFileDialog, QMessageBox,
    QScrollArea, QFrame, QSizePolicy, QComboBox, QCheckBox,
    QColorDialog, QButtonGroup, QToolButton, QGridLayout,
    QGroupBox, QDoubleSpinBox, QRadioButton, QToolBar
)

# ═══════════════════════════════════════════════════════
# CONSTANTES
# ═══════════════════════════════════════════════════════

CANVAS_W = 1920
CANVAS_H = 1080
DEFAULT_FPS = 24
MAX_UNDO = 128

# ═══════════════════════════════════════════════════════
# ENUMS
# ═══════════════════════════════════════════════════════

class Tool(IntEnum):
    BRUSH = 0
    ERASER = 1
    EYEDROPPER = 2
    BUCKET = 3
    LINE = 4
    RECT = 5
    ELLIPSE = 6
    MOVE = 7
    SELECT_RECT = 8
    SELECT_LASSO = 9
    HAND = 10

class BrushShape(IntEnum):
    ROUND = 0
    SQUARE = 1
    FLAT = 2
    CALLIGRAPHY = 3

# ═══════════════════════════════════════════════════════
# ESTADO DE TABLETA
# ═══════════════════════════════════════════════════════

@dataclass
class TabletState:
    is_tablet: bool = False
    is_erasing: bool = False
    pressure: float = 1.0
    tilt_x: float = 0.0
    tilt_y: float = 0.0
    rotation: float = 0.0
    pos: QPointF = field(default_factory=QPointF)
    last_pos: QPointF = field(default_factory=QPointF)
    buttons: int = 0

# ═══════════════════════════════════════════════════════
# CONFIGURACION DE PINCEL
# ═══════════════════════════════════════════════════════

@dataclass
class BrushCfg:
    size: float = 12.0
    min_size: float = 2.0
    opacity: float = 1.0
    flow: float = 0.5
    hardness: float = 0.75
    spacing: float = 0.1
    color: QColor = field(default_factory=lambda: QColor(0, 0, 0))
    shape: BrushShape = BrushShape.ROUND
    use_pressure_size: bool = True
    use_pressure_opacity: bool = True
    roundness: float = 1.0     # 1.0 = circle, 0.1 = thin line
    angle: float = 0.0         # degrees for flat/calligraphy
    stabilizer: int = 0        # 0=off, 1-10=smoothing level

# ═══════════════════════════════════════════════════════
# CAPA
# ═══════════════════════════════════════════════════════

@dataclass
class LayerData:
    name: str = "Layer"
    visible: bool = True
    opacity: float = 1.0
    locked: bool = False
    frames: dict = field(default_factory=dict)  # int -> QImage

    def get(self, idx: int, size: QSize) -> QImage:
        if idx not in self.frames or self.frames[idx] is None:
            img = QImage(size, QImage.Format_ARGB32_Premultiplied)
            img.fill(Qt.transparent)
            self.frames[idx] = img
        return self.frames[idx]

# ═══════════════════════════════════════════════════════
# UNDO
# ═══════════════════════════════════════════════════════

class UndoCmd:
    def __init__(self, layer_idx: int, frame_idx: int, before: QImage, after: QImage):
        self.layer_idx = layer_idx
        self.frame_idx = frame_idx
        self.before = QImage(before) if before else None
        self.after = QImage(after) if after else None


class UndoStack:
    def __init__(self, limit=MAX_UNDO):
        self._data: list[UndoCmd] = []
        self._pos = -1
        self._limit = limit

    def push(self, cmd: UndoCmd):
        while len(self._data) > self._pos + 1:
            self._data.pop()
        self._data.append(cmd)
        if len(self._data) > self._limit:
            self._data.pop(0)
        self._pos = len(self._data) - 1

    def undo(self) -> UndoCmd | None:
        if self._pos >= 0:
            c = self._data[self._pos]
            self._pos -= 1
            return c
        return None

    def redo(self) -> UndoCmd | None:
        if self._pos + 1 < len(self._data):
            self._pos += 1
            return self._data[self._pos]
        return None

    def clear(self):
        self._data.clear()
        self._pos = -1

    def can_undo(self) -> bool: return self._pos >= 0
    def can_redo(self) -> bool: return self._pos + 1 < len(self._data)

# ═══════════════════════════════════════════════════════
# DOCUMENTO DE ANIMACION
# ═══════════════════════════════════════════════════════

class Document:
    def __init__(self):
        self.cw = CANVAS_W
        self.ch = CANVAS_H
        self.fps = DEFAULT_FPS
        self.total_frames = 1
        self.current_frame = 0
        self.layers: list[LayerData] = [LayerData("Background")]
        self.current_layer = 0
        self.undo = UndoStack()
        self.bg_color = QColor(255, 255, 255)
        self.onion_prev = 3
        self.onion_next = 1
        self.onion_opacity = 0.35
        self.playing = False
        self.loop = True
        self.grid_size = 0  # 0 = off
        self.show_grid = False

    @property
    def size(self): return QSize(self.cw, self.ch)

    @property
    def cur_layer(self) -> LayerData:
        return self.layers[self.current_layer]

    def cur_image(self) -> QImage:
        return self.cur_layer.get(self.current_frame, self.size)

    def snap(self) -> QImage:
        return QImage(self.cur_image())

    def add_layer(self, name=""):
        idx = len(self.layers)
        self.layers.append(LayerData(name or f"Layer {idx + 1}"))
        self.current_layer = idx

    def remove_layer(self, idx: int):
        if len(self.layers) > 1 and 0 <= idx < len(self.layers):
            del self.layers[idx]
            self.current_layer = min(self.current_layer, len(self.layers) - 1)

    def add_frame(self):
        insert_at = self.current_frame + 1
        # shift frames
        for layer in self.layers:
            for f in sorted(layer.frames.keys(), reverse=True):
                if f >= insert_at:
                    layer.frames[f + 1] = layer.frames[f]
        self.total_frames += 1
        self.current_frame = insert_at

    def dup_frame(self):
        src = self.current_frame
        insert_at = src + 1
        for layer in self.layers:
            for f in sorted(layer.frames.keys(), reverse=True):
                if f >= insert_at:
                    layer.frames[f + 1] = layer.frames[f]
            if src in layer.frames:
                layer.frames[insert_at] = QImage(layer.frames[src])
        self.total_frames += 1
        self.current_frame = insert_at

    def del_frame(self):
        if self.total_frames <= 1:
            return
        df = self.current_frame
        for layer in self.layers:
            if df in layer.frames:
                del layer.frames[df]
            for f in sorted(layer.frames.keys()):
                if f > df:
                    layer.frames[f - 1] = layer.frames[f]
                    del layer.frames[f]
        self.total_frames -= 1
        if self.current_frame >= self.total_frames:
            self.current_frame = self.total_frames - 1

    def clear_frame(self):
        layer = self.cur_layer
        if self.current_frame in layer.frames:
            del layer.frames[self.current_frame]


# ═══════════════════════════════════════════════════════
# MOTOR DE PINCEL - Renderizado vectorial suave
# ═══════════════════════════════════════════════════════

class BrushEngine:
    def __init__(self):
        self.cfg = BrushCfg()

    def create_pen(self, pressure: float) -> QPen:
        """Crea QPen con gradiente radial para trazo suave"""
        cfg = self.cfg

        if not cfg.use_pressure_size:
            pressure = 1.0
        size = cfg.min_size + (cfg.size - cfg.min_size) * pressure
        if size < 0.5:
            size = 0.5

        opacity_factor = 1.0
        if cfg.use_pressure_opacity:
            opacity_factor = max(0.1, pressure)

        alpha = int(cfg.opacity * opacity_factor * 255)
        color = QColor(cfg.color)
        color.setAlpha(min(255, max(0, alpha)))

        pen = QPen(Qt.NoPen)  # We use the brush for the gradient effect

        # Create a gradient "tip" image we can use as pen brush
        # For smooth rendering, we use the QPainterPath with a gradient brush on the pen
        if cfg.hardness >= 0.98 and cfg.shape == BrushShape.ROUND:
            pen = QPen(color, size, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin)
        elif cfg.shape == BrushShape.ROUND:
            # Soft round brush: use a radial gradient as pen brush
            sz = int(size) + 4
            if sz < 4: sz = 4
            if sz % 2 == 0: sz += 1
            brush_img = QImage(sz, sz, QImage.Format_ARGB32_Premultiplied)
            brush_img.fill(Qt.transparent)
            bp = QPainter(brush_img)
            bp.setRenderHint(QPainter.Antialiasing)
            grad = QRadialGradient(sz / 2, sz / 2, size / 2)
            h = cfg.hardness
            c0 = QColor(color); c0.setAlpha(alpha)
            c1 = QColor(color); c1.setAlpha(int(alpha * 0.6))
            c2 = QColor(color); c2.setAlpha(0)
            grad.setColorAt(0.0, c0)
            grad.setColorAt(h, c0)
            grad.setColorAt(h + (1 - h) * 0.4, c1)
            grad.setColorAt(1.0, c2)
            bp.setBrush(QBrush(grad))
            bp.setPen(Qt.NoPen)
            bp.drawEllipse(QPointF(sz / 2, sz / 2), size / 2, size / 2)
            bp.end()
            pen = QPen(QBrush(brush_img), size, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin)
        elif cfg.shape == BrushShape.FLAT:
            sz = int(size) + 4
            if sz < 4: sz = 4
            if sz % 2 == 0: sz += 1
            brush_img = QImage(sz, sz, QImage.Format_ARGB32_Premultiplied)
            brush_img.fill(Qt.transparent)
            bp = QPainter(brush_img)
            bp.setRenderHint(QPainter.Antialiasing)
            bp.translate(sz / 2, sz / 2)
            bp.rotate(cfg.angle)
            bp.scale(1.0, max(0.1, cfg.roundness))
            grad = QRadialGradient(0, 0, size / 2)
            h = cfg.hardness
            c0 = QColor(color); c0.setAlpha(alpha)
            c2 = QColor(color); c2.setAlpha(0)
            grad.setColorAt(0, c0)
            grad.setColorAt(h, c0)
            grad.setColorAt(1, c2)
            bp.setBrush(QBrush(grad))
            bp.setPen(Qt.NoPen)
            bp.drawEllipse(QPointF(0, 0), size / 2, size / 2)
            bp.end()
            pen = QPen(QBrush(brush_img), size, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin)
        elif cfg.shape == BrushShape.SQUARE:
            pen = QPen(color, size, Qt.SolidLine, Qt.SquareCap, Qt.MiterJoin)
        elif cfg.shape == BrushShape.CALLIGRAPHY:
            sz = int(size * 2) + 6
            if sz < 6: sz = 6
            if sz % 2 == 0: sz += 1
            brush_img = QImage(sz, sz, QImage.Format_ARGB32_Premultiplied)
            brush_img.fill(Qt.transparent)
            bp = QPainter(brush_img)
            bp.setRenderHint(QPainter.Antialiasing)
            bp.translate(sz / 2, sz / 2)
            bp.rotate(cfg.angle + 45)
            bp.scale(1.0, 0.25)
            grad = QRadialGradient(0, 0, size / 2)
            h = cfg.hardness
            c0 = QColor(color); c0.setAlpha(alpha)
            c2 = QColor(color); c2.setAlpha(0)
            grad.setColorAt(0, c0)
            grad.setColorAt(h, c0)
            grad.setColorAt(1, c2)
            bp.setBrush(QBrush(grad))
            bp.setPen(Qt.NoPen)
            bp.drawEllipse(QPointF(0, 0), size * 0.7, size * 0.15)
            bp.end()
            pen = QPen(QBrush(brush_img), size, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin)
        else:
            pen = QPen(color, size, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin)

        pen.setWidthF(size)
        return pen

    def create_eraser_pen(self, pressure: float) -> QPen:
        """Eraser pen - renders with clear composition"""
        size = self.cfg.min_size + (self.cfg.size - self.cfg.min_size) * pressure
        if size < 0.5: size = 0.5
        # For eraser, we use a round soft brush that draws transparent
        sz = int(size) + 4
        if sz < 4: sz = 4
        if sz % 2 == 0: sz += 1
        brush_img = QImage(sz, sz, QImage.Format_ARGB32_Premultiplied)
        brush_img.fill(Qt.transparent)
        bp = QPainter(brush_img)
        bp.setRenderHint(QPainter.Antialiasing)
        grad = QRadialGradient(sz / 2, sz / 2, size / 2)
        grad.setColorAt(0, QColor(255, 255, 255, 255))
        grad.setColorAt(0.7, QColor(255, 255, 255, 220))
        grad.setColorAt(0.9, QColor(255, 255, 255, 80))
        grad.setColorAt(1, QColor(255, 255, 255, 0))
        bp.setBrush(QBrush(grad))
        bp.setPen(Qt.NoPen)
        bp.drawEllipse(QPointF(sz / 2, sz / 2), size / 2, size / 2)
        bp.end()
        return QPen(QBrush(brush_img), size, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin)

    def build_path(self, points: list) -> QPainterPath:
        """Construye QPainterPath suave desde puntos"""
        path = QPainterPath()
        if len(points) < 1:
            return path
        if len(points) == 1:
            p = points[0]
            sz = self.cfg.size * p.pressure * 0.5
            path.addEllipse(QPointF(p.x, p.y), max(0.5, sz), max(0.5, sz))
            return path
        if len(points) == 2:
            path.moveTo(points[0].x, points[0].y)
            path.lineTo(points[1].x, points[1].y)
            return path

        # Usar curvas de Catmull-Rom para suavizar
        path.moveTo(points[0].x, points[0].y)
        for i in range(1, len(points) - 1):
            p0 = points[i - 1]
            p1 = points[i]
            p2 = points[i + 1]
            cx1 = p1.x + (p2.x - p0.x) / 6
            cy1 = p1.y + (p2.y - p0.y) / 6
            cx2 = p2.x - (p2.x - p0.x) / 6
            cy2 = p2.y - (p2.y - p0.y) / 6
            path.cubicTo(cx1, cy1, cx2, cy2, p2.x, p2.y)
        return path

    def build_variable_path(self, points: list) -> list[tuple[QPainterPath, float]]:
        """Construye segmentos de path con presion variable para cambio suave de tamano"""
        segments = []
        if len(points) < 2:
            if len(points) == 1:
                p = points[0]
                path = QPainterPath()
                sz = self.cfg.size * p.pressure * 0.5
                path.addEllipse(QPointF(p.x, p.y), max(0.5, sz), max(0.5, sz))
                segments.append((path, p.pressure))
            return segments

        # Smooth pressure-aware segments
        i = 0
        while i < len(points) - 1:
            j = min(i + 20, len(points) - 1)
            subpath = QPainterPath()
            sub = points[i:j + 1]
            avg_pressure = sum(p.pressure for p in sub) / len(sub)
            subpath.moveTo(sub[0].x, sub[0].y)
            if len(sub) == 2:
                subpath.lineTo(sub[1].x, sub[1].y)
            else:
                for k in range(1, len(sub) - 1):
                    p0 = sub[k - 1]; p1 = sub[k]; p2 = sub[k + 1]
                    cx1 = p1.x + (p2.x - p0.x) / 6
                    cy1 = p1.y + (p2.y - p0.y) / 6
                    cx2 = p2.x - (p2.x - p0.x) / 6
                    cy2 = p2.y - (p2.y - p0.y) / 6
                    subpath.cubicTo(cx1, cy1, cx2, cy2, p2.x, p2.y)
            segments.append((subpath, avg_pressure))
            i = j
        return segments


# ═══════════════════════════════════════════════════════
# HERRAMIENTA DE RELLENO (FLOOD FILL)
# ═══════════════════════════════════════════════════════

def flood_fill(img: QImage, x: int, y: int, fill_color: QColor, tolerance: int = 30):
    """Algoritmo de flood fill (bucket) con tolerancia"""
    w, h = img.width(), img.height()
    if x < 0 or x >= w or y < 0 or y >= h:
        return

    target = img.pixelColor(x, y)
    if target == fill_color:
        return

    # Usar scanline flood fill
    pixels = img.bits()
    img_format = img.format()

    # Convertir a QImage temporal para el fill si es necesario
    filled = QImage(img)
    stack = [(x, y)]
    visited = set()

    tr = target.red(); tg = target.green(); tb = target.blue(); ta = target.alpha()
    fr = fill_color.red(); fg = fill_color.green(); fb = fill_color.blue(); fa = fill_color.alpha()

    while stack:
        px, py = stack.pop()
        if (px, py) in visited:
            continue
        if px < 0 or px >= w or py < 0 or py >= h:
            continue
        c = filled.pixelColor(px, py)
        if abs(c.red() - tr) > tolerance or abs(c.green() - tg) > tolerance or \
           abs(c.blue() - tb) > tolerance or abs(c.alpha() - ta) > tolerance:
            continue

        visited.add((px, py))

        # Scan left
        lx = px
        while lx >= 0:
            c = filled.pixelColor(lx, py)
            if abs(c.red() - tr) <= tolerance and abs(c.green() - tg) <= tolerance and \
               abs(c.blue() - tb) <= tolerance and abs(c.alpha() - ta) <= tolerance:
                filled.setPixelColor(lx, py, fill_color)
                visited.add((lx, py))
                # Check above and below
                if py > 0: stack.append((lx, py - 1))
                if py < h - 1: stack.append((lx, py + 1))
                lx -= 1
            else:
                break

        # Scan right
        rx = px + 1
        while rx < w:
            c = filled.pixelColor(rx, py)
            if abs(c.red() - tr) <= tolerance and abs(c.green() - tg) <= tolerance and \
               abs(c.blue() - tb) <= tolerance and abs(c.alpha() - ta) <= tolerance:
                filled.setPixelColor(rx, py, fill_color)
                visited.add((rx, py))
                if py > 0: stack.append((rx, py - 1))
                if py < h - 1: stack.append((rx, py + 1))
                rx += 1
            else:
                break

    # Copy result back
    p = QPainter(img)
    p.setCompositionMode(QPainter.CompositionMode_Source)
    p.drawImage(0, 0, filled)
    p.end()


# ═══════════════════════════════════════════════════════
# CANVAS - WIDGET PRINCIPAL DE DIBUJO
# ═══════════════════════════════════════════════════════

class Canvas(QWidget):
    canvas_updated = Signal()

    def __init__(self, doc: Document, brush_eng: BrushEngine):
        super().__init__()
        self.doc = doc
        self.brush = brush_eng
        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.StrongFocus)
        self.setAttribute(Qt.WA_TabletTracking, True)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.setMinimumSize(400, 300)

        # Viewport
        self._zoom = 1.0
        self._off_x = 0.0
        self._off_y = 0.0
        self._rot = 0  # canvas rotation in degrees
        self._flip_h = False

        # Input
        self.tab = TabletState()
        self._drawing = False
        self._tool = Tool.BRUSH
        self._color = QColor(0, 0, 0, 255)
        self._stroke_pts: list = []
        self._before_img: QImage | None = None
        self._stroke_path: QPainterPath = QPainterPath()  # preview path
        self._last_stroke_size: float = 10.0

        # Hand tool
        self._panning = False
        self._pan_start = QPointF()
        self._pan_off_start = QPointF()

        # Move tool
        self._moving_selection = False
        self._sel_rect: QRectF | None = None  # selection rectangle
        self._sel_image: QImage | None = None

        # Shape tools (line, rect, ellipse)
        self._shape_start: QPointF | None = None
        self._shape_end: QPointF | None = None

        # Cursor overlay
        self._show_cursor = True

        self.setCursor(Qt.CrossCursor)

    # ── Propiedades ──────────────────────────────
    @property
    def zoom(self): return self._zoom
    @zoom.setter
    def zoom(self, z):
        self._zoom = max(0.02, min(64.0, z))
        self.update()

    @property
    def tool(self): return self._tool
    @tool.setter
    def tool(self, t: Tool):
        self._tool = t
        if t == Tool.HAND:
            self.setCursor(Qt.OpenHandCursor)
        elif t in (Tool.EYEDROPPER, Tool.MOVE):
            self.setCursor(Qt.ArrowCursor)
        elif t == Tool.BUCKET:
            self.setCursor(QCursor(QPixmap(16, 16)))  # crosshair
            self.setCursor(Qt.CrossCursor)
        elif t in (Tool.SELECT_RECT, Tool.SELECT_LASSO):
            self.setCursor(Qt.CrossCursor)
        else:
            self.setCursor(Qt.CrossCursor)
        self.update()

    @property
    def color(self): return self._color
    @color.setter
    def color(self, c: QColor):
        self._color = c
        self.brush.cfg.color = c

    # ── Transformaciones ────────────────────────
    def _to_canvas(self, sp: QPointF) -> QPointF:
        """Screen -> Canvas coords"""
        return (sp - QPointF(self._off_x, self._off_y)) / self._zoom

    def _to_screen(self, cp: QPointF) -> QPointF:
        """Canvas -> Screen coords"""
        return cp * self._zoom + QPointF(self._off_x, self._off_y)

    # ── Eventos de tableta Wacom ────────────────
    def tabletEvent(self, ev: QTabletEvent):
        """Maneja eventos de tableta con presion, tilt, rotacion"""
        ev.accept()
        self.tab.is_tablet = True
        self.tab.pressure = ev.pressure()
        self.tab.tilt_x = ev.xTilt()
        self.tab.tilt_y = ev.yTilt()
        self.tab.rotation = ev.rotation()
        self.tab.pos = ev.position()
        self.tab.is_erasing = (ev.pointerType() == QTabletEvent.Eraser)

        t = ev.type()
        if t == QTabletEvent.TabletPress:
            self._input_press(ev.position())
        elif t == QTabletEvent.TabletMove:
            if self._drawing:
                self._input_move(ev.position())
        elif t == QTabletEvent.TabletRelease:
            if self._drawing:
                self._input_release()

        self.update()

    # ── Eventos de mouse (fallback + pan) ────────
    def mousePressEvent(self, ev: QMouseEvent):
        # Si la tableta estaba activa pero ahora recibimos mouse,
        # significa que la tableta se alejo
        if self.tab.is_tablet and not self._drawing:
            self.tab.is_tablet = False

        if self.tab.is_tablet:
            return

        self.tab.pressure = 1.0
        self.tab.pos = ev.position()

        if ev.button() == Qt.MiddleButton:
            self._panning = True
            self._pan_start = ev.position()
            self._pan_off_start = QPointF(self._off_x, self._off_y)
            self.setCursor(Qt.ClosedHandCursor)
            return
        if ev.button() == Qt.LeftButton:
            self._input_press(ev.position())

    def mouseMoveEvent(self, ev: QMouseEvent):
        # Si recibimos evento de mouse pero estabamos en modo tableta,
        # significa que la tableta ya no esta activa
        if self.tab.is_tablet and not self._drawing:
            self.tab.is_tablet = False

        if self.tab.is_tablet:
            return
        self.tab.pos = ev.position()

        if self._panning:
            delta = ev.position() - self._pan_start
            self._off_x = self._pan_off_start.x() + delta.x()
            self._off_y = self._pan_off_start.y() + delta.y()
            self.update()
            return
        if self._drawing:
            self._input_move(ev.position())
        self.update()

    def mouseReleaseEvent(self, ev: QMouseEvent):
        if self.tab.is_tablet:
            return
        self.tab.pos = ev.position()

        if self._panning:
            self._panning = False
            self.setCursor(Qt.CrossCursor)
            return
        if self._drawing:
            self._input_release()

    def wheelEvent(self, ev: QWheelEvent):
        """Zoom con rueda del mouse"""
        factor = 1.12
        mpos = ev.position()
        old_cpos = self._to_canvas(mpos)
        if ev.angleDelta().y() > 0:
            self._zoom *= factor
        else:
            self._zoom /= factor
        self._zoom = max(0.02, min(64.0, self._zoom))
        new_cpos = self._to_canvas(mpos)
        self._off_x += (new_cpos.x() - old_cpos.x()) * self._zoom
        self._off_y += (new_cpos.y() - old_cpos.y()) * self._zoom
        self.update()

    def keyPressEvent(self, ev: QKeyEvent):
        k = ev.key(); ctrl = ev.modifiers() & Qt.ControlModifier
        shift = ev.modifiers() & Qt.ShiftModifier

        if ctrl and k == Qt.Key_Z:
            self._undo()
        elif ctrl and (k == Qt.Key_Y or (k == Qt.Key_Z and shift)):
            self._redo()
        elif ctrl and k == Qt.Key_0:
            self._zoom = 1.0; self._off_x = 0; self._off_y = 0; self._rot = 0
            self.fit(); self.update()
        elif ctrl and k == Qt.Key_S:
            self._save_project()
        elif k == Qt.Key_B:
            self.tool = Tool.BRUSH
        elif k == Qt.Key_E:
            self.tool = Tool.ERASER
        elif k == Qt.Key_I:
            self.tool = Tool.EYEDROPPER
        elif k == Qt.Key_G:
            self.tool = Tool.BUCKET
        elif k == Qt.Key_L:
            self.tool = Tool.LINE
        elif k == Qt.Key_U:
            self.tool = Tool.RECT
        elif k == Qt.Key_Y and not ctrl:
            self.tool = Tool.ELLIPSE
        elif k == Qt.Key_M:
            self.tool = Tool.MOVE
        elif k == Qt.Key_H:
            self._flip_h = not self._flip_h; self.update()
        elif k == Qt.Key_R and not ctrl:
            self._rot = (self._rot + 15) % 360; self.update()
        elif k == Qt.Key_Apostrophe:
            self.doc.show_grid = not self.doc.show_grid; self.update()
        elif k == Qt.Key_Left:
            self.doc.current_frame = max(0, self.doc.current_frame - 1)
            self.canvas_updated.emit()
        elif k == Qt.Key_Right:
            self.doc.current_frame = min(self.doc.total_frames - 1, self.doc.current_frame + 1)
            self.canvas_updated.emit()
        elif k == Qt.Key_Delete:
            self.doc.clear_frame(); self.canvas_updated.emit()
        elif k == Qt.Key_F:
            self.fit()

    # ── Logica de entrada unificada ─────────────
    def _input_press(self, screen_pos: QPointF):
        if self._tool == Tool.HAND:
            return
        if self._tool == Tool.EYEDROPPER:
            self._do_eyedropper(screen_pos)
            return

        cpos = self._to_canvas(screen_pos)

        if self._tool == Tool.BUCKET:
            self._do_fill(cpos)
            return

        if self._tool == Tool.MOVE:
            # Check if clicking on something to move
            self._moving_selection = True
            self._move_start = cpos
            return

        if self._tool in (Tool.SELECT_RECT, Tool.SELECT_LASSO):
            self._sel_start = cpos
            self._sel_rect = None
            self._drawing = True
            return

        if self._tool in (Tool.LINE, Tool.RECT, Tool.ELLIPSE):
            self._shape_start = cpos
            self._shape_end = cpos
            self._drawing = True
            return

        # Brush/eraser - start stroke
        self._drawing = True
        self._stroke_pts = []
        self._stroke_path = QPainterPath()
        pressure = self._get_pressure()
        self._before_img = self.doc.snap()
        pt = self._make_point(cpos, pressure)
        self._stroke_pts.append(pt)
        self._stroke_path.moveTo(cpos.x(), cpos.y())

    def _input_move(self, screen_pos: QPointF):
        cpos = self._to_canvas(screen_pos)
        if not self._drawing:
            # Cursor position update for hover
            self.update()
            return

        if self._tool == Tool.MOVE:
            if self._moving_selection and self._sel_image:
                pass
            return

        if self._tool == Tool.SELECT_RECT:
            self._sel_rect = QRectF(self._sel_start, cpos).normalized()
            self.update()
            return

        if self._tool in (Tool.LINE, Tool.RECT, Tool.ELLIPSE):
            self._shape_end = cpos
            self.update()
            return

        # Brush/eraser stroke - add points and grow path
        pressure = self._get_pressure()
        last = self._stroke_pts[-1] if self._stroke_pts else None

        # Spacing check for smoother strokes
        if last:
            dist = QLineF(QPointF(last.x, last.y), cpos).length()
            min_spacing = self.brush.cfg.size * self.brush.cfg.spacing * 0.3
            if dist < max(0.5, min_spacing) and self.brush.cfg.stabilizer == 0:
                return

        pt = self._make_point(cpos, pressure)

        # Stabilizer: simple moving average
        if self.brush.cfg.stabilizer > 0:
            self._stroke_pts.append(pt)
            window = min(self.brush.cfg.stabilizer + 1, len(self._stroke_pts))
            recent = self._stroke_pts[-window:]
            avg_x = sum(p.x for p in recent) / len(recent)
            avg_y = sum(p.y for p in recent) / len(recent)
            pt = self._make_point(QPointF(avg_x, avg_y), sum(p.pressure for p in recent) / len(recent))
        else:
            self._stroke_pts.append(pt)

        # Grow the live stroke path for preview
        if len(self._stroke_pts) == 1:
            self._stroke_path.moveTo(pt.x, pt.y)
        else:
            self._stroke_path.lineTo(pt.x, pt.y)

        self._last_stroke_size = self.brush.cfg.size * pressure
        self.update()

    def _input_release(self):
        if not self._drawing:
            return
        self._drawing = False

        if self._tool in (Tool.SELECT_RECT, Tool.SELECT_LASSO):
            self.update()
            return

        if self._tool in (Tool.LINE, Tool.RECT, Tool.ELLIPSE):
            if self._shape_start and self._shape_end:
                self._draw_shape()
            self._shape_start = None
            self._shape_end = None
            self.update()
            return

        # Brush/eraser - commit stroke
        if len(self._stroke_pts) > 0:
            try:
                self._commit_stroke()
            except Exception as e:
                print(f"Error committing stroke: {e}", file=sys.stderr)
                import traceback; traceback.print_exc()

        self._stroke_pts.clear()
        self._stroke_path = QPainterPath()
        self._before_img = None
        self.canvas_updated.emit()

        # Reset tablet flag to allow mouse fallback on next stroke
        self.tab.is_tablet = False
        self.update()

    # ── Acciones ────────────────────────────────
    def _get_pressure(self) -> float:
        p = self.tab.pressure if self.tab.is_tablet else 1.0
        return max(0.01, min(1.0, p))

    def _make_point(self, pos: QPointF, pressure: float):
        return type('pt', (), {'x': pos.x(), 'y': pos.y(), 'pressure': pressure})()

    def _do_eyedropper(self, sp: QPointF):
        cpos = self._to_canvas(sp)
        img = self.doc.cur_image()
        x, y = int(cpos.x()), int(cpos.y())
        if 0 <= x < img.width() and 0 <= y < img.height():
            c = img.pixelColor(x, y)
            if c.alpha() > 5:
                self._color = c
                self.brush.cfg.color = c
                self.canvas_updated.emit()

    def _do_fill(self, cpos: QPointF):
        img = self.doc.cur_image()
        x, y = int(cpos.x()), int(cpos.y())
        if 0 <= x < img.width() and 0 <= y < img.height():
            before = self.doc.snap()
            flood_fill(img, x, y, self._color, tolerance=25)
            after = self.doc.snap()
            self.doc.undo.push(UndoCmd(self.doc.current_layer, self.doc.current_frame, before, after))
            self.canvas_updated.emit()

    def _draw_shape(self):
        """Render line/rect/ellipse onto current image"""
        if not self._shape_start or not self._shape_end:
            return
        before = self.doc.snap()
        img = self.doc.cur_image()
        p = QPainter(img)
        p.setRenderHint(QPainter.Antialiasing)
        pen = self.brush.create_pen(1.0)
        p.setPen(pen)
        x1, y1 = self._shape_start.x(), self._shape_start.y()
        x2, y2 = self._shape_end.x(), self._shape_end.y()

        if self._tool == Tool.LINE:
            p.drawLine(QPointF(x1, y1), QPointF(x2, y2))
        elif self._tool == Tool.RECT:
            r = QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized()
            p.drawRect(r)
        elif self._tool == Tool.ELLIPSE:
            r = QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized()
            p.drawEllipse(r)
        p.end()
        after = self.doc.snap()
        self.doc.undo.push(UndoCmd(self.doc.current_layer, self.doc.current_frame, before, after))

    def _commit_stroke(self):
        """Renderizar el trazo completo al QImage con calidad maxima"""
        segments = self.brush.build_variable_path(self._stroke_pts)
        if not segments:
            return

        img = self.doc.cur_image()
        p = QPainter(img)
        p.setRenderHint(QPainter.Antialiasing, True)
        p.setRenderHint(QPainter.SmoothPixmapTransform, True)

        for path, pressure in segments:
            if self._tool == Tool.ERASER or self.tab.is_erasing:
                pen = self.brush.create_eraser_pen(pressure)
                p.setCompositionMode(QPainter.CompositionMode_DestinationOut)
            else:
                pen = self.brush.create_pen(pressure)
                p.setCompositionMode(QPainter.CompositionMode_SourceOver)
            p.setPen(pen)
            p.drawPath(path)

        p.end()
        after = self.doc.snap()
        if self._before_img is not None:
            self.doc.undo.push(UndoCmd(self.doc.current_layer, self.doc.current_frame, self._before_img, after))

    def _undo(self):
        cmd = self.doc.undo.undo()
        if cmd:
            self.doc.layers[cmd.layer_idx].frames[cmd.frame_idx] = QImage(cmd.before)
            self.doc.current_frame = cmd.frame_idx
            self.doc.current_layer = cmd.layer_idx
            self.canvas_updated.emit()

    def _redo(self):
        cmd = self.doc.undo.redo()
        if cmd:
            self.doc.layers[cmd.layer_idx].frames[cmd.frame_idx] = QImage(cmd.after)
            self.doc.current_frame = cmd.frame_idx
            self.doc.current_layer = cmd.layer_idx
            self.canvas_updated.emit()

    def _save_project(self):
        path, _ = QFileDialog.getSaveFileName(self, "Save Project", "project.fap.json",
                                              "FAP Project (*.fap.json);;All Files (*)")
        if path:
            # Save as individual frames
            base = os.path.dirname(path)
            name = os.path.splitext(os.path.basename(path))[0]
            frames_dir = os.path.join(base, f"{name}_frames")
            os.makedirs(frames_dir, exist_ok=True)
            for li, layer in enumerate(self.doc.layers):
                for fi in layer.frames:
                    fname = os.path.join(frames_dir, f"L{li:02d}_F{fi:04d}.png")
                    layer.frames[fi].save(fname)
            QMessageBox.information(self, "Saved", f"Saved to {frames_dir}")

    def _update_cursor_for_tool(self):
        if self._panning:
            self.setCursor(Qt.ClosedHandCursor)
        elif self._tool == Tool.HAND:
            self.setCursor(Qt.OpenHandCursor)
        elif self._tool in (Tool.EYEDROPPER,):
            self.setCursor(Qt.CrossCursor)
        else:
            self.setCursor(Qt.CrossCursor)

    # ── Pintado ────────────────────────────────
    def paintEvent(self, ev):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        p.setRenderHint(QPainter.SmoothPixmapTransform)

        # Fondo gris oscuro
        p.fillRect(self.rect(), QColor(40, 40, 42))

        cr = QRectF(0, 0, self.doc.cw, self.doc.ch)
        # Screen rect for canvas
        sr = QRectF(
            self._off_x, self._off_y,
            cr.width() * self._zoom,
            cr.height() * self._zoom
        )

        # Aplicar rotacion y flip
        p.save()
        if self._rot != 0 or self._flip_h:
            cx = sr.center()
            p.translate(cx)
            if self._rot != 0:
                p.rotate(self._rot)
            if self._flip_h:
                p.scale(-1.0, 1.0)
            p.translate(-cx)

        # Clipping al area del canvas
        p.setClipRect(sr)

        # Checkerboard de transparencia
        self._paint_checker(p, cr)

        # Fondo blanco
        p.fillRect(cr, self.doc.bg_color)

        # Grid
        if self.doc.show_grid and self.doc.grid_size > 0:
            self._paint_grid(p, cr)

        # ─── Onion Skin: Previos (rojo) ───
        for i in range(1, self.doc.onion_prev + 1):
            fi = self.doc.current_frame - i
            if fi < 0: break
            a = int(255 * self.doc.onion_opacity * (1.0 - i / (self.doc.onion_prev + 1.5)))
            self._render_frame_composite(p, fi, QColor(255, 60, 60, a))

        # ─── Onion Skin: Siguientes (azul) ───
        for i in range(1, self.doc.onion_next + 1):
            fi = self.doc.current_frame + i
            if fi >= self.doc.total_frames: break
            a = int(255 * self.doc.onion_opacity * (1.0 - i / (self.doc.onion_next + 1.5)))
            self._render_frame_composite(p, fi, QColor(60, 100, 255, a))

        # ─── Frame actual ───
        for layer in self.doc.layers:
            if not layer.visible: continue
            frame = layer.get(self.doc.current_frame, self.doc.size)
            p.setOpacity(layer.opacity)
            p.drawImage(QPointF(0, 0), frame)
        p.setOpacity(1.0)

        # ─── Preview temporal del trazo actual ───
        if self._drawing and self._tool in (Tool.BRUSH, Tool.ERASER) and len(self._stroke_pts) > 1:
            self._render_live_stroke(p)

        # ─── Preview de shape tools ───
        if self._shape_start and self._shape_end and self._tool in (Tool.LINE, Tool.RECT, Tool.ELLIPSE):
            self._render_shape_preview(p)

        # ─── Selection rectangle ───
        if self._sel_rect and self._tool == Tool.SELECT_RECT:
            p.setPen(QPen(QColor(60, 140, 255), 1.5 / self._zoom, Qt.DashLine))
            p.setBrush(QBrush(QColor(60, 140, 255, 30)))
            p.drawRect(self._sel_rect)

        p.restore()

        # ─── Borde del canvas ───
        p.setPen(QPen(QColor(80, 80, 80), 1))
        p.drawRect(sr)

        # ─── Cursor overlay (circulo de pincel) ───
        if self._show_cursor and not self._drawing and not self._panning and \
           self._tool in (Tool.BRUSH, Tool.ERASER):
            cpos = self._to_canvas(self.tab.pos)
            sz = self.brush.cfg.size * self._zoom
            scx = self._off_x + cpos.x() * self._zoom
            scy = self._off_y + cpos.y() * self._zoom
            p.setPen(QPen(QColor(255, 255, 255, 160), 1.5))
            p.setBrush(QBrush(QColor(180, 180, 180, 30)))
            p.drawEllipse(QPointF(scx, scy), sz / 2, sz / 2)

        # ─── Info overlay ───
        self._paint_info_overlay(p)

        p.end()

    def _paint_checker(self, p: QPainter, rect: QRectF):
        cs = 16
        light = QColor(195, 195, 195)
        dark = QColor(165, 165, 165)
        x0, y0 = int(rect.x()), int(rect.y())
        x1, y1 = int(rect.right()) + cs, int(rect.bottom()) + cs
        for y in range(y0, y1, cs):
            for x in range(x0, x1, cs):
                c = light if ((x // cs + y // cs) % 2 == 0) else dark
                p.fillRect(QRectF(x, y, cs, cs), c)

    def _paint_grid(self, p: QPainter, rect: QRectF):
        gs = self.doc.grid_size
        p.setPen(QPen(QColor(180, 180, 180, 60), 0.5))
        x0, y0 = int(rect.x()), int(rect.y())
        x1, y1 = int(rect.right()), int(rect.bottom())
        for x in range(gs * (x0 // gs), x1, gs):
            p.drawLine(QPointF(x, y0), QPointF(x, y1))
        for y in range(gs * (y0 // gs), y1, gs):
            p.drawLine(QPointF(x0, y), QPointF(x1, y))

    def _render_frame_composite(self, p: QPainter, fi: int, tint: QColor):
        """Render all visible layers for a frame with tint"""
        combined = QImage(self.doc.cw, self.doc.ch, QImage.Format_ARGB32_Premultiplied)
        combined.fill(Qt.transparent)
        cp = QPainter(combined)
        for layer in self.doc.layers:
            if not layer.visible: continue
            img = layer.get(fi, self.doc.size)
            cp.setOpacity(layer.opacity)
            cp.drawImage(0, 0, img)
        cp.end()

        tinted = QImage(combined.size(), QImage.Format_ARGB32_Premultiplied)
        tinted.fill(tint)
        tp = QPainter(tinted)
        tp.setCompositionMode(QPainter.CompositionMode_DestinationIn)
        tp.drawImage(0, 0, combined)
        tp.end()
        p.drawImage(0, 0, tinted)

    def _render_live_stroke(self, p: QPainter):
        """Render live stroke preview while drawing"""
        if len(self._stroke_pts) < 2:
            return
        p.save()
        segments = self.brush.build_variable_path(self._stroke_pts)
        if self._tool == Tool.ERASER or self.tab.is_erasing:
            p.setCompositionMode(QPainter.CompositionMode_DestinationOut)
        for path, pressure in segments:
            pen = self.brush.create_pen(pressure) if self._tool != Tool.ERASER else self.brush.create_eraser_pen(pressure)
            p.setPen(pen)
            p.drawPath(path)
        p.restore()

    def _render_shape_preview(self, p: QPainter):
        p.setPen(QPen(QColor(60, 140, 255, 180), 2.0 / self._zoom, Qt.DashLine))
        x1, y1 = self._shape_start.x(), self._shape_start.y()
        x2, y2 = self._shape_end.x(), self._shape_end.y()
        if self._tool == Tool.LINE:
            p.drawLine(QPointF(x1, y1), QPointF(x2, y2))
        elif self._tool == Tool.RECT:
            p.drawRect(QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized())
        elif self._tool == Tool.ELLIPSE:
            p.drawEllipse(QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized())

    def _paint_info_overlay(self, p: QPainter):
        """Paint coordinate/zoom info in corner"""
        p.save()
        p.setFont(QFont("Consolas", 9))
        cpos = self._to_canvas(self.tab.pos)
        text = f"XY: {int(cpos.x())}, {int(cpos.y())}  |  Zoom: {int(self._zoom * 100)}%  |  "
        text += f"Frame: {self.doc.current_frame + 1}/{self.doc.total_frames}"
        text += f"  |  Layer: {self.doc.cur_layer.name}"
        if self._rot != 0:
            text += f"  |  Rot: {self._rot}deg"
        if self._flip_h:
            text += "  |  FLIPPED"
        fm = p.fontMetrics()
        tw = fm.horizontalAdvance(text) + 16
        th = fm.height() + 8
        x = self.width() - tw - 8
        y = self.height() - th - 8
        p.fillRect(QRectF(x, y, tw, th), QColor(0, 0, 0, 140))
        p.setPen(QColor(200, 200, 200))
        p.drawText(QRectF(x + 8, y + 4, tw - 8, th - 4), text)
        p.restore()

    def fit(self):
        """Fit canvas in viewport"""
        if self.width() == 0 or self.height() == 0: return
        zw = self.width() / self.doc.cw
        zh = self.height() / self.doc.ch
        self._zoom = min(zw, zh) * 0.80
        self._off_x = (self.width() - self.doc.cw * self._zoom) / 2
        self._off_y = (self.height() - self.doc.ch * self._zoom) / 2
        self.update()

    def resizeEvent(self, ev):
        super().resizeEvent(ev)
        self.fit()


# ═══════════════════════════════════════════════════════
# PANEL DE HERRAMIENTAS REDISENADO
# ═══════════════════════════════════════════════════════

class ToolPanel(QWidget):
    tool_changed = Signal(Tool)
    color_changed = Signal(QColor)
    settings_changed = Signal()

    def __init__(self, brush_eng: BrushEngine):
        super().__init__()
        self.brush = brush_eng
        self.setMinimumWidth(180)
        self.setMaximumWidth(220)

        self.setStyleSheet("""
            QGroupBox { color: #ccc; font-weight: bold; font-size: 10px; border: 1px solid #555; border-radius: 4px; margin-top: 12px; padding-top: 16px; }
            QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 6px; }
            QToolButton { background: #3a3a3d; color: #ddd; border: 1px solid #555; border-radius: 3px; font-weight: bold; font-size: 11px; }
            QToolButton:checked { background: #d4782a; color: #fff; border-color: #e09040; }
            QToolButton:hover { background: #555; }
            QToolButton:hover:checked { background: #e09040; }
            QSlider::groove:horizontal { background: #333; height: 6px; border-radius: 3px; }
            QSlider::handle:horizontal { background: #d4782a; width: 14px; margin: -4px 0; border-radius: 7px; }
            QSlider::sub-page:horizontal { background: #d4782a; border-radius: 3px; }
            QLabel { color: #bbb; font-size: 10px; }
            QSpinBox, QDoubleSpinBox, QComboBox { background: #333; color: #ddd; border: 1px solid #555; padding: 2px 4px; font-size: 10px; }
            QCheckBox { color: #bbb; font-size: 10px; }
        """)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(6)

        # ── Grupos de herramientas ──
        self._build_tool_buttons(layout)
        self._build_color_section(layout)
        self._build_brush_section(layout)
        self._build_shape_section(layout)

        layout.addStretch()

    def _build_tool_buttons(self, parent_layout):
        grp = QGroupBox("TOOLS")
        grid = QGridLayout(grp)
        grid.setSpacing(3)

        tools = [
            (Tool.BRUSH,    "Brush",      "B", "🖌"),
            (Tool.ERASER,   "Eraser",     "E", "◼"),
            (Tool.EYEDROPPER, "Pick Color","I", "💧"),
            (Tool.BUCKET,   "Fill",       "G", "🪣"),
            (Tool.LINE,     "Line",       "L", "╱"),
            (Tool.RECT,     "Rectangle",  "U", "▭"),
            (Tool.ELLIPSE,  "Ellipse",    "Y", "◯"),
            (Tool.MOVE,     "Move",       "M", "↕"),
            (Tool.SELECT_RECT, "Select",  "S", "⬚"),
            (Tool.HAND,     "Hand/Pan",   "H", "✋"),
        ]

        self._btn_grp = QButtonGroup(self)
        for i, (tid, label, key, icon_char) in enumerate(tools):
            btn = QToolButton()
            btn.setText(f"{icon_char} {label}")
            btn.setToolTip(f"{label} ({key})")
            btn.setCheckable(True)
            btn.setFixedHeight(30)
            btn.setToolButtonStyle(Qt.ToolButtonTextBesideIcon)
            btn.clicked.connect(lambda checked, t=tid: self.tool_changed.emit(t))
            self._btn_grp.addButton(btn, int(tid))
            grid.addWidget(btn, i // 2, i % 2)
        # Select first
        self._btn_grp.button(int(Tool.BRUSH)).setChecked(True)

        parent_layout.addWidget(grp)

    def _build_color_section(self, parent_layout):
        grp = QGroupBox("COLOR")
        v = QVBoxLayout(grp)
        self._clr_btn = QPushButton()
        self._clr_btn.setFixedSize(80, 40)
        self._clr_btn.setStyleSheet(
            f"background: {self.brush.cfg.color.name()}; border: 2px solid #777; border-radius: 4px;"
        )
        self._clr_btn.clicked.connect(self._pick_color)
        v.addWidget(self._clr_btn, alignment=Qt.AlignCenter)
        parent_layout.addWidget(grp)

    def _build_brush_section(self, parent_layout):
        cfg = self.brush.cfg
        grp = QGroupBox("BRUSH")
        v = QVBoxLayout(grp)
        v.setSpacing(4)

        # Size
        row = QHBoxLayout()
        row.addWidget(QLabel("Size"))
        sz = QSlider(Qt.Horizontal)
        sz.setRange(1, 300)
        sz.setValue(int(cfg.size))
        sz.valueChanged.connect(lambda v: self._update_cfg('size', float(v)))
        row.addWidget(sz)
        self._sz_lbl = QLabel(f"{int(cfg.size)}px"); self._sz_lbl.setFixedWidth(35)
        row.addWidget(self._sz_lbl)
        v.addLayout(row)
        # Store ref for update
        self._sz_slider = sz

        # Min size
        row = QHBoxLayout()
        row.addWidget(QLabel("Min"))
        msz = QSlider(Qt.Horizontal)
        msz.setRange(0, 100)
        msz.setValue(int(cfg.min_size))
        msz.valueChanged.connect(lambda v: self._update_cfg('min_size', float(v)))
        row.addWidget(msz)
        self._msz_lbl = QLabel(f"{int(cfg.min_size)}px"); self._msz_lbl.setFixedWidth(35)
        row.addWidget(self._msz_lbl)
        v.addLayout(row)

        # Opacity
        row = QHBoxLayout()
        row.addWidget(QLabel("Opac"))
        op = QSlider(Qt.Horizontal)
        op.setRange(1, 100)
        op.setValue(int(cfg.opacity * 100))
        op.valueChanged.connect(lambda v: self._update_cfg('opacity', v / 100.0))
        row.addWidget(op)
        self._op_lbl = QLabel(f"{int(cfg.opacity * 100)}%"); self._op_lbl.setFixedWidth(35)
        row.addWidget(self._op_lbl)
        v.addLayout(row)

        # Hardness
        row = QHBoxLayout()
        row.addWidget(QLabel("Hard"))
        hd = QSlider(Qt.Horizontal)
        hd.setRange(0, 100)
        hd.setValue(int(cfg.hardness * 100))
        hd.valueChanged.connect(lambda v: self._update_cfg('hardness', v / 100.0))
        row.addWidget(hd)
        self._hd_lbl = QLabel(f"{int(cfg.hardness * 100)}%"); self._hd_lbl.setFixedWidth(35)
        row.addWidget(self._hd_lbl)
        v.addLayout(row)

        # Stabilizer
        row = QHBoxLayout()
        row.addWidget(QLabel("Stab"))
        sb = QSlider(Qt.Horizontal)
        sb.setRange(0, 20)
        sb.setValue(cfg.stabilizer)
        sb.valueChanged.connect(lambda v: self._update_cfg('stabilizer', v))
        row.addWidget(sb)
        self._sb_lbl = QLabel(str(cfg.stabilizer)); self._sb_lbl.setFixedWidth(20)
        row.addWidget(self._sb_lbl)
        v.addLayout(row)

        # Brush shape
        row = QHBoxLayout()
        row.addWidget(QLabel("Tip"))
        shape_cmb = QComboBox()
        shape_cmb.addItems(["Round", "Square", "Flat", "Calligraphy"])
        shape_cmb.setCurrentIndex(int(cfg.shape))
        shape_cmb.currentIndexChanged.connect(lambda i: self._update_cfg('shape', BrushShape(i)))
        row.addWidget(shape_cmb)
        v.addLayout(row)

        # Dynamics checkboxes
        row = QHBoxLayout()
        ps = QCheckBox("P.Size"); ps.setChecked(cfg.use_pressure_size)
        ps.toggled.connect(lambda v: self._update_cfg('use_pressure_size', v))
        row.addWidget(ps)
        po = QCheckBox("P.Opac"); po.setChecked(cfg.use_pressure_opacity)
        po.toggled.connect(lambda v: self._update_cfg('use_pressure_opacity', v))
        row.addWidget(po)
        v.addLayout(row)

        parent_layout.addWidget(grp)

    def _build_shape_section(self, parent_layout):
        grp = QGroupBox("CANVAS")
        v = QVBoxLayout(grp)
        v.setSpacing(3)
        row = QHBoxLayout()
        row.addWidget(QLabel("Canvas W"))
        cw = QSpinBox(); cw.setRange(64, 8192); cw.setValue(CANVAS_W)
        cw.setFixedWidth(60)
        row.addWidget(cw)
        v.addLayout(row)
        row = QHBoxLayout()
        row.addWidget(QLabel("Canvas H"))
        ch = QSpinBox(); ch.setRange(64, 8192); ch.setValue(CANVAS_H)
        ch.setFixedWidth(60)
        row.addWidget(ch)
        v.addLayout(row)
        btn = QPushButton("Apply Canvas Size")
        btn.clicked.connect(lambda: self._resize_canvas(cw.value(), ch.value()))
        v.addWidget(btn)
        parent_layout.addWidget(grp)

    def _update_cfg(self, attr: str, value):
        setattr(self.brush.cfg, attr, value)
        # Update labels
        cfg = self.brush.cfg
        if attr == 'size': self._sz_lbl.setText(f"{int(value)}px")
        elif attr == 'min_size': self._msz_lbl.setText(f"{int(value)}px")
        elif attr == 'opacity': self._op_lbl.setText(f"{int(value * 100):.0f}%")
        elif attr == 'hardness': self._hd_lbl.setText(f"{int(value * 100)}%")
        elif attr == 'stabilizer': self._sb_lbl.setText(str(int(value)))
        self.settings_changed.emit()

    def _pick_color(self):
        color = QColorDialog.getColor(self.brush.cfg.color, self, "Pick Color")
        if color.isValid():
            self.brush.cfg.color = color
            self._clr_btn.setStyleSheet(f"background: {color.name()}; border: 2px solid #777; border-radius: 4px;")
            self.color_changed.emit(color)

    def _resize_canvas(self, w: int, h: int):
        # This will notify the main window to resize document
        self.settings_changed.emit()


# ═══════════════════════════════════════════════════════
# PANEL DE CAPAS
# ═══════════════════════════════════════════════════════

class LayerPanel(QWidget):
    layer_changed = Signal(int)

    def __init__(self, doc: Document):
        super().__init__()
        self.doc = doc
        self.setMinimumWidth(150)
        self.setStyleSheet("""
            QPushButton { background: #4a4a4d; color: #ccc; border: 1px solid #555; padding: 3px 6px; border-radius: 3px; font-size: 10px; }
            QPushButton:hover { background: #5a5a5d; }
            QLabel { color: #ccc; font-size: 11px; }
        """)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)
        layout.setSpacing(3)

        title = QLabel("LAYERS")
        title.setStyleSheet("color: #999; font-weight: bold; font-size: 9px; letter-spacing: 2px;")
        layout.addWidget(title)

        self._list_area = QVBoxLayout()
        self._list_area.setSpacing(2)
        layout.addLayout(self._list_area)

        row = QHBoxLayout()
        add = QPushButton("+ Layer"); add.clicked.connect(self._add)
        rm = QPushButton("-"); rm.setFixedWidth(28); rm.clicked.connect(self._remove)
        row.addWidget(add); row.addWidget(rm)
        layout.addLayout(row)

        up_btn = QPushButton("Move Up")
        up_btn.clicked.connect(self._move_up)
        layout.addWidget(up_btn)

        layout.addStretch()
        self._build()

    def _build(self):
        # Clear
        while self._list_area.count():
            item = self._list_area.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

        for i, layer in enumerate(self.doc.layers):
            row = QWidget()
            if i == self.doc.current_layer:
                row.setStyleSheet("background: #3a5060; border-radius: 3px;")
            else:
                row.setStyleSheet("background: transparent; border-radius: 3px;")
            rl = QHBoxLayout(row)
            rl.setContentsMargins(4, 2, 4, 2)

            vis = QCheckBox(); vis.setChecked(layer.visible); vis.setFixedSize(16, 16)
            vis.toggled.connect(lambda v, li=i: self._toggle_vis(li, v))
            rl.addWidget(vis)

            lbl = QLabel(layer.name)
            lbl.setStyleSheet(f"color: {'#fff' if i == self.doc.current_layer else '#ccc'};")
            rl.addWidget(lbl)
            rl.addStretch()

            row.mousePressEvent = lambda ev, idx=i: self._select(idx)
            self._list_area.addWidget(row)

    def _select(self, idx: int):
        self.doc.current_layer = idx
        self._build()
        self.layer_changed.emit(idx)

    def _toggle_vis(self, idx: int, v: bool):
        self.doc.layers[idx].visible = v

    def _add(self):
        self.doc.add_layer()
        self._build()
        self.layer_changed.emit(self.doc.current_layer)

    def _remove(self):
        self.doc.remove_layer(self.doc.current_layer)
        self._build()
        self.layer_changed.emit(self.doc.current_layer)

    def _move_up(self):
        idx = self.doc.current_layer
        if idx > 0:
            self.doc.layers[idx], self.doc.layers[idx - 1] = self.doc.layers[idx - 1], self.doc.layers[idx]
            self.doc.current_layer = idx - 1
            self._build()
            self.layer_changed.emit(self.doc.current_layer)


# ═══════════════════════════════════════════════════════
# TIMELINE MEJORADO
# ═══════════════════════════════════════════════════════

class TimelineWidget(QWidget):
    frame_changed = Signal(int)

    def __init__(self, doc: Document):
        super().__init__()
        self.doc = doc
        self.setMinimumHeight(170)
        self.setMaximumHeight(200)
        self._fw = 90
        self._fh = 50
        self._scroll = 0
        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.NoFocus)  # Don't steal focus from canvas

    def paintEvent(self, ev):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        p.fillRect(self.rect(), QColor(48, 48, 50))

        total = self.doc.total_frames
        cur = self.doc.current_frame
        x0 = 8 - self._scroll

        for i in range(total):
            x = x0 + i * (self._fw + 4)
            if x + self._fw < 0 or x > self.width(): continue

            rect = QRectF(x, 6, self._fw, self._fh)
            if i == cur:
                p.setPen(QPen(QColor(212, 120, 42), 2.5))
                p.fillRect(rect, QColor(58, 58, 68))
            else:
                p.setPen(QPen(QColor(75, 75, 75), 1))
                p.fillRect(rect, QColor(42, 42, 46))

            p.drawRect(rect)

            # Frame number
            p.setPen(QColor(180, 180, 180))
            p.setFont(QFont("Consolas", 9))
            p.drawText(QRectF(x + 3, 8, self._fw - 6, 16), Qt.AlignLeft, f"#{i + 1}")

            # Thumbnail
            thumb_rect = QRectF(x + 3, 24, self._fw - 6, self._fh - 26)
            self._draw_thumb(p, i, thumb_rect)
            p.setPen(QPen(QColor(55, 55, 55), 0.5))
            p.drawRect(thumb_rect)

        # Controls info
        p.setPen(QColor(120, 120, 120))
        p.setFont(QFont("Consolas", 8))
        p.drawText(8, self.height() - 6, "Click frame to select | Scroll to navigate | Use controls below")

        p.end()

    def _draw_thumb(self, p: QPainter, fi: int, rect: QRectF):
        # Draw first visible layer's frame thumbnail
        for layer in self.doc.layers:
            if fi in layer.frames and layer.visible:
                img = layer.frames[fi]
                scaled = img.scaled(int(rect.width()), int(rect.height()),
                                   Qt.IgnoreAspectRatio, Qt.SmoothTransformation)
                p.drawImage(rect.topLeft(), scaled)
                return

    def mousePressEvent(self, ev: QMouseEvent):
        x = ev.position().x()
        x0 = 8 - self._scroll
        for i in range(self.doc.total_frames):
            fx = x0 + i * (self._fw + 4)
            if fx <= x <= fx + self._fw:
                self.doc.current_frame = i
                self.frame_changed.emit(i)
                self.update()
                return

    def wheelEvent(self, ev: QWheelEvent):
        self._scroll = max(0, self._scroll - ev.angleDelta().y() // 40)
        self.update()


class TimelineControls(QWidget):
    frame_changed = Signal(int)
    play_toggled = Signal(bool)

    def __init__(self, doc: Document):
        super().__init__()
        self.doc = doc
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)
        self.setFocusPolicy(Qt.NoFocus)  # Don't steal focus from canvas

        self.setStyleSheet("""
            QPushButton { background: #4a4a4d; color: #ccc; border: 1px solid #555; padding: 4px 8px; border-radius: 3px; font-size: 11px; }
            QPushButton:hover { background: #5a5a5d; }
            QSpinBox { background: #333; color: #ddd; border: 1px solid #555; padding: 2px 4px; }
            QLabel { color: #ccc; font-size: 10px; }
        """)

        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 4, 8, 4)
        layout.setSpacing(5)

        # Navigation
        layout.addWidget(self._btn("|<", self._first))
        layout.addWidget(self._btn("<", self._prev))
        self._play_btn = QPushButton(">")
        self._play_btn.setFixedWidth(35)
        self._play_btn.setStyleSheet("font-weight: bold; font-size: 14px; background: #d4782a; color: #fff;")
        self._play_btn.clicked.connect(self._toggle)
        layout.addWidget(self._play_btn)
        layout.addWidget(self._btn(">", self._next))
        layout.addWidget(self._btn(">|", self._last))

        layout.addSpacing(8)

        # Frame counter
        self._frm_lbl = QLabel(f"Frame {self.doc.current_frame + 1} / {self.doc.total_frames}")
        self._frm_lbl.setStyleSheet("color: #ddd; font-weight: bold;")
        layout.addWidget(self._frm_lbl)

        layout.addStretch()

        # Frame operations
        layout.addWidget(self._btn("+ Frame", self._add_frame))
        layout.addWidget(self._btn("Dup Frame", self._dup_frame))
        layout.addWidget(self._btn("Del Frame", self._del_frame))
        layout.addWidget(self._btn("Clear Frame", self._clear))

        layout.addSpacing(10)

        # FPS
        layout.addWidget(QLabel("FPS:"))
        fps = QSpinBox(); fps.setRange(1, 60); fps.setValue(self.doc.fps)
        fps.valueChanged.connect(self._set_fps); fps.setFixedWidth(48)
        layout.addWidget(fps)

        # Onion skin
        layout.addSpacing(8)
        layout.addWidget(QLabel("Onion:"))
        layout.addWidget(QLabel("Prev"))
        op = QSpinBox(); op.setRange(0, 5); op.setValue(self.doc.onion_prev); op.setFixedWidth(36)
        op.valueChanged.connect(lambda v: setattr(self.doc, 'onion_prev', v))
        layout.addWidget(op)
        layout.addWidget(QLabel("Next"))
        on = QSpinBox(); on.setRange(0, 5); on.setValue(self.doc.onion_next); on.setFixedWidth(36)
        on.valueChanged.connect(lambda v: setattr(self.doc, 'onion_next', v))
        layout.addWidget(on)

    def _btn(self, text, fn) -> QPushButton:
        btn = QPushButton(text)
        btn.clicked.connect(fn)
        return btn

    def _tick(self):
        self.doc.current_frame += 1
        if self.doc.current_frame >= self.doc.total_frames:
            self.doc.current_frame = 0 if self.doc.loop else self.doc.total_frames - 1
            if not self.doc.loop:
                self._stop()
        self.frame_changed.emit(self.doc.current_frame)
        self._upd()

    def _toggle(self):
        if self.doc.playing:
            self._stop()
        else:
            self._timer.start(int(1000 / self.doc.fps))
            self.doc.playing = True
            self._play_btn.setText("||")
            self.play_toggled.emit(True)

    def _stop(self):
        self._timer.stop()
        self.doc.playing = False
        self._play_btn.setText(">")
        self.play_toggled.emit(False)

    def _first(self): self.doc.current_frame = 0; self._emit(); self._upd()
    def _last(self): self.doc.current_frame = self.doc.total_frames - 1; self._emit(); self._upd()
    def _prev(self):
        if self.doc.current_frame > 0: self.doc.current_frame -= 1
        self._emit(); self._upd()
    def _next(self):
        if self.doc.current_frame < self.doc.total_frames - 1: self.doc.current_frame += 1
        self._emit(); self._upd()
    def _add_frame(self):
        self.doc.add_frame(); self._emit(); self._upd()
    def _dup_frame(self):
        self.doc.dup_frame(); self._emit(); self._upd()
    def _del_frame(self):
        self.doc.del_frame(); self._emit(); self._upd()
    def _clear(self):
        self.doc.clear_frame(); self._emit(); self._upd()

    def _set_fps(self, v):
        self.doc.fps = v
        if self.doc.playing:
            self._timer.setInterval(int(1000 / v))

    def _emit(self):
        self.frame_changed.emit(self.doc.current_frame)

    def _upd(self):
        self._frm_lbl.setText(f"Frame {self.doc.current_frame + 1} / {self.doc.total_frames}")


# ═══════════════════════════════════════════════════════
# MAIN WINDOW
# ═══════════════════════════════════════════════════════

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Free Animation Power v0.2 — Prototype")
        self.resize(1700, 950)

        self.doc = Document()
        self.brush = BrushEngine()

        # Widgets
        self.canvas = Canvas(self.doc, self.brush)
        self.tool_panel = ToolPanel(self.brush)
        self.layer_panel = LayerPanel(self.doc)
        self.timeline = TimelineWidget(self.doc)
        self.timeline_ctrl = TimelineControls(self.doc)

        self._setup_ui()
        self._connect()
        self.canvas.fit()

    def _setup_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main = QVBoxLayout(central)
        main.setContentsMargins(0, 0, 0, 0)
        main.setSpacing(0)

        # Toolbar superior
        toolbar = QToolBar()
        toolbar.setMovable(False)
        new_act = toolbar.addAction("New")
        new_act.triggered.connect(self._new_project)
        save_act = toolbar.addAction("Save")
        save_act.triggered.connect(self._save)
        export_act = toolbar.addAction("Export PNG")
        export_act.triggered.connect(self._export)
        toolbar.addSeparator()
        fit_act = toolbar.addAction("Fit (F)")
        fit_act.triggered.connect(self.canvas.fit)
        flip_act = toolbar.addAction("Flip H (H)")
        flip_act.triggered.connect(self._toggle_flip)
        rot_act = toolbar.addAction("Rotate (R)")
        rot_act.triggered.connect(self._do_rotate)
        grid_act = toolbar.addAction("Grid (')")
        grid_act.triggered.connect(self._toggle_grid)
        self.addToolBar(toolbar)

        # Content area
        content = QHBoxLayout()
        content.setSpacing(0)

        # Left panel: tools + layers
        left = QWidget()
        left.setFixedWidth(220)
        left.setStyleSheet("background: #353538;")
        left_lay = QVBoxLayout(left)
        left_lay.setContentsMargins(0, 0, 0, 0)
        left_lay.setSpacing(4)
        scroll = QScrollArea()
        scroll.setWidget(self.tool_panel)
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("QScrollArea { border: none; }")
        left_lay.addWidget(scroll)
        left_lay.addWidget(self.layer_panel)
        content.addWidget(left)

        # Center: Canvas
        content.addWidget(self.canvas, 1)

        main.addLayout(content, 1)

        # Timeline area
        btn_area = QWidget()
        btn_area.setStyleSheet("background: #3a3a3d;")
        btn_lay = QVBoxLayout(btn_area)
        btn_lay.setContentsMargins(0, 0, 0, 0)
        btn_lay.setSpacing(0)
        btn_lay.addWidget(self.timeline_ctrl)
        btn_lay.addWidget(self.timeline)
        main.addWidget(btn_area)

        # Status bar
        self.statusBar().showMessage(
            "Ready | B:Brush E:Eraser I:Pick G:Fill L:Line U:Rect Y:Ellipse M:Move H:Hand | "
            "Ctrl+Z:Undo Ctrl+Y:Redo | F:Fit R:Rotate H:Flip | Space:Play | Arrows:Prev/Next Frame | Wheel:Zoom"
        )

    def _connect(self):
        self.tool_panel.tool_changed.connect(self._set_canvas_tool)
        self.tool_panel.color_changed.connect(lambda c: setattr(self.canvas, 'color', c))
        self.tool_panel.settings_changed.connect(self.canvas.update)

        self.canvas.canvas_updated.connect(self._on_canvas_updated)

        self.timeline.frame_changed.connect(self._on_frame)
        self.timeline_ctrl.frame_changed.connect(self._on_frame)
        self.layer_panel.layer_changed.connect(lambda i: self.canvas.update())

    def _on_canvas_updated(self):
        self.timeline.update()
        self.timeline_ctrl._upd()

    def _set_canvas_tool(self, tool: Tool):
        self.canvas.tool = tool

    def _toggle_flip(self):
        self.canvas._flip_h = not self.canvas._flip_h
        self.canvas.update()

    def _do_rotate(self):
        self.canvas._rot = (self.canvas._rot + 15) % 360
        self.canvas.update()

    def _toggle_grid(self):
        self.doc.show_grid = not self.doc.show_grid
        if self.doc.show_grid and self.doc.grid_size == 0:
            self.doc.grid_size = 64  # default grid size
        self.canvas.update()

    def _on_frame(self, fi: int):
        self.timeline.update()
        self.canvas.update()

    def _new_project(self):
        reply = QMessageBox.question(self, "New Project",
                                     "Start a new project? Unsaved work will be lost.",
                                     QMessageBox.Yes | QMessageBox.No)
        if reply != QMessageBox.Yes:
            return
        self.doc = Document()
        self.canvas.doc = self.doc
        self.canvas._drawing = False
        self.canvas._stroke_pts = []
        self.timeline.doc = self.doc
        self.timeline_ctrl.doc = self.doc
        self.timeline_ctrl._stop()
        self.layer_panel.doc = self.doc
        self.layer_panel._build()
        self.canvas.fit()
        self.timeline.update()
        self.canvas.update()

    def _save(self):
        path, _ = QFileDialog.getSaveFileName(self, "Save Project", "project.fap.json",
                                              "FAP (*.fap.json)")
        if not path:
            return
        base = os.path.dirname(path)
        name = os.path.splitext(os.path.basename(path))[0]
        fdir = os.path.join(base, f"{name}_frames")
        os.makedirs(fdir, exist_ok=True)
        for li, layer in enumerate(self.doc.layers):
            for fi in layer.frames:
                fname = os.path.join(fdir, f"L{li:02d}_F{fi:04d}.png")
                layer.frames[fi].save(fname)
        import json
        meta = {"canvas_w": self.doc.cw, "canvas_h": self.doc.ch, "fps": self.doc.fps,
                "total_frames": self.doc.total_frames, "layers": [l.name for l in self.doc.layers]}
        with open(path, 'w') as f:
            json.dump(meta, f, indent=2)
        self.statusBar().showMessage(f"Project saved: {fdir}")

    def _export(self):
        path, _ = QFileDialog.getSaveFileName(self, "Export Frame", "frame_001.png",
                                              "PNG (*.png);;JPEG (*.jpg)")
        if not path:
            return
        self.doc.cur_image().save(path)
        self.statusBar().showMessage(f"Exported: {path}")


# ═══════════════════════════════════════════════════════
# ENTRY POINT
# ═══════════════════════════════════════════════════════

def main():
    app = QApplication(sys.argv)
    app.setApplicationName("Free Animation Power")
    app.setStyle("Fusion")

    # Dark theme
    from PySide6.QtGui import QPalette
    pal = QPalette()
    pal.setColor(QPalette.Window, QColor(42, 42, 45))
    pal.setColor(QPalette.WindowText, QColor(220, 220, 220))
    pal.setColor(QPalette.Base, QColor(28, 28, 30))
    pal.setColor(QPalette.AlternateBase, QColor(42, 42, 45))
    pal.setColor(QPalette.Text, QColor(220, 220, 220))
    pal.setColor(QPalette.Button, QColor(52, 52, 55))
    pal.setColor(QPalette.ButtonText, QColor(220, 220, 220))
    pal.setColor(QPalette.Highlight, QColor(212, 120, 42))
    pal.setColor(QPalette.HighlightedText, QColor(255, 255, 255))
    app.setPalette(pal)

    # Global stylesheet
    app.setStyleSheet("""
        QToolBar { background: #353538; border-bottom: 1px solid #555; spacing: 4px; }
        QToolBar QToolButton { background: #4a4a4d; color: #ccc; border: 1px solid #555; padding: 4px 10px; border-radius: 3px; }
        QToolBar QToolButton:hover { background: #5a5a5d; }
        QMenuBar { background: #353538; color: #ccc; border-bottom: 1px solid #555; }
        QMenuBar::item:selected { background: #d4782a; }
        QMenu { background: #353538; color: #ccc; border: 1px solid #555; }
        QMenu::item:selected { background: #d4782a; }
        QStatusBar { background: #2a2a2d; color: #999; font-size: 9px; }
        QScrollArea { border: none; }
    """)

    win = MainWindow()
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
