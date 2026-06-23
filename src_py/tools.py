"""
Drawing tools: flood fill, shape tools, selection tools.
"""

import sys
import traceback

from PySide6.QtCore import Qt, QPointF, QRectF, QLineF
from PySide6.QtGui import (
    QImage, QPainter, QPen, QBrush, QColor,
    QPainterPath,
)

from .types_enums import Tool, UndoCmd, StrokePoint
from .document import Document
from .brush_engine import BrushEngine


def flood_fill(img: QImage, x: int, y: int, fill_color: QColor, tolerance: int = 30):
    """Scanline flood fill. Works on a copy, then copies back to original."""
    w, h = img.width(), img.height()
    if x < 0 or x >= w or y < 0 or y >= h:
        return

    target = img.pixelColor(x, y)
    if target == fill_color:
        return

    tr, tg, tb, ta = target.red(), target.green(), target.blue(), target.alpha()

    filled = QImage(img)
    visited = bytearray(w * h)
    stack: list[tuple[int, int]] = [(x, y)]

    while stack:
        px, py = stack.pop()
        if visited[py * w + px]:
            continue
        if px < 0 or px >= w or py < 0 or py >= h:
            continue

        c = filled.pixelColor(px, py)
        if (abs(c.red() - tr) > tolerance or abs(c.green() - tg) > tolerance or
                abs(c.blue() - tb) > tolerance or abs(c.alpha() - ta) > tolerance):
            continue

        visited[py * w + px] = 1

        lx = px
        while lx >= 0:
            lidx = py * w + lx
            if visited[lidx]:
                break
            c = filled.pixelColor(lx, py)
            if (abs(c.red() - tr) <= tolerance and abs(c.green() - tg) <= tolerance and
                    abs(c.blue() - tb) <= tolerance and abs(c.alpha() - ta) <= tolerance):
                filled.setPixelColor(lx, py, fill_color)
                visited[lidx] = 1
                if py > 0 and not visited[(py - 1) * w + lx]:
                    stack.append((lx, py - 1))
                if py < h - 1 and not visited[(py + 1) * w + lx]:
                    stack.append((lx, py + 1))
                lx -= 1
            else:
                break

        rx = px + 1
        while rx < w:
            ridx = py * w + rx
            if visited[ridx]:
                break
            c = filled.pixelColor(rx, py)
            if (abs(c.red() - tr) <= tolerance and abs(c.green() - tg) <= tolerance and
                    abs(c.blue() - tb) <= tolerance and abs(c.alpha() - ta) <= tolerance):
                filled.setPixelColor(rx, py, fill_color)
                visited[ridx] = 1
                if py > 0 and not visited[(py - 1) * w + rx]:
                    stack.append((rx, py - 1))
                if py < h - 1 and not visited[(py + 1) * w + rx]:
                    stack.append((rx, py + 1))
                rx += 1
            else:
                break

    p = QPainter(img)
    p.setCompositionMode(QPainter.CompositionMode.CompositionMode_Source)
    p.drawImage(0, 0, filled)
    p.end()


def draw_shape_on_image(img: QImage, brush: BrushEngine, shape_start: QPointF,
                        shape_end: QPointF, tool: Tool, fill_enabled: bool = False):
    """Draw a line, rect, or ellipse onto the image."""
    p = QPainter(img)
    p.setRenderHint(QPainter.RenderHint.Antialiasing)
    pen = brush.create_pen(1.0)
    p.setPen(pen)
    x1, y1 = shape_start.x(), shape_start.y()
    x2, y2 = shape_end.x(), shape_end.y()

    if fill_enabled and tool in (Tool.RECT, Tool.ELLIPSE):
        p.setBrush(QBrush(brush.cfg.color))

    if tool == Tool.LINE:
        p.drawLine(QPointF(x1, y1), QPointF(x2, y2))
    elif tool == Tool.RECT:
        r = QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized()
        p.drawRect(r)
    elif tool == Tool.ELLIPSE:
        r = QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized()
        p.drawEllipse(r)
    p.end()
