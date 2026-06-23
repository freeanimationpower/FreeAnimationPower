"""
Main canvas widget for drawing and viewing the animation.
"""

import sys
import traceback

from PySide6.QtCore import (
    Qt, QPointF, QRectF, QSize, QPoint, QLineF, Signal,
)
from PySide6.QtGui import (
    QImage, QPainter, QPen, QBrush, QColor, QPixmap,
    QTabletEvent, QMouseEvent, QWheelEvent, QKeyEvent,
    QPainterPath, QFont, QAction, QTransform,
    QPainterPath, QCursor,
)
from PySide6.QtWidgets import (
    QWidget, QSizePolicy,
)

from .types_enums import Tool, UndoCmd, StrokePoint, TabletState, BlendMode

BLEND_TO_COMPOSITION = {
    BlendMode.NORMAL: QPainter.CompositionMode.CompositionMode_SourceOver,
    BlendMode.MULTIPLY: QPainter.CompositionMode.CompositionMode_Multiply,
    BlendMode.SCREEN: QPainter.CompositionMode.CompositionMode_Screen,
    BlendMode.OVERLAY: QPainter.CompositionMode.CompositionMode_Overlay,
    BlendMode.ADD: QPainter.CompositionMode.CompositionMode_Plus,
    BlendMode.DARKEN: QPainter.CompositionMode.CompositionMode_Darken,
    BlendMode.LIGHTEN: QPainter.CompositionMode.CompositionMode_Lighten,
    BlendMode.COLOR_BURN: QPainter.CompositionMode.CompositionMode_ColorBurn,
    BlendMode.COLOR_DODGE: QPainter.CompositionMode.CompositionMode_ColorDodge,
    BlendMode.SOFT_LIGHT: QPainter.CompositionMode.CompositionMode_SoftLight,
    BlendMode.HARD_LIGHT: QPainter.CompositionMode.CompositionMode_HardLight,
}
from .document import Document
from .brush_engine import BrushEngine
from .tools import flood_fill, draw_shape_on_image


class Canvas(QWidget):
    canvas_updated = Signal()
    color_picked = Signal(QColor)
    tool_changed_by_key = Signal(Tool)

    def __init__(self, doc: Document, brush_eng: BrushEngine):
        super().__init__()
        self.doc = doc
        self.brush = brush_eng
        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        self.setAttribute(Qt.WidgetAttribute.WA_TabletTracking, True)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.setMinimumSize(400, 300)

        # Viewport
        self._zoom = 1.0
        self._off_x = 0.0
        self._off_y = 0.0
        self._rot = 0
        self._flip_h = False
        self._show_rulers = True
        self._ruler_size = 20  # px for ruler strip

        # Input
        self.tab = TabletState()
        self._drawing = False
        self._tool = Tool.BRUSH
        self._color = QColor(0, 0, 0, 255)
        self._stroke_pts: list[StrokePoint] = []
        self._raw_pts: list[StrokePoint] = []
        self._before_img: QImage | None = None
        self._stroke_path: QPainterPath = QPainterPath()
        self._last_stroke_size: float = 10.0

        # Hand tool
        self._panning = False
        self._pan_start = QPointF()
        self._pan_off_start = QPointF()

        # Selection tool
        self._sel_start: QPointF | None = None
        self._sel_rect: QRectF | None = None
        self._sel_image: QImage | None = None
        self._moving_selection = False
        self._move_start: QPointF | None = None
        self._clipboard: QImage | None = None

        # Shape tools
        self._shape_start: QPointF | None = None
        self._shape_end: QPointF | None = None
        self._shape_fill: bool = False

        # Cursor overlay
        self._show_cursor = True
        self.setCursor(Qt.CursorShape.CrossCursor)

    # ── Properties ──────────────────────────────
    @property
    def zoom(self):
        return self._zoom

    @zoom.setter
    def zoom(self, z):
        self._zoom = max(0.02, min(64.0, z))
        self.update()

    @property
    def tool(self):
        return self._tool

    @tool.setter
    def tool(self, t: Tool):
        self._tool = t
        if t == Tool.HAND:
            self.setCursor(Qt.CursorShape.OpenHandCursor)
        elif t in (Tool.EYEDROPPER, Tool.MOVE):
            self.setCursor(Qt.CursorShape.ArrowCursor)
        elif t in (Tool.SELECT_RECT, Tool.SELECT_LASSO):
            self.setCursor(Qt.CursorShape.CrossCursor)
        elif t == Tool.BUCKET:
            self.setCursor(Qt.CursorShape.CrossCursor)
        else:
            self.setCursor(Qt.CursorShape.CrossCursor)
        self.update()

    @property
    def color(self):
        return self._color

    @color.setter
    def color(self, c: QColor):
        self._color = c
        self.brush.cfg.color = c

    # ── Coordinate transforms ──────────────────
    def _to_canvas(self, sp: QPointF) -> QPointF:
        return (sp - QPointF(self._off_x, self._off_y)) / self._zoom

    def _to_screen(self, cp: QPointF) -> QPointF:
        return cp * self._zoom + QPointF(self._off_x, self._off_y)

    # ── Tablet events ──────────────────────────
    def tabletEvent(self, ev: QTabletEvent):
        ev.accept()
        self.tab.is_tablet = True
        self.tab.pressure = ev.pressure()
        self.tab.tilt_x = ev.xTilt()
        self.tab.tilt_y = ev.yTilt()
        self.tab.rotation = ev.rotation()
        self.tab.pos = ev.position()
        self.tab.is_erasing = (ev.pointerType() == QTabletEvent.PointerType.Eraser)

        t = ev.type()
        if t == QTabletEvent.Type.TabletPress:
            self._input_press(ev.position())
        elif t == QTabletEvent.Type.TabletMove:
            if self._drawing:
                self._input_move(ev.position())
        elif t == QTabletEvent.Type.TabletRelease:
            if self._drawing:
                self._input_release()

        self.update()

    # ── Mouse events ───────────────────────────
    def mousePressEvent(self, ev: QMouseEvent):
        if self.tab.is_tablet and not self._drawing:
            self.tab.is_tablet = False

        if self.tab.is_tablet:
            return

        self.tab.pressure = 1.0
        self.tab.pos = ev.position()

        if ev.button() == Qt.MouseButton.MiddleButton:
            self._panning = True
            self._pan_start = ev.position()
            self._pan_off_start = QPointF(self._off_x, self._off_y)
            self.setCursor(Qt.CursorShape.ClosedHandCursor)
            return
        if ev.button() == Qt.MouseButton.LeftButton:
            self._input_press(ev.position())

    def mouseMoveEvent(self, ev: QMouseEvent):
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
            self.setCursor(self._cursor_for_tool())
            return
        if self._drawing:
            self._input_release()

    def wheelEvent(self, ev: QWheelEvent):
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
        k = ev.key()
        ctrl = bool(ev.modifiers() & Qt.KeyboardModifier.ControlModifier)
        shift = bool(ev.modifiers() & Qt.KeyboardModifier.ShiftModifier)

        if ctrl and k == Qt.Key.Key_Z:
            self._undo()
        elif ctrl and (k == Qt.Key.Key_Y or (k == Qt.Key.Key_Z and shift)):
            self._redo()
        elif ctrl and k == Qt.Key.Key_C:
            self._copy_selection()
        elif ctrl and k == Qt.Key.Key_V:
            self._paste_selection()
        elif ctrl and k == Qt.Key.Key_0:
            self._zoom = 1.0
            self._off_x = 0
            self._off_y = 0
            self._rot = 0
            self.fit()
            self.update()
        elif ctrl and k == Qt.Key.Key_S:
            self.canvas_updated.emit()
        elif k == Qt.Key.Key_B:
            self.tool = Tool.BRUSH; self.tool_changed_by_key.emit(Tool.BRUSH)
        elif k == Qt.Key.Key_E:
            self.tool = Tool.ERASER; self.tool_changed_by_key.emit(Tool.ERASER)
        elif k == Qt.Key.Key_I:
            self.tool = Tool.EYEDROPPER; self.tool_changed_by_key.emit(Tool.EYEDROPPER)
        elif k == Qt.Key.Key_G:
            self.tool = Tool.BUCKET; self.tool_changed_by_key.emit(Tool.BUCKET)
        elif k == Qt.Key.Key_L:
            self.tool = Tool.LINE; self.tool_changed_by_key.emit(Tool.LINE)
        elif k == Qt.Key.Key_U:
            self.tool = Tool.RECT; self.tool_changed_by_key.emit(Tool.RECT)
        elif k == Qt.Key.Key_Y and not ctrl:
            self.tool = Tool.ELLIPSE; self.tool_changed_by_key.emit(Tool.ELLIPSE)
        elif k == Qt.Key.Key_M:
            self.tool = Tool.MOVE; self.tool_changed_by_key.emit(Tool.MOVE)
        elif k == Qt.Key.Key_S and not ctrl:
            self.tool = Tool.SELECT_RECT; self.tool_changed_by_key.emit(Tool.SELECT_RECT)
        elif k == Qt.Key.Key_H:
            self._flip_h = not self._flip_h
            self.update()
        elif k == Qt.Key.Key_R and not ctrl:
            self._rot = (self._rot + 15) % 360
            self.update()
        elif k == Qt.Key.Key_Apostrophe:
            self.doc.show_grid = not self.doc.show_grid
            if self.doc.show_grid and self.doc.grid_size == 0:
                self.doc.grid_size = 64
            self.update()
        elif k == Qt.Key.Key_Left:
            self.doc.current_frame = max(0, self.doc.current_frame - 1)
            self._clear_selection()
            self.canvas_updated.emit()
        elif k == Qt.Key.Key_Right:
            self.doc.current_frame = min(self.doc.total_frames - 1, self.doc.current_frame + 1)
            self._clear_selection()
            self.canvas_updated.emit()
        elif k == Qt.Key.Key_Delete:
            self.doc.clear_frame()
            self._clear_selection()
            self.canvas_updated.emit()
        elif k == Qt.Key.Key_F:
            self.fit()
        elif k == Qt.Key.Key_Escape:
            self._clear_selection()
            self.update()

    # ── Input logic ────────────────────────────
    def _cursor_for_tool(self):
        if self._tool == Tool.HAND:
            return Qt.CursorShape.OpenHandCursor
        return Qt.CursorShape.CrossCursor

    def _get_pressure(self) -> float:
        p = self.tab.pressure if self.tab.is_tablet else 1.0
        return max(0.01, min(1.0, p))

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

        self._drawing = True
        self._stroke_pts = []
        self._raw_pts = []
        self._stroke_path = QPainterPath()
        pressure = self._get_pressure()
        try:
            self._before_img = self.doc.snap()
        except Exception:
            self._before_img = None
        pt = StrokePoint(x=cpos.x(), y=cpos.y(), pressure=pressure)
        self._stroke_pts.append(pt)
        self._stroke_path.moveTo(cpos.x(), cpos.y())

    def _input_move(self, screen_pos: QPointF):
        cpos = self._to_canvas(screen_pos)
        if not self._drawing:
            self.update()
            return

        if self._tool == Tool.MOVE:
            if self._moving_selection and self._sel_image and self._move_start:
                delta = cpos - self._move_start
                self._sel_rect = QRectF(delta.x(), delta.y(),
                                        self._sel_image.width(), self._sel_image.height())
                self.update()
            return

        if self._tool == Tool.SELECT_RECT:
            self._sel_rect = QRectF(self._sel_start, cpos).normalized()
            self.update()
            return

        if self._tool in (Tool.LINE, Tool.RECT, Tool.ELLIPSE):
            self._shape_end = cpos
            self.update()
            return

        pressure = self._get_pressure()
        last = self._stroke_pts[-1] if self._stroke_pts else None

        if last:
            dist = QLineF(QPointF(last.x, last.y), cpos).length()
            min_spacing = self.brush.cfg.size * self.brush.cfg.spacing * 0.3
            if dist < max(0.5, min_spacing) and self.brush.cfg.stabilizer == 0:
                return

        pt = StrokePoint(x=cpos.x(), y=cpos.y(), pressure=pressure)

        if self.brush.cfg.stabilizer > 0:
            self._raw_pts.append(pt)
            window = min(self.brush.cfg.stabilizer + 1, len(self._raw_pts))
            recent = self._raw_pts[-window:]
            avg_x = sum(p.x for p in recent) / len(recent)
            avg_y = sum(p.y for p in recent) / len(recent)
            avg_p = sum(p.pressure for p in recent) / len(recent)
            pt = StrokePoint(x=avg_x, y=avg_y, pressure=avg_p)
            self._stroke_pts.append(pt)
        else:
            self._stroke_pts.append(pt)

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

        if self._tool == Tool.MOVE:
            if self._sel_image and self._sel_rect:
                self._commit_move()
            self._moving_selection = False
            self.update()
            return

        if self._tool == Tool.SELECT_RECT:
            if self._sel_rect and self._sel_rect.width() > 1 and self._sel_rect.height() > 1:
                self._capture_selection()
            self.update()
            return

        if self._tool in (Tool.LINE, Tool.RECT, Tool.ELLIPSE):
            if self._shape_start and self._shape_end:
                self._draw_shape()
            self._shape_start = None
            self._shape_end = None
            self.update()
            return

        if len(self._stroke_pts) > 0:
            try:
                self._commit_stroke()
            except Exception as e:
                print(f"Error committing stroke: {e}", file=sys.stderr)
                traceback.print_exc()

        self._stroke_pts.clear()
        self._raw_pts.clear()
        self._stroke_path = QPainterPath()
        self._before_img = None
        self.canvas_updated.emit()

        self.tab.is_tablet = False
        self.update()

    # ── Tool actions ───────────────────────────
    def _do_eyedropper(self, sp: QPointF):
        cpos = self._to_canvas(sp)
        img = self.doc.cur_image()
        x, y = int(cpos.x()), int(cpos.y())
        if 0 <= x < img.width() and 0 <= y < img.height():
            c = img.pixelColor(x, y)
            if c.alpha() > 5:
                self._color = c
                self.brush.cfg.color = c
                self.color_picked.emit(c)
                self.canvas_updated.emit()

    def _do_fill(self, cpos: QPointF):
        img = self.doc.cur_image()
        x, y = int(cpos.x()), int(cpos.y())
        if 0 <= x < img.width() and 0 <= y < img.height():
            before = self.doc.snap()
            flood_fill(img, x, y, self._color, tolerance=32)
            after = self.doc.snap()
            self.doc.undo.push(UndoCmd(self.doc.current_layer, self.doc.current_frame, before, after))
            self.canvas_updated.emit()

    def _draw_shape(self):
        if not self._shape_start or not self._shape_end:
            return
        before = self.doc.snap()
        img = self.doc.cur_image()
        draw_shape_on_image(img, self.brush, self._shape_start, self._shape_end,
                            self._tool, fill_enabled=self._shape_fill)
        after = self.doc.snap()
        self.doc.undo.push(UndoCmd(self.doc.current_layer, self.doc.current_frame, before, after))

    def _commit_stroke(self):
        if len(self._stroke_pts) < 1:
            return

        img = self.doc.cur_image()
        p = QPainter(img)
        p.setRenderHint(QPainter.RenderHint.Antialiasing, True)

        avg_pressure = sum(pt.pressure for pt in self._stroke_pts) / len(self._stroke_pts)

        if self._tool == Tool.ERASER or self.tab.is_erasing:
            pen = self.brush.create_eraser_pen(avg_pressure)
            p.setCompositionMode(QPainter.CompositionMode.CompositionMode_DestinationOut)
        else:
            pen = self.brush.create_pen(avg_pressure)
            p.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)

        p.setPen(pen)
        path = self.brush.build_path(self._stroke_pts)
        p.drawPath(path)
        p.end()

        after = self.doc.snap()
        if self._before_img is not None:
            self.doc.undo.push(UndoCmd(self.doc.current_layer, self.doc.current_frame,
                                        self._before_img, after))

    # ── Selection operations ───────────────────
    def _capture_selection(self):
        if not self._sel_rect:
            return
        img = self.doc.cur_image()
        r = self._sel_rect.toAlignedRect()
        r = r.intersected(img.rect())
        if r.width() > 0 and r.height() > 0:
            self._sel_image = img.copy(r)
            self._sel_rect = QRectF(0, 0, r.width(), r.height())
            self._sel_start = QPointF(r.x(), r.y())

    def _commit_move(self):
        if not self._sel_image or not self._sel_rect or not self._sel_start:
            return
        before = self.doc.snap()
        img = self.doc.cur_image()
        p = QPainter(img)
        p.setCompositionMode(QPainter.CompositionMode.CompositionMode_Source)
        p.fillRect(QRectF(self._sel_start, QSize(self._sel_image.width(),
                          self._sel_image.height())), Qt.GlobalColor.transparent)
        p.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)
        dest_x = self._sel_start.x() + self._sel_rect.x()
        dest_y = self._sel_start.y() + self._sel_rect.y()
        p.drawImage(QPointF(dest_x, dest_y), self._sel_image)
        p.end()
        after = self.doc.snap()
        self.doc.undo.push(UndoCmd(self.doc.current_layer, self.doc.current_frame, before, after))
        self._sel_image = None
        self._sel_rect = None
        self._sel_start = None
        self.canvas_updated.emit()

    def _copy_selection(self):
        if self._sel_image:
            self._clipboard = QImage(self._sel_image)

    def _paste_selection(self):
        if self._clipboard:
            self._sel_image = QImage(self._clipboard)
            self._sel_rect = QRectF(0, 0, self._clipboard.width(), self._clipboard.height())
            self._sel_start = QPointF(0, 0)
            self._moving_selection = False
            self.update()

    def _clear_selection(self):
        self._sel_image = None
        self._sel_rect = None
        self._sel_start = None
        self._moving_selection = False

    # ── Undo/Redo ──────────────────────────────
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

    # ── Paint ──────────────────────────────────
    def paintEvent(self, ev):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform)

        p.fillRect(self.rect(), QColor(40, 40, 42))

        cr = QRectF(0, 0, self.doc.cw, self.doc.ch)

        p.save()
        # Viewport transform
        p.translate(self._off_x, self._off_y)
        p.scale(self._zoom, self._zoom)

        # Canvas rotation/flip around center
        if self._rot != 0 or self._flip_h:
            cx = cr.center()
            p.translate(cx.x(), cx.y())
            if self._rot != 0:
                p.rotate(self._rot)
            if self._flip_h:
                p.scale(-1.0, 1.0)
            p.translate(-cx.x(), -cx.y())

        # Clip to canvas bounds for clean edges
        p.setClipRect(cr)

        self._paint_checker(p, cr)
        p.fillRect(cr, self.doc.bg_color)

        if self.doc.show_grid and self.doc.grid_size > 0:
            self._paint_grid(p, cr)

        # Onion skin: previous frames (red) - only when not playing
        if not self.doc.playing:
            for i in range(1, self.doc.onion_prev + 1):
                fi = self.doc.current_frame - i
                if fi < 0:
                    break
                a = int(255 * self.doc.onion_opacity * (1.0 - i / (self.doc.onion_prev + 1.5)))
                self._render_frame_composite(p, fi, QColor(255, 60, 60, a))

            # Onion skin: next frames (blue) - only when not playing
            for i in range(1, self.doc.onion_next + 1):
                fi = self.doc.current_frame + i
                if fi >= self.doc.total_frames:
                    break
                a = int(255 * self.doc.onion_opacity * (1.0 - i / (self.doc.onion_next + 1.5)))
                self._render_frame_composite(p, fi, QColor(60, 100, 255, a))

        # Current frame layers with blend modes and transforms
        for layer in self.doc.layers:
            if not layer.visible:
                continue
            frame = layer.get(self.doc.current_frame, self.doc.size)
            p.save()
            p.setOpacity(layer.opacity)
            comp_mode = BLEND_TO_COMPOSITION.get(layer.blend_mode,
                                                  QPainter.CompositionMode.CompositionMode_SourceOver)
            p.setCompositionMode(comp_mode)

            # Apply layer transform
            if layer.offset_x != 0 or layer.offset_y != 0 or layer.rotation != 0 or layer.scale_x != 1.0 or layer.scale_y != 1.0:
                cx = frame.width() / 2
                cy = frame.height() / 2
                p.translate(cx + layer.offset_x, cy + layer.offset_y)
                if layer.rotation != 0:
                    p.rotate(layer.rotation)
                if layer.scale_x != 1.0 or layer.scale_y != 1.0:
                    p.scale(layer.scale_x, layer.scale_y)
                p.translate(-cx, -cy)

            p.drawImage(QPointF(0, 0), frame)
            p.restore()
        p.setOpacity(1.0)

        # Live stroke preview
        if self._drawing and self._tool in (Tool.BRUSH, Tool.ERASER) and len(self._stroke_pts) > 1:
            self._render_live_stroke(p)

        # Shape preview
        if self._shape_start and self._shape_end and self._tool in (Tool.LINE, Tool.RECT, Tool.ELLIPSE):
            self._render_shape_preview(p)

        # Selection rectangle
        if self._sel_rect and not self._moving_selection:
            p.setPen(QPen(QColor(60, 140, 255), 1.5 / self._zoom, Qt.PenStyle.DashLine))
            p.setBrush(QBrush(QColor(60, 140, 255, 30)))
            if self._sel_start:
                draw_rect = QRectF(self._sel_start.x() + self._sel_rect.x(),
                                   self._sel_start.y() + self._sel_rect.y(),
                                   self._sel_rect.width(), self._sel_rect.height())
                p.drawRect(draw_rect)
            else:
                p.drawRect(self._sel_rect)

        # Selection image dragged
        if self._moving_selection and self._sel_image and self._sel_rect:
            dest_x = (self._sel_start.x() + self._sel_rect.x()) if self._sel_start else self._sel_rect.x()
            dest_y = (self._sel_start.y() + self._sel_rect.y()) if self._sel_start else self._sel_rect.y()
            p.setOpacity(0.7)
            p.drawImage(QPointF(dest_x, dest_y), self._sel_image)
            p.setOpacity(1.0)

        p.restore()

        # Canvas border (screen coords)
        sr = QRectF(self._off_x, self._off_y,
                    cr.width() * self._zoom,
                    cr.height() * self._zoom)
        p.setPen(QPen(QColor(80, 80, 80), 1))
        p.drawRect(sr)

        # Rulers
        if self._show_rulers:
            self._paint_rulers(p, cr)

        # Brush cursor overlay
        if (self._show_cursor and not self._drawing and not self._panning and
                self._tool in (Tool.BRUSH, Tool.ERASER)):
            cpos = self._to_canvas(self.tab.pos)
            sz = self.brush.cfg.size * self._zoom
            scx = self._off_x + cpos.x() * self._zoom
            scy = self._off_y + cpos.y() * self._zoom
            p.setPen(QPen(QColor(255, 255, 255, 160), 1.5))
            p.setBrush(QBrush(QColor(180, 180, 180, 30)))
            p.drawEllipse(QPointF(scx, scy), sz / 2, sz / 2)

        # Info overlay
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
        p.setPen(QPen(QColor(160, 160, 160, 100), 1.0 / self._zoom))
        x0, y0 = int(rect.x()), int(rect.y())
        x1, y1 = int(rect.right()), int(rect.bottom())
        for x in range(gs * (x0 // gs), x1, gs):
            p.drawLine(QPointF(x, y0), QPointF(x, y1))
        for y in range(gs * (y0 // gs), y1, gs):
            p.drawLine(QPointF(x0, y), QPointF(x1, y))

    def _paint_rulers(self, p: QPainter, cr: QRectF):
        rs = self._ruler_size
        p.save()
        p.setFont(QFont("Consolas", 7))

        # Horizontal ruler background
        ruler_y = self._off_y - rs if self._off_y >= 0 else self._off_y
        h_rect = QRectF(max(0, self._off_x), max(0, ruler_y),
                        min(self.width(), cr.width() * self._zoom), rs)
        p.fillRect(h_rect, QColor(48, 48, 50))
        p.setPen(QPen(QColor(100, 100, 100), 1))
        p.drawRect(h_rect)

        # Horizontal tick marks
        p.setPen(QColor(160, 160, 160))
        step = max(1, int(100 / self._zoom))  # tick every ~100px
        x0 = int(-self._off_x / self._zoom / step) * step
        x1 = int((self.width() - self._off_x) / self._zoom)
        for cx in range(x0, x1 + step, step):
            sx = self._off_x + cx * self._zoom
            if 0 <= sx <= self.width():
                tick_h = rs * 0.4 if cx % (step * 5) == 0 else rs * 0.25
                p.drawLine(QPointF(sx, h_rect.bottom()),
                           QPointF(sx, h_rect.bottom() - tick_h))
                if cx % (step * 5) == 0:
                    p.drawText(QRectF(sx - 20, h_rect.top(), 40, rs),
                               Qt.AlignmentFlag.AlignCenter, str(cx))

        # Vertical ruler background
        ruler_x = self._off_x - rs if self._off_x >= 0 else self._off_x
        v_rect = QRectF(max(0, ruler_x), max(0, self._off_y),
                        rs, min(self.height(), cr.height() * self._zoom))
        p.fillRect(v_rect, QColor(48, 48, 50))
        p.setPen(QPen(QColor(100, 100, 100), 1))
        p.drawRect(v_rect)

        # Vertical tick marks
        y0 = int(-self._off_y / self._zoom / step) * step
        y1 = int((self.height() - self._off_y) / self._zoom)
        for cy in range(y0, y1 + step, step):
            sy = self._off_y + cy * self._zoom
            if 0 <= sy <= self.height():
                tick_w = rs * 0.4 if cy % (step * 5) == 0 else rs * 0.25
                p.drawLine(QPointF(v_rect.right(), sy),
                           QPointF(v_rect.right() - tick_w, sy))
                if cy % (step * 5) == 0:
                    p.drawText(QRectF(v_rect.left(), sy - 6, rs - 4, 12),
                               Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter, str(cy))

        p.restore()

    def _render_frame_composite(self, p: QPainter, fi: int, tint: QColor):
        combined = QImage(self.doc.cw, self.doc.ch, QImage.Format_ARGB32_Premultiplied)
        combined.fill(Qt.GlobalColor.transparent)
        cp = QPainter(combined)
        for layer in self.doc.layers:
            if not layer.visible:
                continue
            img = layer.get(fi, self.doc.size)
            cp.setOpacity(layer.opacity)
            cp.drawImage(0, 0, img)
        cp.end()

        tinted = QImage(combined.size(), QImage.Format_ARGB32_Premultiplied)
        tinted.fill(tint)
        tp = QPainter(tinted)
        tp.setCompositionMode(QPainter.CompositionMode.CompositionMode_DestinationIn)
        tp.drawImage(0, 0, combined)
        tp.end()
        p.drawImage(0, 0, tinted)

    def _render_live_stroke(self, p: QPainter):
        if len(self._stroke_pts) < 2:
            return
        p.save()
        avg_pressure = sum(pt.pressure for pt in self._stroke_pts) / len(self._stroke_pts)

        if self._tool == Tool.ERASER or self.tab.is_erasing:
            pen = self.brush.create_eraser_pen(avg_pressure)
            p.setCompositionMode(QPainter.CompositionMode.CompositionMode_DestinationOut)
        else:
            pen = self.brush.create_pen(avg_pressure)
        p.setPen(pen)
        path = self.brush.build_path(self._stroke_pts)
        p.drawPath(path)
        p.restore()

    def _render_shape_preview(self, p: QPainter):
        p.setPen(QPen(QColor(60, 140, 255, 180), 2.0 / self._zoom, Qt.PenStyle.DashLine))
        x1, y1 = self._shape_start.x(), self._shape_start.y()
        x2, y2 = self._shape_end.x(), self._shape_end.y()
        if self._tool == Tool.LINE:
            p.drawLine(QPointF(x1, y1), QPointF(x2, y2))
        elif self._tool == Tool.RECT:
            p.drawRect(QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized())
        elif self._tool == Tool.ELLIPSE:
            p.drawEllipse(QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized())

    def _paint_info_overlay(self, p: QPainter):
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
        if self.width() == 0 or self.height() == 0:
            return
        zw = self.width() / self.doc.cw
        zh = self.height() / self.doc.ch
        self._zoom = min(zw, zh) * 0.80
        self._off_x = (self.width() - self.doc.cw * self._zoom) / 2
        self._off_y = (self.height() - self.doc.ch * self._zoom) / 2
        self.update()

    def resizeEvent(self, ev):
        super().resizeEvent(ev)
        self.fit()
