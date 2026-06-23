"""
Animation document model.
"""

from PySide6.QtCore import QSize, Qt
from PySide6.QtGui import QImage, QColor, QPainter

from .types_enums import (
    CANVAS_W, CANVAS_H, DEFAULT_FPS, MAX_UNDO,
    LayerData, UndoCmd, UndoStack,
)


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
        self.grid_size = 0
        self.show_grid = False
        self._project_path = None
        self._frames_dir = None
        self.audio_path: str | None = None
        self.audio_volume: float = 1.0
        self.audio_offset: int = 0  # frames offset

    @property
    def size(self) -> QSize:
        return QSize(self.cw, self.ch)

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

    def resize_canvas(self, new_w: int, new_h: int):
        old_size = self.size
        self.cw = new_w
        self.ch = new_h
        new_size = self.size
        for layer in self.layers:
            for fi in list(layer.frames.keys()):
                old_img = layer.frames[fi]
                new_img = QImage(new_size, QImage.Format_ARGB32_Premultiplied)
                new_img.fill(Qt.GlobalColor.transparent)
                p = QPainter(new_img)
                p.drawImage(0, 0, old_img)
                p.end()
                layer.frames[fi] = new_img
