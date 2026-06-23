"""
Free Animation Power v0.3 — Entry point
"""

import sys

from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPalette
from PySide6.QtWidgets import QApplication

from src_py.main_window import MainWindow


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("Free Animation Power")
    app.setStyle("Fusion")

    pal = QPalette()
    pal.setColor(QPalette.ColorRole.Window, QColor(42, 42, 45))
    pal.setColor(QPalette.ColorRole.WindowText, QColor(220, 220, 220))
    pal.setColor(QPalette.ColorRole.Base, QColor(28, 28, 30))
    pal.setColor(QPalette.ColorRole.AlternateBase, QColor(42, 42, 45))
    pal.setColor(QPalette.ColorRole.Text, QColor(220, 220, 220))
    pal.setColor(QPalette.ColorRole.Button, QColor(52, 52, 55))
    pal.setColor(QPalette.ColorRole.ButtonText, QColor(220, 220, 220))
    pal.setColor(QPalette.ColorRole.Highlight, QColor(212, 120, 42))
    pal.setColor(QPalette.ColorRole.HighlightedText, QColor(255, 255, 255))
    app.setPalette(pal)

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
