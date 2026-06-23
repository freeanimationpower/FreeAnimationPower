"""
UI panels: ToolPanel and LayerPanel.
"""

import json
import os

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QColor, QImage
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QPushButton, QSlider, QLabel, QSpinBox, QDoubleSpinBox,
    QComboBox, QCheckBox, QColorDialog, QButtonGroup,
    QToolButton, QGroupBox, QScrollArea, QFileDialog,
)

from .types_enums import Tool, BrushShape, BlendMode, CANVAS_W, CANVAS_H
from .brush_engine import BrushEngine
from .document import Document


class ToolPanel(QWidget):
    tool_changed = Signal(Tool)
    color_changed = Signal(QColor)
    settings_changed = Signal()
    shape_fill_changed = Signal(bool)
    canvas_resize_requested = Signal(int, int)

    def __init__(self, brush_eng: BrushEngine):
        super().__init__()
        self.brush = brush_eng
        self.palette: list[QColor] = []
        self._swatches: list[QPushButton] = []
        self.setMinimumWidth(200)
        self.setMaximumWidth(240)

        self.setStyleSheet("""
            QGroupBox { color: #ccc; font-weight: bold; font-size: 10px; border: 1px solid #555; border-radius: 4px; margin-top: 12px; padding-top: 16px; }
            QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 6px; }
            QToolButton { background: #3a3a3d; color: #ddd; border: 1px solid #555; border-radius: 3px; font-size: 14px; }
            QToolButton:checked { background: #d4782a; color: #fff; border-color: #e09040; }
            QToolButton:hover { background: #555; }
            QToolButton:hover:checked { background: #e09040; }
            QSlider::groove:horizontal { background: #333; height: 6px; border-radius: 3px; }
            QSlider::handle:horizontal { background: #d4782a; width: 14px; margin: -4px 0; border-radius: 7px; }
            QSlider::sub-page:horizontal { background: #d4782a; border-radius: 3px; }
            QLabel { color: #bbb; font-size: 10px; }
            QSpinBox, QDoubleSpinBox, QComboBox { background: #333; color: #ddd; border: 1px solid #555; padding: 2px 4px; font-size: 10px; }
            QCheckBox { color: #bbb; font-size: 10px; }
            QPushButton { background: #4a4a4d; color: #ccc; border: 1px solid #555; padding: 4px 8px; border-radius: 3px; font-size: 10px; }
            QPushButton:hover { background: #5a5a5d; }
        """)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(4)

        self._build_tool_buttons(layout)
        self._build_color_section(layout)
        self._build_brush_section(layout)
        self._build_shape_section(layout)

        layout.addStretch()

    def _build_tool_buttons(self, parent_layout):
        grp = QGroupBox("TOOLS")
        grid = QGridLayout(grp)
        grid.setSpacing(4)

        tools = [
            (Tool.BRUSH,       "Brush",       "B", "🖌"),
            (Tool.ERASER,      "Eraser",      "E", "◼"),
            (Tool.EYEDROPPER,  "Eyedropper",  "I", "💧"),
            (Tool.BUCKET,      "Paint Bucket","G", "🪣"),
            (Tool.LINE,        "Line",        "L", "╱"),
            (Tool.RECT,        "Rectangle",   "U", "▬"),
            (Tool.ELLIPSE,     "Ellipse",     "Y", "◯"),
            (Tool.MOVE,        "Move",        "M", "↕"),
            (Tool.SELECT_RECT, "Rect Select", "S", "⬚"),
            (Tool.HAND,        "Hand / Pan",  "H", "✋"),
        ]

        self._btn_grp = QButtonGroup(self)
        self._btn_grp.setExclusive(True)
        for i, (tid, name, key, icon) in enumerate(tools):
            btn = QToolButton()
            btn.setText(icon)
            btn.setToolTip(f"{name} ({key})")
            btn.setCheckable(True)
            btn.setFixedSize(42, 38)
            btn.setToolButtonStyle(Qt.ToolButtonStyle.ToolButtonIconOnly)
            btn.clicked.connect(lambda checked, t=tid: self.tool_changed.emit(t))
            self._btn_grp.addButton(btn, int(tid))
            grid.addWidget(btn, i // 2, i % 2)
        self._btn_grp.button(int(Tool.BRUSH)).setChecked(True)

        parent_layout.addWidget(grp)

    def set_active_tool(self, tool: Tool):
        btn = self._btn_grp.button(int(tool))
        if btn:
            btn.setChecked(True)

    def _build_color_section(self, parent_layout):
        grp = QGroupBox("COLOR")
        v = QVBoxLayout(grp)
        v.setSpacing(4)

        self._clr_btn = QPushButton()
        self._clr_btn.setFixedSize(80, 40)
        self._clr_btn.setStyleSheet(
            f"background: {self.brush.cfg.color.name()}; border: 2px solid #777; border-radius: 4px;"
        )
        self._clr_btn.clicked.connect(self._pick_color)
        v.addWidget(self._clr_btn, alignment=Qt.AlignmentFlag.AlignCenter)

        self._palette_grid = QGridLayout()
        self._palette_grid.setSpacing(2)
        v.addLayout(self._palette_grid)

        pal_row = QHBoxLayout()
        pal_row.setSpacing(3)
        add_sw = QPushButton("+")
        add_sw.setFixedSize(28, 20)
        add_sw.setToolTip("Add current color to palette")
        add_sw.clicked.connect(self._add_swatch)
        pal_row.addWidget(add_sw)
        load_pal = QPushButton("Load")
        load_pal.setFixedHeight(20)
        load_pal.clicked.connect(self._load_palette)
        pal_row.addWidget(load_pal)
        save_pal = QPushButton("Save")
        save_pal.setFixedHeight(20)
        save_pal.clicked.connect(self._save_palette)
        pal_row.addWidget(save_pal)
        pal_row.addStretch()
        v.addLayout(pal_row)

        parent_layout.addWidget(grp)

    def update_color_btn(self, color: QColor):
        self._clr_btn.setStyleSheet(
            f"background: {color.name()}; border: 2px solid #777; border-radius: 4px;"
        )

    def _add_swatch(self):
        color = QColor(self.brush.cfg.color)
        self.palette.append(color)
        self._refresh_swatches()

    def _remove_swatch(self, idx: int):
        if 0 <= idx < len(self.palette):
            del self.palette[idx]
            self._refresh_swatches()

    def _refresh_swatches(self):
        for btn in self._swatches:
            btn.deleteLater()
        self._swatches.clear()

        while self._palette_grid.count():
            item = self._palette_grid.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

        cols = 7
        for i, color in enumerate(self.palette):
            btn = QPushButton()
            btn.setFixedSize(22, 22)
            btn.setToolTip(f"Click: select | Right-click: remove\n{color.name()}")
            btn.setStyleSheet(
                f"background: {color.name()}; border: 1px solid #666; border-radius: 2px;"
            )
            lidx = i
            btn.clicked.connect(lambda checked, c=color: self._select_swatch(c))
            btn.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
            btn.customContextMenuRequested.connect(
                lambda pos, idx=lidx: self._remove_swatch(idx)
            )
            self._swatches.append(btn)
            self._palette_grid.addWidget(btn, i // cols, i % cols)

    def _select_swatch(self, color: QColor):
        self.brush.cfg.color = QColor(color)
        self._clr_btn.setStyleSheet(
            f"background: {color.name()}; border: 2px solid #777; border-radius: 4px;"
        )
        self.color_changed.emit(color)

    def _save_palette(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "Save Palette", "palette.gpl",
            "GIMP Palette (*.gpl);;JSON Palette (*.json)"
        )
        if not path:
            return
        if path.lower().endswith('.gpl'):
            with open(path, 'w') as f:
                f.write("GIMP Palette\n")
                f.write(f"Name: {os.path.splitext(os.path.basename(path))[0]}\n")
                f.write("Columns: 0\n#\n")
                for c in self.palette:
                    f.write(f"{c.red():3d} {c.green():3d} {c.blue():3d} Untitled\n")
        elif path.lower().endswith('.json'):
            data = [[c.red(), c.green(), c.blue()] for c in self.palette]
            with open(path, 'w') as f:
                json.dump(data, f, indent=2)

    def _load_palette(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Load Palette", "",
            "Palette Files (*.gpl *.json);;All Files (*)"
        )
        if not path:
            return
        self.palette = []
        try:
            if path.lower().endswith('.gpl'):
                with open(path, 'r') as f:
                    for line in f:
                        line = line.strip()
                        if not line or line.startswith('#') or line.startswith('GIMP') or \
                           line.startswith('Name:') or line.startswith('Columns:'):
                            continue
                        parts = line.split()
                        if len(parts) >= 3:
                            try:
                                r, g, b = int(parts[0]), int(parts[1]), int(parts[2])
                                self.palette.append(QColor(r, g, b))
                            except ValueError:
                                pass
            elif path.lower().endswith('.json'):
                with open(path, 'r') as f:
                    data = json.load(f)
                    for item in data:
                        self.palette.append(QColor(item[0], item[1], item[2]))
        except Exception:
            pass
        self._refresh_swatches()

    def _build_brush_section(self, parent_layout):
        cfg = self.brush.cfg
        grp = QGroupBox("BRUSH")
        v = QVBoxLayout(grp)
        v.setSpacing(3)

        # Size
        row = QHBoxLayout()
        row.addWidget(QLabel("Sz"))
        sz = QSlider(Qt.Orientation.Horizontal)
        sz.setRange(1, 300)
        sz.setValue(int(cfg.size))
        sz.valueChanged.connect(lambda v: self._update_cfg('size', float(v)))
        row.addWidget(sz)
        self._sz_lbl = QLabel(f"{int(cfg.size)}")
        self._sz_lbl.setFixedWidth(30)
        row.addWidget(self._sz_lbl)
        v.addLayout(row)
        self._sz_slider = sz

        # Min size
        row = QHBoxLayout()
        row.addWidget(QLabel("Min"))
        msz = QSlider(Qt.Orientation.Horizontal)
        msz.setRange(0, 100)
        msz.setValue(int(cfg.min_size))
        msz.valueChanged.connect(lambda v: self._update_cfg('min_size', float(v)))
        row.addWidget(msz)
        self._msz_lbl = QLabel(f"{int(cfg.min_size)}")
        self._msz_lbl.setFixedWidth(30)
        row.addWidget(self._msz_lbl)
        v.addLayout(row)

        # Opacity
        row = QHBoxLayout()
        row.addWidget(QLabel("Op"))
        op = QSlider(Qt.Orientation.Horizontal)
        op.setRange(1, 100)
        op.setValue(int(cfg.opacity * 100))
        op.valueChanged.connect(lambda v: self._update_cfg('opacity', v / 100.0))
        row.addWidget(op)
        self._op_lbl = QLabel(f"{int(cfg.opacity * 100)}%")
        self._op_lbl.setFixedWidth(30)
        row.addWidget(self._op_lbl)
        v.addLayout(row)

        # Hardness
        row = QHBoxLayout()
        row.addWidget(QLabel("Hd"))
        hd = QSlider(Qt.Orientation.Horizontal)
        hd.setRange(0, 100)
        hd.setValue(int(cfg.hardness * 100))
        hd.valueChanged.connect(lambda v: self._update_cfg('hardness', v / 100.0))
        row.addWidget(hd)
        self._hd_lbl = QLabel(f"{int(cfg.hardness * 100)}%")
        self._hd_lbl.setFixedWidth(30)
        row.addWidget(self._hd_lbl)
        v.addLayout(row)

        # Stabilizer
        row = QHBoxLayout()
        row.addWidget(QLabel("St"))
        sb = QSlider(Qt.Orientation.Horizontal)
        sb.setRange(0, 20)
        sb.setValue(cfg.stabilizer)
        sb.valueChanged.connect(lambda v: self._update_cfg('stabilizer', v))
        row.addWidget(sb)
        self._sb_lbl = QLabel(str(cfg.stabilizer))
        self._sb_lbl.setFixedWidth(20)
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

        # Dynamics
        row = QHBoxLayout()
        ps = QCheckBox("P.Size")
        ps.setChecked(cfg.use_pressure_size)
        ps.toggled.connect(lambda v: self._update_cfg('use_pressure_size', v))
        row.addWidget(ps)
        po = QCheckBox("P.Opac")
        po.setChecked(cfg.use_pressure_opacity)
        po.toggled.connect(lambda v: self._update_cfg('use_pressure_opacity', v))
        row.addWidget(po)
        v.addLayout(row)

        # Fill shapes
        sf = QCheckBox("Fill shapes")
        sf.setChecked(False)
        sf.toggled.connect(lambda v: self.shape_fill_changed.emit(v))
        v.addWidget(sf)

        parent_layout.addWidget(grp)

    def _build_shape_section(self, parent_layout):
        grp = QGroupBox("CANVAS")
        v = QVBoxLayout(grp)
        v.setSpacing(3)
        row = QHBoxLayout()
        row.addWidget(QLabel("W"))
        cw = QSpinBox()
        cw.setRange(64, 8192)
        cw.setValue(CANVAS_W)
        cw.setFixedWidth(65)
        row.addWidget(cw)
        row.addWidget(QLabel("H"))
        ch = QSpinBox()
        ch.setRange(64, 8192)
        ch.setValue(CANVAS_H)
        ch.setFixedWidth(65)
        row.addWidget(ch)
        v.addLayout(row)
        btn = QPushButton("Resize Canvas")
        btn.clicked.connect(lambda: self.canvas_resize_requested.emit(cw.value(), ch.value()))
        v.addWidget(btn)
        parent_layout.addWidget(grp)

    def _update_cfg(self, attr: str, value):
        setattr(self.brush.cfg, attr, value)
        cfg = self.brush.cfg
        if attr == 'size':
            self._sz_lbl.setText(f"{int(value)}")
        elif attr == 'min_size':
            self._msz_lbl.setText(f"{int(value)}")
        elif attr == 'opacity':
            self._op_lbl.setText(f"{int(value * 100):.0f}%")
        elif attr == 'hardness':
            self._hd_lbl.setText(f"{int(value * 100)}%")
        elif attr == 'stabilizer':
            self._sb_lbl.setText(str(int(value)))
        self.settings_changed.emit()

    def _pick_color(self):
        color = QColorDialog.getColor(self.brush.cfg.color, self, "Pick Color")
        if color.isValid():
            self.brush.cfg.color = color
            self._clr_btn.setStyleSheet(
                f"background: {color.name()}; border: 2px solid #777; border-radius: 4px;"
            )
            self.color_changed.emit(color)


class LayerPanel(QWidget):
    layer_changed = Signal(int)

    BLEND_NAMES = ["Normal", "Multiply", "Screen", "Overlay", "Add",
                   "Darken", "Lighten", "ColorBurn", "ColorDodge",
                   "SoftLight", "HardLight"]

    def __init__(self, doc: Document):
        super().__init__()
        self.doc = doc
        self.setMinimumWidth(180)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)
        layout.setSpacing(4)

        title = QLabel("LAYERS")
        title.setStyleSheet("color: #999; font-weight: bold; font-size: 9px; letter-spacing: 2px;")
        layout.addWidget(title)

        self._list_widget = QWidget()
        self._list_area = QVBoxLayout(self._list_widget)
        self._list_area.setSpacing(1)
        self._list_area.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self._list_widget, 1)

        row = QHBoxLayout()
        row.setSpacing(3)
        add = QPushButton("+")
        add.setFixedWidth(28)
        add.setToolTip("Add layer")
        add.clicked.connect(self._add)
        row.addWidget(add)
        rm = QPushButton("-")
        rm.setFixedWidth(28)
        rm.setToolTip("Delete selected layer")
        rm.clicked.connect(self._remove)
        row.addWidget(rm)
        up_btn = QPushButton("▲")
        up_btn.setFixedWidth(28)
        up_btn.setToolTip("Move layer up")
        up_btn.clicked.connect(self._move_up)
        row.addWidget(up_btn)
        down_btn = QPushButton("▼")
        down_btn.setFixedWidth(28)
        down_btn.setToolTip("Move layer down")
        down_btn.clicked.connect(self._move_down)
        row.addWidget(down_btn)
        dup_btn = QPushButton("Dup")
        dup_btn.setFixedWidth(32)
        dup_btn.setToolTip("Duplicate layer")
        dup_btn.clicked.connect(self._duplicate)
        row.addWidget(dup_btn)
        row.addStretch()
        layout.addLayout(row)

        self._prop_widget = QWidget()
        self._prop_area = QVBoxLayout(self._prop_widget)
        self._prop_area.setSpacing(2)
        self._prop_area.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self._prop_widget)

        self.setStyleSheet("""
            QPushButton { background: #4a4a4d; color: #ccc; border: 1px solid #555; padding: 2px 4px; border-radius: 3px; font-size: 10px; }
            QPushButton:hover { background: #5a5a5d; }
            QLabel { color: #ccc; font-size: 10px; }
            QComboBox { background: #333; color: #ddd; border: 1px solid #555; padding: 1px 3px; font-size: 9px; }
            QDoubleSpinBox { background: #333; color: #ddd; border: 1px solid #555; padding: 1px 3px; font-size: 9px; }
        """)

        self._build()

    def _build(self):
        while self._list_area.count():
            item = self._list_area.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

        for i, layer in enumerate(self.doc.layers):
            row = QWidget()
            if i == self.doc.current_layer:
                row.setStyleSheet("background: #3a5060; border-radius: 2px;")
            else:
                row.setStyleSheet("background: transparent; border-radius: 2px;")
            rl = QHBoxLayout(row)
            rl.setContentsMargins(3, 1, 3, 1)
            rl.setSpacing(3)

            vis = QCheckBox()
            vis.setChecked(layer.visible)
            vis.setFixedSize(14, 14)
            vis.toggled.connect(lambda v, li=i: self._toggle_vis(li, v))
            rl.addWidget(vis)

            lbl = QLabel(layer.name[:14])
            lbl.setStyleSheet(f"color: {'#fff' if i == self.doc.current_layer else '#ccc'}; font-size: 10px;")
            rl.addWidget(lbl)
            rl.addStretch()

            row.mousePressEvent = lambda ev, idx=i: self._select(idx)
            self._list_area.addWidget(row)

        self._build_properties()

    def _build_properties(self):
        while self._prop_area.count():
            item = self._prop_area.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

        layer = self.doc.cur_layer

        # Blend mode
        row = QHBoxLayout()
        row.setSpacing(3)
        row.addWidget(QLabel("Blend"))
        bm = QComboBox()
        bm.addItems(self.BLEND_NAMES)
        bm.setCurrentIndex(int(layer.blend_mode))
        bm.currentIndexChanged.connect(lambda i: self._set_blend(i))
        row.addWidget(bm)
        self._prop_area.addLayout(row)

        # Layer opacity
        row = QHBoxLayout()
        row.setSpacing(3)
        row.addWidget(QLabel("Opac"))
        op = QSlider(Qt.Orientation.Horizontal)
        op.setRange(0, 100)
        op.setValue(int(layer.opacity * 100))
        op.valueChanged.connect(lambda v: self._set_opacity(v / 100.0))
        row.addWidget(op)
        self._prop_area.addLayout(row)

        # Offset X / Y
        row = QHBoxLayout()
        row.setSpacing(3)
        row.addWidget(QLabel("X"))
        ox = QDoubleSpinBox()
        ox.setRange(-8192, 8192)
        ox.setValue(layer.offset_x)
        ox.setDecimals(1)
        ox.valueChanged.connect(lambda v: self._set_prop('offset_x', v))
        row.addWidget(ox)
        row.addWidget(QLabel("Y"))
        oy = QDoubleSpinBox()
        oy.setRange(-8192, 8192)
        oy.setValue(layer.offset_y)
        oy.setDecimals(1)
        oy.valueChanged.connect(lambda v: self._set_prop('offset_y', v))
        row.addWidget(oy)
        self._prop_area.addLayout(row)

        # Rotation / Scale
        row = QHBoxLayout()
        row.setSpacing(3)
        row.addWidget(QLabel("Rot"))
        rr = QDoubleSpinBox()
        rr.setRange(-360, 360)
        rr.setValue(layer.rotation)
        rr.setDecimals(1)
        rr.valueChanged.connect(lambda v: self._set_prop('rotation', v))
        row.addWidget(rr)
        row.addWidget(QLabel("Scl"))
        ss = QDoubleSpinBox()
        ss.setRange(0.01, 100)
        ss.setValue(layer.scale_x)
        ss.setDecimals(2)
        ss.setSingleStep(0.1)
        ss.valueChanged.connect(lambda v: self._set_prop('scale_x', v))
        ss.valueChanged.connect(lambda v: self._set_prop('scale_y', v))
        row.addWidget(ss)
        self._prop_area.addLayout(row)

    def _set_blend(self, idx: int):
        self.doc.cur_layer.blend_mode = BlendMode(idx)
        self.layer_changed.emit(self.doc.current_layer)

    def _set_opacity(self, v: float):
        self.doc.cur_layer.opacity = v
        self.layer_changed.emit(self.doc.current_layer)

    def _set_prop(self, attr: str, v: float):
        setattr(self.doc.cur_layer, attr, v)
        self.layer_changed.emit(self.doc.current_layer)

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
        if len(self.doc.layers) <= 1:
            return
        self.doc.remove_layer(self.doc.current_layer)
        self._build()
        self.layer_changed.emit(self.doc.current_layer)

    def _duplicate(self):
        src = self.doc.cur_layer
        from .types_enums import LayerData
        new_layer = LayerData(name=f"{src.name} copy")
        new_layer.visible = src.visible
        new_layer.opacity = src.opacity
        new_layer.blend_mode = src.blend_mode
        new_layer.offset_x = src.offset_x
        new_layer.offset_y = src.offset_y
        new_layer.scale_x = src.scale_x
        new_layer.scale_y = src.scale_y
        new_layer.rotation = src.rotation
        for fi, img in src.frames.items():
            new_layer.frames[fi] = QImage(img) if img else None
        idx = self.doc.current_layer + 1
        self.doc.layers.insert(idx, new_layer)
        self.doc.current_layer = idx
        self._build()
        self.layer_changed.emit(self.doc.current_layer)

    def _move_up(self):
        idx = self.doc.current_layer
        if idx > 0:
            self.doc.layers[idx], self.doc.layers[idx - 1] = self.doc.layers[idx - 1], self.doc.layers[idx]
            self.doc.current_layer = idx - 1
            self._build()
            self.layer_changed.emit(self.doc.current_layer)

    def _move_down(self):
        idx = self.doc.current_layer
        if idx < len(self.doc.layers) - 1:
            self.doc.layers[idx], self.doc.layers[idx + 1] = self.doc.layers[idx + 1], self.doc.layers[idx]
            self.doc.current_layer = idx + 1
            self._build()
            self.layer_changed.emit(self.doc.current_layer)
