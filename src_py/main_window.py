"""
Main window - assembles all panels and handles project I/O.
"""

import os
import json
import zipfile
import io

from PySide6.QtCore import Qt, QSize
from PySide6.QtGui import QAction, QColor, QImage, QPainter
from PySide6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QScrollArea, QToolBar, QFileDialog, QMessageBox,
    QStatusBar,
)

from .types_enums import Tool
from .document import Document
from .brush_engine import BrushEngine
from .canvas import Canvas
from .panels import ToolPanel, LayerPanel
from .timeline import TimelineWidget, TimelineControls


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Free Animation Power v0.3 — Prototype")
        self.resize(1700, 950)

        self.doc = Document()
        self.brush = BrushEngine()

        self.canvas = Canvas(self.doc, self.brush)
        self.tool_panel = ToolPanel(self.brush)
        self.layer_panel = LayerPanel(self.doc)
        self.timeline = TimelineWidget(self.doc)
        self.timeline_ctrl = TimelineControls(self.doc)

        self._setup_ui()
        self._setup_menu()
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
        toolbar.addAction("New").triggered.connect(self._new_project)
        toolbar.addAction("Open").triggered.connect(self._open_project)
        toolbar.addAction("Save").triggered.connect(self._save)
        toolbar.addAction("Export PNG").triggered.connect(self._export_frame)
        toolbar.addAction("Export Video").triggered.connect(self._export_video)
        toolbar.addSeparator()
        audio_btn = toolbar.addAction("Import Audio")
        audio_btn.triggered.connect(self._import_audio)
        toolbar.addAction("Fit (F)").triggered.connect(self.canvas.fit)
        toolbar.addAction("Flip H (H)").triggered.connect(self._toggle_flip)
        toolbar.addAction("Rotate (R)").triggered.connect(self._do_rotate)
        toolbar.addAction("Grid (')").triggered.connect(self._toggle_grid)
        self.addToolBar(toolbar)

        # Content area
        content = QHBoxLayout()
        content.setSpacing(0)

        # Left panel: tools only
        left = QWidget()
        left.setFixedWidth(220)
        left.setStyleSheet("background: #353538;")
        left_lay = QVBoxLayout(left)
        left_lay.setContentsMargins(0, 0, 0, 0)
        left_lay.setSpacing(0)
        scroll = QScrollArea()
        scroll.setWidget(self.tool_panel)
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("QScrollArea { border: none; }")
        left_lay.addWidget(scroll)
        content.addWidget(left)

        # Center: Canvas
        content.addWidget(self.canvas, 1)

        # Right panel: layers in scroll area
        right = QWidget()
        right.setFixedWidth(210)
        right.setStyleSheet("background: #353538;")
        right_lay = QVBoxLayout(right)
        right_lay.setContentsMargins(2, 2, 2, 2)
        right_lay.setSpacing(0)
        right_scroll = QScrollArea()
        right_scroll.setWidget(self.layer_panel)
        right_scroll.setWidgetResizable(True)
        right_scroll.setStyleSheet("QScrollArea { border: none; }")
        right_lay.addWidget(right_scroll)
        content.addWidget(right)

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

        self.statusBar().showMessage(
            "Ready | B:Brush E:Eraser I:Pick G:Fill L:Line U:Rect Y:Ellipse "
            "M:Move S:Select H:Hand | Ctrl+Z:Undo Ctrl+Y:Redo Ctrl+C/V:Copy/Paste | "
            "F:Fit R:Rotate H:Flip | Space:Play | Arrows:Prev/Next Frame | Wheel:Zoom"
        )

    def _setup_menu(self):
        menubar = self.menuBar()

        file_menu = menubar.addMenu("&File")
        file_menu.addAction("New", self._new_project, "Ctrl+N")
        file_menu.addAction("Open...", self._open_project, "Ctrl+O")
        file_menu.addAction("Save", self._save, "Ctrl+S")
        file_menu.addSeparator()
        file_menu.addAction("Export Frame...", self._export_frame)
        file_menu.addAction("Export Video...", self._export_video)
        file_menu.addSeparator()
        file_menu.addAction("Exit", self.close, "Ctrl+Q")

        edit_menu = menubar.addMenu("&Edit")
        edit_menu.addAction("Undo", self.canvas._undo, "Ctrl+Z")
        edit_menu.addAction("Redo", self.canvas._redo, "Ctrl+Y")
        edit_menu.addSeparator()
        edit_menu.addAction("Copy Selection", self.canvas._copy_selection, "Ctrl+C")
        edit_menu.addAction("Paste Selection", self.canvas._paste_selection, "Ctrl+V")

        view_menu = menubar.addMenu("&View")
        view_menu.addAction("Fit Canvas", self.canvas.fit, "F")
        view_menu.addAction("Flip Horizontal", self._toggle_flip, "H")
        view_menu.addAction("Rotate +15 deg", self._do_rotate, "R")
        view_menu.addAction("Toggle Grid", self._toggle_grid, "'")

    def _connect(self):
        self.tool_panel.tool_changed.connect(self._set_canvas_tool)
        self.tool_panel.color_changed.connect(lambda c: setattr(self.canvas, 'color', c))
        self.tool_panel.settings_changed.connect(self.canvas.update)
        self.tool_panel.shape_fill_changed.connect(
            lambda v: setattr(self.canvas, '_shape_fill', v)
        )
        self.tool_panel.canvas_resize_requested.connect(self._resize_canvas)

        self.canvas.canvas_updated.connect(self._on_canvas_updated)
        self.canvas.color_picked.connect(self._on_color_picked)
        self.canvas.tool_changed_by_key.connect(self.tool_panel.set_active_tool)
        self.timeline.frame_changed.connect(self._on_frame)
        self.timeline_ctrl.frame_changed.connect(self._on_frame)
        self.timeline_ctrl.play_toggled.connect(self._on_play_toggled)
        self.layer_panel.layer_changed.connect(lambda i: self.canvas.update())

    def _on_canvas_updated(self):
        self.timeline.invalidate_cache()
        self.timeline.update()
        self.timeline_ctrl._upd()

    def _on_color_picked(self, color: QColor):
        self.tool_panel.brush.cfg.color = color
        self.tool_panel.update_color_btn(color)

    def _set_canvas_tool(self, tool: Tool):
        self.canvas.tool = tool
        self.tool_panel.set_active_tool(tool)

    def _toggle_flip(self):
        self.canvas._flip_h = not self.canvas._flip_h
        self.canvas.update()

    def _do_rotate(self):
        self.canvas._rot = (self.canvas._rot + 15) % 360
        self.canvas.update()

    def _toggle_grid(self):
        self.doc.show_grid = not self.doc.show_grid
        if self.doc.show_grid and self.doc.grid_size == 0:
            self.doc.grid_size = 64
        self.canvas.update()

    def _on_frame(self, fi: int):
        self.timeline.update()
        self.canvas.update()

    def _on_play_toggled(self, playing: bool):
        pass

    def _resize_canvas(self, new_w: int, new_h: int):
        reply = QMessageBox.question(
            self, "Resize Canvas",
            f"Resize canvas to {new_w}x{new_h}?\nExisting content will be preserved.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if reply != QMessageBox.StandardButton.Yes:
            return
        self.doc.resize_canvas(new_w, new_h)
        self.timeline.invalidate_cache()
        self.canvas.fit()
        self.canvas.update()
        self.statusBar().showMessage(f"Canvas resized to {new_w}x{new_h}")

    def _new_project(self):
        reply = QMessageBox.question(
            self, "New Project",
            "Start a new project? Unsaved work will be lost.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if reply != QMessageBox.StandardButton.Yes:
            return
        self.doc = Document()
        self.canvas.doc = self.doc
        self.canvas._drawing = False
        self.canvas._stroke_pts = []
        self.canvas._clear_selection()
        self.timeline.doc = self.doc
        self.timeline.invalidate_cache()
        self.timeline_ctrl.doc = self.doc
        self.timeline_ctrl._stop()
        self.layer_panel.doc = self.doc
        self.layer_panel._build()
        self.canvas.fit()
        self.timeline.update()
        self.canvas.update()
        self.statusBar().showMessage("New project created")

    def _open_project(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Open Project", "", "FAP Project (*.fap);;Legacy JSON (*.fap.json);;All Files (*)"
        )
        if not path:
            return

        try:
            if path.lower().endswith('.fap'):
                self._open_fap(path)
            else:
                self._open_legacy(path)
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to open project:\n{e}")
            return

        self.statusBar().showMessage(f"Opened: {os.path.basename(path)}")

    def _open_fap(self, path: str):
        with zipfile.ZipFile(path, 'r') as zf:
            meta = json.loads(zf.read('document.json').decode('utf-8'))

            self.doc = Document()
            self.doc.cw = meta.get("canvas_w", 1920)
            self.doc.ch = meta.get("canvas_h", 1080)
            self.doc.fps = meta.get("fps", 24)
            self.doc.total_frames = meta.get("total_frames", 1)
            self.doc.onion_prev = meta.get("onion_prev", 3)
            self.doc.onion_next = meta.get("onion_next", 1)
            self.doc.onion_opacity = meta.get("onion_opacity", 0.35)

            from .types_enums import LayerData, BlendMode
            layer_metas = meta.get("layers", [{"name": "Layer 1"}])
            self.doc.layers = []
            for lm in layer_metas:
                ld = LayerData(name=lm.get("name", "Layer"))
                ld.visible = lm.get("visible", True)
                ld.opacity = lm.get("opacity", 1.0)
                ld.blend_mode = BlendMode(lm.get("blend_mode", 0))
                ld.offset_x = lm.get("offset_x", 0.0)
                ld.offset_y = lm.get("offset_y", 0.0)
                ld.scale_x = lm.get("scale_x", 1.0)
                ld.scale_y = lm.get("scale_y", 1.0)
                ld.rotation = lm.get("rotation", 0.0)
                self.doc.layers.append(ld)

            for li in range(len(self.doc.layers)):
                for fi in range(self.doc.total_frames):
                    frame_path = f"layers/L{li:02d}_F{fi:04d}.png"
                    try:
                        data = zf.read(frame_path)
                        img = QImage()
                        img.loadFromData(data)
                        if not img.isNull():
                            self.doc.layers[li].frames[fi] = img
                    except KeyError:
                        pass

        self._rebind_document()

    def _open_legacy(self, path: str):
        with open(path, 'r') as f:
            meta = json.load(f)

        base = os.path.dirname(path)
        name = os.path.splitext(os.path.basename(path))[0]
        frames_dir = os.path.join(base, f"{name}_frames")

        if not os.path.isdir(frames_dir):
            QMessageBox.warning(self, "Missing frames",
                                f"Frames directory not found: {frames_dir}")
            return

        self.doc = Document()
        self.doc.cw = meta.get("canvas_w", 1920)
        self.doc.ch = meta.get("canvas_h", 1080)
        self.doc.fps = meta.get("fps", 24)
        self.doc.total_frames = meta.get("total_frames", 1)

        layer_names = meta.get("layers", ["Layer 1"])
        from .types_enums import LayerData
        self.doc.layers = []
        for ln in layer_names:
            self.doc.layers.append(LayerData(name=ln))

        for li in range(len(self.doc.layers)):
            for fi in range(self.doc.total_frames):
                fname = os.path.join(frames_dir, f"L{li:02d}_F{fi:04d}.png")
                if os.path.isfile(fname):
                    img = QImage(fname)
                    if not img.isNull():
                        self.doc.layers[li].frames[fi] = img

        self._rebind_document()

    def _rebind_document(self):
        self.canvas.doc = self.doc
        self.canvas._drawing = False
        self.canvas._stroke_pts = []
        self.canvas._clear_selection()
        self.timeline.doc = self.doc
        self.timeline.invalidate_cache()
        self.timeline_ctrl.doc = self.doc
        self.timeline_ctrl._stop()
        self.layer_panel.doc = self.doc
        self.layer_panel._build()
        self.canvas.fit()
        self.timeline.update()
        self.canvas.update()

    def _save(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "Save Project", "project.fap",
            "FAP Project (*.fap);;Legacy JSON (*.fap.json)"
        )
        if not path:
            return

        if path.lower().endswith('.fap'):
            self._save_fap(path)
        else:
            self._save_legacy(path)

    def _save_fap(self, path: str):
        with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as zf:
            layer_metas = []
            for layer in self.doc.layers:
                layer_metas.append({
                    "name": layer.name,
                    "visible": layer.visible,
                    "opacity": layer.opacity,
                    "blend_mode": int(layer.blend_mode),
                    "offset_x": layer.offset_x,
                    "offset_y": layer.offset_y,
                    "scale_x": layer.scale_x,
                    "scale_y": layer.scale_y,
                    "rotation": layer.rotation,
                })

            meta = {
                "version": "0.3",
                "canvas_w": self.doc.cw,
                "canvas_h": self.doc.ch,
                "fps": self.doc.fps,
                "total_frames": self.doc.total_frames,
                "onion_prev": self.doc.onion_prev,
                "onion_next": self.doc.onion_next,
                "onion_opacity": self.doc.onion_opacity,
                "layers": layer_metas,
            }
            zf.writestr('document.json', json.dumps(meta, indent=2))

            for li, layer in enumerate(self.doc.layers):
                for fi, img in layer.frames.items():
                    data = io.BytesIO()
                    img.save(data, 'PNG')
                    zf.writestr(f"layers/L{li:02d}_F{fi:04d}.png", data.getvalue())

        self.statusBar().showMessage(f"Project saved: {os.path.basename(path)}")
        QMessageBox.information(self, "Saved", f"Project saved to {path}")

    def _save_legacy(self, path: str):
        base = os.path.dirname(path)
        name = os.path.splitext(os.path.basename(path))[0]
        fdir = os.path.join(base, f"{name}_frames")
        os.makedirs(fdir, exist_ok=True)

        for li, layer in enumerate(self.doc.layers):
            for fi in layer.frames:
                fname = os.path.join(fdir, f"L{li:02d}_F{fi:04d}.png")
                layer.frames[fi].save(fname)

        meta = {
            "canvas_w": self.doc.cw,
            "canvas_h": self.doc.ch,
            "fps": self.doc.fps,
            "total_frames": self.doc.total_frames,
            "layers": [l.name for l in self.doc.layers],
        }
        with open(path, 'w') as f:
            json.dump(meta, f, indent=2)

        self.statusBar().showMessage(f"Project saved: {os.path.basename(path)}")
        QMessageBox.information(self, "Saved", f"Saved to {fdir}")

    def _export_frame(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "Export Frame", "frame_001.png",
            "PNG (*.png);;JPEG (*.jpg)"
        )
        if not path:
            return
        self.doc.cur_image().save(path)
        self.statusBar().showMessage(f"Exported: {os.path.basename(path)}")

    def _import_audio(self):
        if self.timeline_ctrl.import_audio():
            name = os.path.basename(self.doc.audio_path)
            self.statusBar().showMessage(f"Audio imported: {name}")

    def _export_video(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "Export Video", "animation.mp4",
            "MP4 (*.mp4);;GIF (*.gif);;PNG Sequence (*.png)"
        )
        if not path:
            return

        # Export all frames as PNG sequence, then use FFmpeg if available
        import tempfile
        import subprocess
        import shutil

        tmpdir = tempfile.mkdtemp(prefix="fap_export_")

        try:
            for fi in range(self.doc.total_frames):
                combined = QImage(self.doc.size, QImage.Format_ARGB32_Premultiplied)
                combined.fill(QColor(255, 255, 255))
                cp = QPainter(combined)
                for layer in self.doc.layers:
                    if not layer.visible:
                        continue
                    img = layer.get(fi, self.doc.size)
                    cp.setOpacity(layer.opacity)
                    cp.drawImage(0, 0, img)
                cp.end()
                fname = os.path.join(tmpdir, f"frame_{fi:04d}.png")
                combined.save(fname)

            ext = os.path.splitext(path)[1].lower()

            if ext == '.png':
                # Copy all frames to destination directory
                outdir = os.path.splitext(path)[0]
                os.makedirs(outdir, exist_ok=True)
                for f in os.listdir(tmpdir):
                    shutil.copy(os.path.join(tmpdir, f), os.path.join(outdir, f))
                self.statusBar().showMessage(f"Exported {self.doc.total_frames} frames to {outdir}")
            else:
                # Try FFmpeg
                fps = self.doc.fps
                if ext == '.gif':
                    cmd = [
                        "ffmpeg", "-y", "-framerate", str(fps),
                        "-i", os.path.join(tmpdir, "frame_%04d.png"),
                        "-vf", f"fps={fps},scale=iw:ih:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse",
                        path
                    ]
                else:
                    cmd = [
                        "ffmpeg", "-y", "-framerate", str(fps),
                        "-i", os.path.join(tmpdir, "frame_%04d.png"),
                        "-c:v", "libx264", "-pix_fmt", "yuv420p",
                        path
                    ]

                result = subprocess.run(cmd, capture_output=True, text=True)
                if result.returncode == 0:
                    self.statusBar().showMessage(
                        f"Video exported: {os.path.basename(path)}"
                    )
                else:
                    QMessageBox.warning(
                        self, "FFmpeg Error",
                        f"FFmpeg failed:\n{result.stderr}\n\n"
                        "Is FFmpeg installed? PNG frames saved to temp directory."
                    )
        except FileNotFoundError:
            QMessageBox.warning(
                self, "FFmpeg not found",
                "FFmpeg is not installed. Please install FFmpeg to export video.\n\n"
                "Download from: https://ffmpeg.org/download.html\n\n"
                f"PNG frames saved to: {tmpdir}"
            )
        except Exception as e:
            QMessageBox.critical(self, "Export Error", str(e))
        finally:
            if os.path.isdir(tmpdir):
                shutil.rmtree(tmpdir, ignore_errors=True)
