"""
Types, enums, and dataclasses for Free Animation Power.
"""

from enum import IntEnum
from dataclasses import dataclass, field

from PySide6.QtCore import Qt, QPointF, QSize
from PySide6.QtGui import QImage, QColor

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


class BlendMode(IntEnum):
    NORMAL = 0
    MULTIPLY = 1
    SCREEN = 2
    OVERLAY = 3
    ADD = 4
    DARKEN = 5
    LIGHTEN = 6
    COLOR_BURN = 7
    COLOR_DODGE = 8
    SOFT_LIGHT = 9
    HARD_LIGHT = 10


# ═══════════════════════════════════════════════════════
# DATACLASSES
# ═══════════════════════════════════════════════════════


@dataclass
class StrokePoint:
    x: float = 0.0
    y: float = 0.0
    pressure: float = 1.0
    tilt_x: float = 0.0
    tilt_y: float = 0.0
    rotation: float = 0.0
    timestamp: float = 0.0


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
    roundness: float = 1.0
    angle: float = 0.0
    stabilizer: int = 0


@dataclass
class LayerData:
    name: str = "Layer"
    visible: bool = True
    opacity: float = 1.0
    locked: bool = False
    blend_mode: BlendMode = BlendMode.NORMAL
    offset_x: float = 0.0
    offset_y: float = 0.0
    scale_x: float = 1.0
    scale_y: float = 1.0
    rotation: float = 0.0  # degrees
    frames: dict = field(default_factory=dict)

    def get(self, idx: int, size: QSize) -> QImage:
        if idx not in self.frames or self.frames[idx] is None:
            img = QImage(size, QImage.Format_ARGB32_Premultiplied)
            img.fill(Qt.GlobalColor.transparent)
            self.frames[idx] = img
        return self.frames[idx]

    def transform_matrix(self):
        """Returns QTransform for this layer's offset/scale/rotation."""
        from PySide6.QtGui import QTransform
        t = QTransform()
        if self.offset_x != 0 or self.offset_y != 0:
            t.translate(self.offset_x, self.offset_y)
        if self.rotation != 0:
            t.rotate(self.rotation)
        if self.scale_x != 1.0 or self.scale_y != 1.0:
            t.scale(self.scale_x, self.scale_y)
        return t


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

    def can_undo(self) -> bool:
        return self._pos >= 0

    def can_redo(self) -> bool:
        return self._pos + 1 < len(self._data)
