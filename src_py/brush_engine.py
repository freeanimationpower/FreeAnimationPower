"""
Brush engine - vector-based soft stroke rendering.
"""

from PySide6.QtCore import Qt, QPointF, QLineF
from PySide6.QtGui import (
    QImage, QPainter, QPen, QBrush, QColor,
    QPainterPath, QRadialGradient,
)

from .types_enums import BrushCfg, BrushShape, StrokePoint


class BrushEngine:
    def __init__(self):
        self.cfg = BrushCfg()

    def create_pen(self, pressure: float) -> QPen:
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

        if cfg.shape == BrushShape.ROUND:
            return QPen(color, size, Qt.PenStyle.SolidLine,
                        Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin)
        elif cfg.shape == BrushShape.SQUARE:
            return QPen(color, size, Qt.PenStyle.SolidLine,
                        Qt.PenCapStyle.SquareCap, Qt.PenJoinStyle.MiterJoin)
        elif cfg.shape in (BrushShape.FLAT, BrushShape.CALLIGRAPHY):
            sz = int(size * 2) + 8
            if sz < 8: sz = 8
            if sz % 2 == 0: sz += 1
            brush_img = QImage(sz, sz, QImage.Format_ARGB32_Premultiplied)
            brush_img.fill(Qt.GlobalColor.transparent)
            bp = QPainter(brush_img)
            bp.setRenderHint(QPainter.RenderHint.Antialiasing)
            bp.translate(sz / 2, sz / 2)
            angle_offset = 45 if cfg.shape == BrushShape.CALLIGRAPHY else 0
            bp.rotate(cfg.angle + angle_offset)
            scale_x = 1.0
            scale_y = max(0.15, cfg.roundness) if cfg.shape == BrushShape.FLAT else 0.25
            bp.scale(scale_x, scale_y)
            grad = QRadialGradient(0, 0, size * 0.7)
            h = cfg.hardness
            c0 = QColor(color); c0.setAlpha(alpha)
            c2 = QColor(color); c2.setAlpha(0)
            grad.setColorAt(0, c0)
            grad.setColorAt(h, c0)
            grad.setColorAt(1.0, c2)
            bp.setBrush(QBrush(grad))
            bp.setPen(Qt.PenStyle.NoPen)
            bp.drawEllipse(QPointF(0, 0), size * 0.7, size * 0.12)
            bp.end()
            return QPen(QBrush(brush_img), size, Qt.PenStyle.SolidLine,
                        Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin)
        else:
            return QPen(color, size, Qt.PenStyle.SolidLine,
                        Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin)

    def create_eraser_pen(self, pressure: float) -> QPen:
        size = self.cfg.min_size + (self.cfg.size - self.cfg.min_size) * pressure
        if size < 0.5:
            size = 0.5
        return QPen(Qt.GlobalColor.white, size, Qt.PenStyle.SolidLine,
                    Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin)

    def build_path(self, points: list[StrokePoint]) -> QPainterPath:
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

    def build_variable_path(self, points: list[StrokePoint]) -> list[tuple[QPainterPath, float]]:
        segments: list[tuple[QPainterPath, float]] = []
        if len(points) < 2:
            if len(points) == 1:
                p = points[0]
                path = QPainterPath()
                sz = self.cfg.size * p.pressure * 0.5
                path.addEllipse(QPointF(p.x, p.y), max(0.5, sz), max(0.5, sz))
                segments.append((path, p.pressure))
            return segments

        chunk = 15
        overlap = 5
        i = 0
        while i < len(points) - 1:
            end = min(i + chunk, len(points))
            sub = points[i:end]
            avg_pressure = sum(p.pressure for p in sub) / len(sub)

            subpath = QPainterPath()
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
            if end >= len(points):
                break
            i = end - overlap

        return segments
