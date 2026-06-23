"""
Timeline widget and playback controls.
"""

import os

from PySide6.QtCore import Qt, QTimer, Signal, QRectF, QUrl
from PySide6.QtGui import (
    QPainter, QPen, QColor, QFont, QMouseEvent, QWheelEvent,
    QImage,
)
from PySide6.QtWidgets import (
    QWidget, QHBoxLayout, QVBoxLayout,
    QPushButton, QLabel, QSpinBox, QFileDialog,
)
from PySide6.QtMultimedia import QMediaPlayer, QAudioOutput

from .document import Document


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
        self.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self._thumb_cache: dict[tuple[int, int], QImage] = {}

    def invalidate_cache(self):
        self._thumb_cache.clear()

    def paintEvent(self, ev):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.fillRect(self.rect(), QColor(48, 48, 50))

        total = self.doc.total_frames
        cur = self.doc.current_frame
        x0 = 8 - self._scroll

        for i in range(total):
            x = x0 + i * (self._fw + 4)
            if x + self._fw < 0 or x > self.width():
                continue

            rect = QRectF(x, 6, self._fw, self._fh)
            if i == cur:
                p.setPen(QPen(QColor(212, 120, 42), 2.5))
                p.fillRect(rect, QColor(58, 58, 68))
            else:
                p.setPen(QPen(QColor(75, 75, 75), 1))
                p.fillRect(rect, QColor(42, 42, 46))

            p.drawRect(rect)

            p.setPen(QColor(180, 180, 180))
            p.setFont(QFont("Consolas", 9))
            p.drawText(QRectF(x + 3, 8, self._fw - 6, 16),
                       Qt.AlignmentFlag.AlignLeft, f"#{i + 1}")

            thumb_rect = QRectF(x + 3, 24, self._fw - 6, self._fh - 26)
            self._draw_thumb(p, i, thumb_rect)
            p.setPen(QPen(QColor(55, 55, 55), 0.5))
            p.drawRect(thumb_rect)

        p.setPen(QColor(120, 120, 120))
        p.setFont(QFont("Consolas", 8))
        p.drawText(8, self.height() - 6,
                   "Click frame to select | Scroll to navigate | Use controls below")

        p.end()

    def _draw_thumb(self, p: QPainter, fi: int, rect: QRectF):
        tw, th = int(rect.width()), int(rect.height())
        if tw <= 0 or th <= 0:
            return

        cache_key = (fi, tw)
        if cache_key in self._thumb_cache:
            p.drawImage(rect.topLeft(), self._thumb_cache[cache_key])
            return

        for layer in self.doc.layers:
            if fi in layer.frames and layer.visible:
                img = layer.frames[fi]
                scaled = img.scaled(tw, th, Qt.AspectRatioMode.IgnoreAspectRatio,
                                    Qt.TransformationMode.SmoothTransformation)
                self._thumb_cache[cache_key] = scaled
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
        self.setFocusPolicy(Qt.FocusPolicy.NoFocus)

        self._audio_player = QMediaPlayer()
        self._audio_output = QAudioOutput()
        self._audio_player.setAudioOutput(self._audio_output)
        self._audio_output.setVolume(self.doc.audio_volume)

        self.setStyleSheet("""
            QPushButton { background: #4a4a4d; color: #ccc; border: 1px solid #555; padding: 4px 8px; border-radius: 3px; font-size: 11px; }
            QPushButton:hover { background: #5a5a5d; }
            QSpinBox { background: #333; color: #ddd; border: 1px solid #555; padding: 2px 4px; }
            QLabel { color: #ccc; font-size: 10px; }
        """)

        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 4, 8, 4)
        layout.setSpacing(5)

        layout.addWidget(self._btn("|<", self._first))
        layout.addWidget(self._btn("<", self._prev))
        self._play_btn = QPushButton(">")
        self._play_btn.setFixedWidth(35)
        self._play_btn.setStyleSheet(
            "font-weight: bold; font-size: 14px; background: #d4782a; color: #fff;"
        )
        self._play_btn.clicked.connect(self._toggle)
        layout.addWidget(self._play_btn)
        layout.addWidget(self._btn(">", self._next))
        layout.addWidget(self._btn(">|", self._last))

        layout.addSpacing(8)

        self._frm_lbl = QLabel(f"Frame {self.doc.current_frame + 1} / {self.doc.total_frames}")
        self._frm_lbl.setStyleSheet("color: #ddd; font-weight: bold;")
        layout.addWidget(self._frm_lbl)

        layout.addStretch()

        layout.addWidget(self._btn("+ Frame", self._add_frame))
        layout.addWidget(self._btn("Dup Frame", self._dup_frame))
        layout.addWidget(self._btn("Del Frame", self._del_frame))
        layout.addWidget(self._btn("Clear Frame", self._clear))

        layout.addSpacing(10)

        layout.addWidget(QLabel("FPS:"))
        fps = QSpinBox()
        fps.setRange(1, 60)
        fps.setValue(self.doc.fps)
        fps.valueChanged.connect(self._set_fps)
        fps.setFixedWidth(48)
        layout.addWidget(fps)
        self._fps_spin = fps

        layout.addSpacing(8)
        layout.addWidget(QLabel("Onion:"))
        layout.addWidget(QLabel("Prev"))
        op = QSpinBox()
        op.setRange(0, 5)
        op.setValue(self.doc.onion_prev)
        op.setFixedWidth(36)
        op.valueChanged.connect(lambda v: setattr(self.doc, 'onion_prev', v))
        layout.addWidget(op)
        layout.addWidget(QLabel("Next"))
        on = QSpinBox()
        on.setRange(0, 5)
        on.setValue(self.doc.onion_next)
        on.setFixedWidth(36)
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
            self._start_audio()
            self.play_toggled.emit(True)

    def _stop(self):
        self._timer.stop()
        self.doc.playing = False
        self._play_btn.setText(">")
        self._audio_player.pause()
        self.play_toggled.emit(False)

    def _start_audio(self):
        if self.doc.audio_path and os.path.isfile(self.doc.audio_path):
            if self._audio_player.playbackState() != QMediaPlayer.PlaybackState.PlayingState:
                self._audio_player.setSource(QUrl.fromLocalFile(self.doc.audio_path))
                offset_ms = int(self.doc.audio_offset * 1000 / max(1, self.doc.fps))
                self._audio_player.setPosition(offset_ms)
                self._audio_player.play()

    def import_audio(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Import Audio", "",
            "Audio Files (*.wav *.mp3 *.ogg *.flac);;All Files (*)"
        )
        if path and os.path.isfile(path):
            self.doc.audio_path = path
            return True
        return False

    def _first(self):
        self.doc.current_frame = 0
        self._emit()
        self._upd()

    def _last(self):
        self.doc.current_frame = self.doc.total_frames - 1
        self._emit()
        self._upd()

    def _prev(self):
        if self.doc.current_frame > 0:
            self.doc.current_frame -= 1
        self._emit()
        self._upd()

    def _next(self):
        if self.doc.current_frame < self.doc.total_frames - 1:
            self.doc.current_frame += 1
        self._emit()
        self._upd()

    def _add_frame(self):
        self.doc.add_frame()
        self._emit()
        self._upd()

    def _dup_frame(self):
        self.doc.dup_frame()
        self._emit()
        self._upd()

    def _del_frame(self):
        self.doc.del_frame()
        self._emit()
        self._upd()

    def _clear(self):
        self.doc.clear_frame()
        self._emit()
        self._upd()

    def _set_fps(self, v):
        self.doc.fps = v
        if self.doc.playing:
            self._timer.setInterval(int(1000 / v))

    def _emit(self):
        self.frame_changed.emit(self.doc.current_frame)

    def _upd(self):
        self._frm_lbl.setText(
            f"Frame {self.doc.current_frame + 1} / {self.doc.total_frames}"
        )
