#include "main_window_v2.hpp"

#include <QtWidgets/QApplication>
#include <QtCore/QCoreApplication>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QMenu>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QDialogButtonBox>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <dwmapi.h>
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#endif
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtCore/QFileInfo>
#include <QtGui/QAction>
#include <QtGui/QKeySequence>
#include <QtGui/QPainter>
#include <QtGui/QImage>

#include "core/diagnostic/tracer_macros.hpp"
#include "platform/file_association.hpp"
#include "core/tool_state.hpp"
#include "toolbox_panel_v2.hpp"
#include "onion_skin_panel.hpp"
#include "canvas_size_panel.hpp"
#include "color_panel_v2.hpp"
#include "layer_panel_v2.hpp"
#include "canvas_widget_v2.hpp"
#include "timeline_panel_v2.hpp"
#include "audio_track_widget.hpp"
#include "video_track_widget.hpp"
#include "busy_overlay.hpp"
#include "property_editor_v2.hpp"
#include "core/document.hpp"
#include "core/undo_manager.hpp"
#include "io/svg_export.hpp"
#include "io/document_manager.hpp"
#include "io/video_export.hpp"

namespace fap {

static const char* kTheme = R"(
* { font-family:'Avenir LT Std'; }
QMainWindow { background:#101218; }
QToolBar { background:#000000; border:none; border-bottom:1px solid #2D3139; spacing:2px; padding:4px 8px; }
QToolBar::separator { width:1px; background:#2D3139; margin:4px 6px; }
QToolButton { color:#FF4800; border:1px solid transparent; border-radius:6px; padding:4px 6px; font-size:11px; }
QToolButton:hover { background:#1A1A1A; color:#FF6A20; }
QToolButton:pressed,QToolButton:checked { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #FF4800,stop:1 #FF6A20); color:#fff; border-color:transparent; }
QToolButton:disabled { color:#3D4150; background:transparent; }
QDockWidget { background:#161A24; border:1px solid #252A36; border-radius:6px; titlebar-close-icon:none; }
QDockWidget::title { background:#1C2030; padding:6px 10px; border-bottom:1px solid #2D3139; border-top-left-radius:6px; border-top-right-radius:6px; font-size:10px; font-weight:600; text-transform:uppercase; letter-spacing:1px; color:#FF4800; }
QDockWidget::title:hover { background:#222838; color:#FF6A20; }
QDockWidget::float-button { background:#252A3A; border:1px solid #3A3E4A; border-radius:3px; width:14px; height:14px; }
QDockWidget::float-button:hover { background:#FF4800; border-color:#FF4800; }
QStatusBar { background:#0B0D12; color:#6B7088; border-top:1px solid #252A36; font-size:11px; padding:2px 8px; }
QScrollArea { background:transparent; border:none; }
QScrollBar:vertical { background:transparent; width:6px; margin:0; }
QScrollBar::handle:vertical { background:#FF4800; border-radius:3px; min-height:20px; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
QScrollBar:horizontal { background:transparent; height:6px; }
QScrollBar::handle:horizontal { background:#FF4800; border-radius:3px; min-width:20px; }
QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal { width:0; }
QSlider::groove:horizontal { background:#22252F; height:3px; border-radius:2px; }
QSlider::handle:horizontal { background:#FF4800; width:12px; height:12px; margin:-5px 0; border-radius:6px; }
QSlider::sub-page:horizontal { background:#FF4800; border-radius:2px; }
QSpinBox,QDoubleSpinBox { background:#1E2130; color:#C8CCD8; border:1px solid #2D3139; padding:2px 6px; border-radius:5px; font-size:10px; font-family:'Avenir LT Std',sans-serif; }
QSpinBox:focus { border-color:#FF4800; }
QComboBox { background:#1E2130; color:#C8CCD8; border:1px solid #2D3139; padding:3px 8px; border-radius:5px; font-size:10px; }
QComboBox:hover { border-color:#3D4150; }
QComboBox::drop-down { border:none; width:16px; }
QComboBox QAbstractItemView { background:#141820; color:#C8CCD8; selection-background-color:#FF4800; border:1px solid #252A36; }
QCheckBox { color:#8B8FA3; font-size:10px; spacing:6px; }
QCheckBox::indicator { width:14px; height:14px; border:1px solid #3D4150; border-radius:3px; background:#1E2130; }
QCheckBox::indicator:checked { background:#FF4800; border-color:#FF4800; }
QPushButton { background:#1E2130; color:#C8CCD8; border:1px solid #2D3139; border-radius:6px; padding:5px 10px; font-size:10px; font-weight:500; }
QPushButton:hover { background:#252838; border-color:#3D4150; }
QPushButton:pressed { background:#FF4800; color:#fff; border-color:#FF4800; }
QGroupBox { color:#FF4800; font-weight:600; font-size:9px; text-transform:uppercase; letter-spacing:1px; border:1px solid #2D3139; border-radius:6px; margin-top:12px; padding-top:14px; }
QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 6px; }
QLabel { color:#8B8FA3; font-size:10px; }
QListWidget { background:#14161C; color:#C8CCD8; border:1px solid #2D3139; border-radius:5px; outline:none; }
QListWidget::item { padding:4px 8px; border-radius:3px; }
QListWidget::item:selected { background:#FF4800; color:#fff; }
QListWidget::item:hover { background:#1E2130; }
)";



MainWindowV2::MainWindowV2(std::shared_ptr<AppState> state, QWidget* parent)
    : QMainWindow(parent)
    , appState_(std::move(state))
{
    setWindowTitle("Free Animation Power - Untitled.fap");
    setWindowIcon(QIcon(":/icons/toolbar/fap.ico"));
    resize(1600, 900);
    setMinimumSize(1024, 600);

    qApp->setStyleSheet(QLatin1String(kTheme));
    qApp->setFont(QFont("Avenir LT Std", 11));

#ifdef Q_OS_WIN
    {
        BOOL dark = TRUE;
        DwmSetWindowAttribute(
            reinterpret_cast<HWND>(winId()),
            20, // DWMWA_USE_IMMERSIVE_DARK_MODE
            &dark, sizeof(dark));
    }
    registerFileAssociation();
#endif

    setupUI();
    updateUIState();
}

MainWindowV2::~MainWindowV2() = default;

void MainWindowV2::setupUI()
{
    setupTopBar();
    setupDocks();
    setupStatusBar();
    setupConnections();
}

void MainWindowV2::setupTopBar()
{
    auto* toolbar = addToolBar("MainBar");
    toolbar->setMovable(false);
    toolbar->setObjectName("mainToolBarV2");
    toolbar->setIconSize(QSize(32, 32));
    toolbar->setFixedHeight(52);

    auto* newAct = toolbar->addAction(QIcon(":/icons/toolbar/new_project.png"), "");
    newAct->setToolTip("New Project (Ctrl+N)");
    newAct->setShortcut(QKeySequence("Ctrl+N"));
    connect(newAct, &QAction::triggered, this, [this]() { newProject(); });

    auto* openAct = toolbar->addAction(QIcon(":/icons/toolbar/open_project.png"), "");
    openAct->setToolTip("Open Project (Ctrl+O)");
    openAct->setShortcut(QKeySequence("Ctrl+O"));
    connect(openAct, &QAction::triggered, this, [this]() { openProjectDialog(); });

    auto* saveAct = toolbar->addAction(QIcon(":/icons/toolbar/save_project.png"), "");
    saveAct->setToolTip("Save Project (Ctrl+S)");
    saveAct->setShortcut(QKeySequence("Ctrl+S"));
    connect(saveAct, &QAction::triggered, this, [this]() { saveProject(); });

    toolbar->addSeparator();

    auto* exportVidAct = toolbar->addAction(QIcon(":/icons/toolbar/export_video.png"), "");
    exportVidAct->setToolTip("Export Video (Ctrl+E)");
    exportVidAct->setShortcut(QKeySequence("Ctrl+E"));
    connect(exportVidAct, &QAction::triggered, this, &MainWindowV2::exportVideo);

    auto* exportGifAct = toolbar->addAction(QIcon(":/icons/toolbar/export_gif.png"), "");
    exportGifAct->setToolTip("Export GIF");
    connect(exportGifAct, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getSaveFileName(
            this, "Export GIF", "animation.gif",
            "GIF Animation (*.gif);;All Files (*)");
        if (path.isEmpty()) return;

        auto& doc = appState_->document();
        int cw = doc.width();
        int ch = doc.height();

        QDialog dlg(this);
        dlg.setWindowTitle("GIF Resolution");
        dlg.setMinimumWidth(280);
        auto* form = new QFormLayout(&dlg);
        auto* spinW = new QSpinBox(&dlg);
        spinW->setRange(1, 8192);
        spinW->setValue(cw);
        auto* spinH = new QSpinBox(&dlg);
        spinH->setRange(1, 8192);
        spinH->setValue(ch);
        form->addRow("Width:", spinW);
        form->addRow("Height:", spinH);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        form->addRow(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        if (dlg.exec() != QDialog::Accepted) return;

        fap::ExportOptions opts;
        opts.outputWidth = spinW->value();
        opts.outputHeight = spinH->value();

        statusBar()->showMessage("Exporting GIF...");
        QApplication::processEvents();
        showBusy("Exportando GIF...");

        if (fap::exportGIF(doc, path, doc.fps(), opts)) {
            hideBusy();
            statusBar()->showMessage("GIF exported: " + path, 5000);
        } else {
            hideBusy();
            QMessageBox::warning(this, "Error",
                "FFmpeg failed. Is FFmpeg installed and in PATH?");
            statusBar()->showMessage("GIF export failed", 5000);
        }
    });

    auto* exportSvgBtn = new QToolButton(this);
    exportSvgBtn->setIcon(QIcon(":/icons/toolbar/export_video.png"));
    exportSvgBtn->setToolTip("Export SVG");
    exportSvgBtn->setPopupMode(QToolButton::InstantPopup);
    auto* svgMenu = new QMenu(exportSvgBtn);
    svgMenu->setStyleSheet(QString(
        "QMenu { background:%1; color:%2; border:1px solid %3; border-radius:4px; padding:4px; }"
        "QMenu::item { padding:4px 16px; border-radius:3px; }"
        "QMenu::item:selected { background:#252838; color:#E8ECF0; }")
        .arg("#1E2130", "#C8CCD8", "#1E2128"));
    svgMenu->addAction("Current Frame", this, &MainWindowV2::exportSVGCurrentFrame);
    svgMenu->addAction("All Frames",   this, &MainWindowV2::exportSVGAllFrames);
    exportSvgBtn->setMenu(svgMenu);
    toolbar->addWidget(exportSvgBtn);

    toolbar->addSeparator();

    auto* fitAct = toolbar->addAction(QIcon(":/icons/toolbar/fit_canvas.png"), "");
    fitAct->setToolTip("Fit Canvas (F)");
    connect(fitAct, &QAction::triggered, [this]() { if (canvas_) canvas_->fit(); });

    auto* flipAct = toolbar->addAction(QIcon(":/icons/toolbar/flip_horizontal.png"), "");
    flipAct->setToolTip("Flip Horizontal (Ctrl+H)");
    connect(flipAct, &QAction::triggered, [this]() { if (canvas_) { canvas_->flipH(); canvas_->update(); } });

    auto* rotAct = toolbar->addAction(QIcon(":/icons/toolbar/rotate.png"), "");
    rotAct->setToolTip("Rotate +15deg (R)");
    connect(rotAct, &QAction::triggered, [this]() { if (canvas_) { canvas_->rotateCanvas(); canvas_->update(); } });

    auto* rotLAct = toolbar->addAction(QIcon(":/icons/toolbar/rotate_minus.png"), "");
    rotLAct->setToolTip("Rotate -15deg (L)");
    rotLAct->setShortcut(QKeySequence("L"));
    connect(rotLAct, &QAction::triggered, [this]() { if (canvas_) { canvas_->rotateCanvasMinus(); canvas_->update(); } });

    auto* gridAct = toolbar->addAction(QIcon(":/icons/toolbar/toggle_grid.png"), "");
    gridAct->setToolTip("Toggle Grid (')");
    connect(gridAct, &QAction::triggered, [this]() { if (canvas_) canvas_->toggleGrid(); });

    toolbar->addSeparator();

    undoAct_ = toolbar->addAction(QIcon(":/icons/toolbar/undo.png"), "");
    undoAct_->setToolTip("Undo (Ctrl+Z)");
    undoAct_->setShortcut(QKeySequence::Undo);
    connect(undoAct_, &QAction::triggered, this, [this]() {
        if (canvas_) { canvas_->undo(); }
        updateUIState();
    });

    redoAct_ = toolbar->addAction(QIcon(":/icons/toolbar/redo.png"), "");
    redoAct_->setToolTip("Redo (Ctrl+Y)");
    redoAct_->setShortcut(QKeySequence::Redo);
    connect(redoAct_, &QAction::triggered, this, [this]() {
        if (canvas_) { canvas_->redo(); }
        updateUIState();
    });

    auto* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    auto* helpAct = toolbar->addAction(QIcon(":/icons/toolbar/help_shortcuts.png"), "");
    helpAct->setToolTip("Keyboard Shortcuts (F1)");
    connect(helpAct, &QAction::triggered, [this]() {
        QMessageBox::about(this, "Keyboard Shortcuts",
            "<style>body{background:#0F1115;color:#C8CCD8;font-family:'Avenir LT Std',sans-serif}"
            "table{border-collapse:collapse;width:100%}"
            "td{padding:4px 12px}"
            "b{color:#FF4800}"
            "h3{color:#FF4800;margin-top:16px;font-size:13px}</style>"
            "<h3>Tools</h3>"
            "<table>"
            "<tr><td><b>B</b></td><td>Brush</td><td><b>E</b></td><td>Eraser</td></tr>"
            "<tr><td><b>I</b></td><td>Pick Color</td><td><b>G</b></td><td>Fill</td></tr>"
            "<tr><td><b>T</b></td><td>Text</td><td><b>L</b></td><td>Line</td></tr>"
            "<tr><td><b>U</b></td><td>Rectangle</td><td><b>Y</b></td><td>Ellipse</td></tr>"
            "<tr><td><b>M</b></td><td>Move</td><td><b>S</b></td><td>Select</td></tr>"
            "<tr><td><b>H</b></td><td>Hand/Pan</td><td></td><td></td></tr>"
            "</table>"
            "<h3>Edit</h3>"
            "<table>"
            "<tr><td><b>Ctrl+Z</b></td><td>Undo</td><td><b>Ctrl+Y</b></td><td>Redo</td></tr>"
            "<tr><td><b>Ctrl+C</b></td><td>Copy</td><td><b>Ctrl+X</b></td><td>Cut</td></tr>"
            "<tr><td><b>Ctrl+V</b></td><td>Paste</td><td><b>Ctrl+N</b></td><td>New</td></tr>"
            "<tr><td><b>Ctrl+O</b></td><td>Open</td><td><b>Ctrl+S</b></td><td>Save</td></tr>"
            "<tr><td><b>Ctrl+E</b></td><td>Export Video</td><td><b>Delete</b></td><td>Clear Frame</td></tr>"
            "</table>"
            "<h3>View</h3>"
            "<table>"
            "<tr><td><b>F</b></td><td>Fit Canvas</td><td><b>Ctrl+H</b></td><td>Flip Horizontal</td></tr>"
            "<tr><td><b>R</b></td><td>Rotate +15deg</td><td><b>'</b></td><td>Toggle Grid</td></tr>"
            "<tr><td><b>Ctrl+0</b></td><td>Reset View</td><td><b>Space</b></td><td>Play/Pause</td></tr>"
            "<tr><td><b>Arrow L/R</b></td><td>Prev/Next Frame</td><td><b>Mouse Wheel</b></td><td>Zoom</td></tr>"
            "<tr><td><b>Middle Drag</b></td><td>Pan</td><td></td><td></td></tr>"
            "</table>");
    });
}

void MainWindowV2::setupDocks()
{
    auto* toolboxDock = new QDockWidget("TOOLS", this);
    toolboxDock->setObjectName("toolboxDockV2");
    toolboxDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    toolbox_panel_ = new ToolboxPanelV2(appState_, toolboxDock);
    toolboxDock->setWidget(toolbox_panel_);
    toolboxDock->setMinimumWidth(60);
    addDockWidget(Qt::LeftDockWidgetArea, toolboxDock);

    auto* layerDock = new QDockWidget("LAYERS", this);
    layerDock->setObjectName("layerDockV2");
    layerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    layer_panel_ = new LayerPanelV2(appState_, layerDock);
    layerDock->setWidget(layer_panel_);
    layerDock->setMinimumWidth(200);
    addDockWidget(Qt::RightDockWidgetArea, layerDock);

    auto* colorDock = new QDockWidget("COLOR", this);
    colorDock->setObjectName("colorDockV2");
    colorDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    color_panel_ = new ColorPanelV2(colorDock);
    colorDock->setWidget(color_panel_);
    addDockWidget(Qt::RightDockWidgetArea, colorDock);

    auto* propertyDock = new QDockWidget("PROPERTIES", this);
    propertyDock->setObjectName("propertyDockV2");
    propertyDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    property_editor_ = new PropertyEditorV2(appState_, propertyDock);
    propertyDock->setWidget(property_editor_);
    addDockWidget(Qt::RightDockWidgetArea, propertyDock);

    auto* onionDock = new QDockWidget("ONION SKIN", this);
    onionDock->setObjectName("onionSkinDockV2");
    onionDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    onion_skin_panel_ = new OnionSkinPanel(onionDock);
    onionDock->setWidget(onion_skin_panel_);
    addDockWidget(Qt::LeftDockWidgetArea, onionDock);
    // Sync from ToolState defaults
    onion_skin_panel_->setEnabled(appState_->toolState().onionEnabled());
    onion_skin_panel_->setPrevFrames(appState_->toolState().onionPrevFrames());
    onion_skin_panel_->setNextFrames(appState_->toolState().onionNextFrames());
    onion_skin_panel_->setOpacity(appState_->toolState().onionOpacity());

    auto* sizeDock = new QDockWidget("CANVAS SIZE", this);
    sizeDock->setObjectName("canvasSizeDockV2");
    sizeDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    canvas_size_panel_ = new CanvasSizePanel(sizeDock);
    sizeDock->setWidget(canvas_size_panel_);
    addDockWidget(Qt::RightDockWidgetArea, sizeDock);
    canvas_size_panel_->setCanvasWidth(appState_->toolState().canvasWidth());
    canvas_size_panel_->setCanvasHeight(appState_->toolState().canvasHeight());

    auto* canvasDock = new QDockWidget("CANVAS", this);
    canvasDock->setObjectName("canvasDockV2");
    canvasDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    canvas_ = new CanvasWidgetV2(appState_, canvasDock);
    canvas_->setMinimumSize(400, 300);
    canvasDock->setWidget(canvas_);
    setCentralWidget(canvasDock);

    auto* timelineDock = new QDockWidget("TIMELINE", this);
    timelineDock->setObjectName("timelineDockV2");
    timelineDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    timeline_panel_ = new TimelinePanelV2(appState_, timelineDock);
    timelineDock->setWidget(timeline_panel_);
    addDockWidget(Qt::BottomDockWidgetArea, timelineDock);

    canvas_->setVideoTrackWidgets(const_cast<std::vector<VideoTrackWidget*>*>(
        &timeline_panel_->videoTrackWidgets()));

    // Apply orange title color via title bar widget (CSS QDockWidget::title unreliable on Win)
    const char* kDockTitleStyle =
        "QLabel { color: #FF4800; font-size: 10px; font-weight: 600;"
        " padding: 4px 10px; }";
    for (auto* dock : {toolboxDock, layerDock, colorDock, propertyDock, onionDock, sizeDock, canvasDock, timelineDock}) {
        auto* titleLabel = new QLabel(dock->windowTitle());
        titleLabel->setStyleSheet(kDockTitleStyle);
        dock->setTitleBarWidget(titleLabel);
    }
}

void MainWindowV2::setupStatusBar()
{
    statusBar()->showMessage("Ready");

    auto* hintLabel = new QLabel(
        "  Ctrl+N New  |  Ctrl+O Open  |  Ctrl+S Save  |  Ctrl+E Export  |  Ctrl+Z Undo  ");
    hintLabel->setStyleSheet("color: #6B7088; font-size: 10px;");
    statusBar()->addPermanentWidget(hintLabel);
}

void MainWindowV2::setupConnections()
{
    connect(toolbox_panel_, &ToolboxPanelV2::toolChanged, [this](int tool) {
        if (canvas_) { canvas_->setTool(tool); canvas_->setFocus(); }
        if (property_editor_) property_editor_->refreshFields();
        statusBar()->showMessage(QString("Tool: %1").arg(tool), 2000);
    });

    connect(toolbox_panel_, &ToolboxPanelV2::colorChanged, [this](const QColor& color) {
        if (canvas_) canvas_->setColor(color);
    });

    connect(color_panel_, &ColorPanelV2::colorChanged, [this](const QColor& color) {
        toolbox_panel_->setColor(color);
    });

    connect(layer_panel_, &LayerPanelV2::layerChanged, [this](int index) {
        if (canvas_) { canvas_->setCurrentLayer(index); canvas_->update(); }
        statusBar()->showMessage(QString("Layer: %1").arg(index + 1), 2000);
    });

    connect(layer_panel_, &LayerPanelV2::layerDisplayPropertiesChanged, [this]() {
        if (canvas_) {
            canvas_->invalidateBackgroundCache();
            canvas_->update();
        }
    });

    connect(toolbox_panel_, &ToolboxPanelV2::settingsChanged, [this]() {
        appState_->document().setModified(true);
    });

    connect(onion_skin_panel_, &OnionSkinPanel::settingsChanged, [this]() {
        appState_->toolState().setOnionEnabled(onion_skin_panel_->enabled());
        appState_->toolState().setOnionPrevFrames(onion_skin_panel_->prevFrames());
        appState_->toolState().setOnionNextFrames(onion_skin_panel_->nextFrames());
        appState_->toolState().setOnionOpacity(onion_skin_panel_->opacity());
        if (canvas_) {
            canvas_->setOnionEnabled(onion_skin_panel_->enabled());
            canvas_->setOnionSettings(
                onion_skin_panel_->prevFrames(),
                onion_skin_panel_->nextFrames(),
                onion_skin_panel_->opacity());
            canvas_->update();
        }
    });

    connect(canvas_size_panel_, &CanvasSizePanel::canvasResized, [this](int w, int h) {
        appState_->toolState().setCanvasSize(w, h);
        if (canvas_) {
            canvas_->resizeCanvas(w, h);
            canvas_->fit();
            canvas_->update();
        }
        appState_->document().setCanvasSize(w, h);
        appState_->document().setModified(true);
        statusBar()->showMessage(QString("Canvas resized to %1 x %2").arg(w).arg(h), 3000);
    });

    connect(timeline_panel_, &TimelinePanelV2::frameChanged, [this](int frame) {
        if (canvas_) { canvas_->setCurrentFrame(frame); canvas_->update(); }
        if (layer_panel_) layer_panel_->setCurrentFrame(frame);
        statusBar()->showMessage(QString("Frame: %1 / %2")
            .arg(frame + 1).arg(appState_->activeSequence().durationFrames()), 3000);
    });

    connect(timeline_panel_, &TimelinePanelV2::frameCountChanged, [this](int count) {
        if (canvas_) {
            canvas_->setTotalFrames(count);
            canvas_->setDurationFrames(appState_->activeSequence().durationFrames());
        }
        appState_->document().setTotalFrames(count);
    });

    connect(timeline_panel_, &TimelinePanelV2::fpsChanged, [this](int fps) {
        appState_->document().setFPS(fps);
    });

    connect(timeline_panel_, &TimelinePanelV2::sequenceChanged, [this](int) {
        updateUIState();
    });

    connect(timeline_panel_, &TimelinePanelV2::videoTrackChanged, [this]() {
        if (canvas_) {
            canvas_->invalidateBackgroundCache();
            canvas_->update();
        }
    });

    connect(timeline_panel_, &TimelinePanelV2::busyStarted, this, &MainWindowV2::showBusy);
    connect(timeline_panel_, &TimelinePanelV2::busyFinished, this, &MainWindowV2::hideBusy);

    connect(property_editor_, &PropertyEditorV2::durationFramesChanged,
            [this](int dur) {
        appState_->setDurationFrames(dur);
        if (timeline_panel_) {
            timeline_panel_->refreshTimelineLayout();
        }
    });

    connect(canvas_, &CanvasWidgetV2::frameChanged, [this](int frame) {
        if (timeline_panel_) timeline_panel_->setCurrentFrame(frame);
        if (layer_panel_) layer_panel_->setCurrentFrame(frame);
        statusBar()->showMessage(QString("Frame: %1 / %2")
            .arg(frame + 1).arg(appState_->activeSequence().durationFrames()), 3000);
    });

    connect(canvas_, &CanvasWidgetV2::canvasUpdated, [this]() {
        if (timeline_panel_) timeline_panel_->update();
        if (layer_panel_) layer_panel_->refreshLayerList();
    });

    connect(canvas_, &CanvasWidgetV2::colorPicked, [this](const QColor& color) {
        toolbox_panel_->setColor(color);
        color_panel_->setColor(color);
        if (property_editor_) property_editor_->updateColorVariations();
    });

    connect(canvas_, &CanvasWidgetV2::statusMessage, [this](const QString& msg) {
        statusBar()->showMessage(msg, 3000);
    });

    connect(canvas_, &CanvasWidgetV2::toolChangedByKey, [this](int tool) {
        toolbox_panel_->setActiveTool(tool);
    });

    connect(canvas_, &CanvasWidgetV2::togglePlayPause, [this]() {
        if (timeline_panel_) timeline_panel_->togglePlayback();
    });

    connect(canvas_, &CanvasWidgetV2::addFrameRequested, [this]() {
        if (timeline_panel_) timeline_panel_->addFrame();
    });

    connect(canvas_, &CanvasWidgetV2::hideFrameRequested, [this]() {
        if (timeline_panel_) timeline_panel_->hideFrame();
    });

    connect(property_editor_, &PropertyEditorV2::brushSizeChanged, [this](int size) {
        if (canvas_) { canvas_->setBrushSize(size); }
    });

    connect(property_editor_, &PropertyEditorV2::brushOpacityChanged, [this](int) {
        if (canvas_) { canvas_->update(); }
    });

    connect(property_editor_, &PropertyEditorV2::brushShapeChanged, [this](const QString& shape) {
        if (canvas_) { canvas_->setBrushShape(shape); canvas_->update(); }
    });

    connect(property_editor_, &PropertyEditorV2::primaryColorChanged, [this](const QColor& color) {
        if (canvas_) canvas_->setColor(color);
        toolbox_panel_->setColor(color);
        color_panel_->setColor(color);
    });
}

void MainWindowV2::updateUIState()
{
    auto& um = appState_->document().undoManager();
    if (undoAct_) undoAct_->setEnabled(um.canUndo());
    if (redoAct_) redoAct_->setEnabled(um.canRedo());
}

void MainWindowV2::closeEvent(QCloseEvent* event)
{
    if (!appState_->isModified()) {
        event->accept();
        return;
    }

    auto btn = QMessageBox::question(this, "Unsaved Changes",
        QString::fromUtf8("The project has unsaved changes.\n\nSave before closing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (btn == QMessageBox::Save) {
        saveProject();
        if (!appState_->isModified()) {
            event->accept();
        } else {
            event->ignore();
        }
    } else if (btn == QMessageBox::Discard) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindowV2::newProject()
{
    if (saving_) {
        statusBar()->showMessage("Save in progress, wait...", 2000);
        return;
    }

    if (appState_->isModified()) {
        auto result = QMessageBox::question(this, "Unsaved Changes",
            "The current project has unsaved changes. Create a new project anyway?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (result != QMessageBox::Yes)
            return;
    }

    appState_->resetDocument(1920, 1080, 24, 24);

    // Process deferred deletions and pending events from old document
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    setWindowTitle("Free Animation Power - Untitled.fap");
    if (timeline_panel_) {
        timeline_panel_->clearAudioTracks();
        timeline_panel_->clearVideoTracks();
    }
    layer_panel_->refreshLayerList();
    if (canvas_) {
        canvas_->resetState();
        canvas_->invalidateBackgroundCache();
        canvas_->setCurrentFrame(0);
        canvas_->setTotalFrames(appState_->document().totalFrames());
        canvas_->setDurationFrames(appState_->activeSequence().durationFrames());
        canvas_->setCurrentLayer(0);
        canvas_->fit();
        canvas_->update();
    }
    if (timeline_panel_) {
        timeline_panel_->setTotalFrames(appState_->document().totalFrames());
        timeline_panel_->setDurationFrames(appState_->activeSequence().durationFrames());
        timeline_panel_->setCurrentFrame(0);
        timeline_panel_->setFPS(appState_->document().fps());
        timeline_panel_->rebuildTracks();
    }
    updateUIState();
    toolbox_panel_->setActiveTool(0);
    statusBar()->showMessage("New project created", 3000);
}

void MainWindowV2::openProjectDialog()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Open Project", QString(),
        "Free Animation Power (*.fap);;All Files (*)");
    if (!path.isEmpty())
        openProject(path);
}

void MainWindowV2::openProject(const QString& path)
{
    if (path.isEmpty())
        return;

    if (saving_) {
        statusBar()->showMessage("Save in progress, wait...", 2000);
        return;
    }

    if (appState_->isModified()) {
        auto result = QMessageBox::question(this, "Unsaved Changes",
            "The current project has unsaved changes. Open another project?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (result != QMessageBox::Yes)
            return;
    }

    appState_->resetDocument(1920, 1080, 24, 1);

    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    showBusy("Abriendo proyecto...");

    DocumentManager dm;
    if (dm.load(appState_->document(), path)) {
        hideBusy();
        appState_->document().setFilepath(path.toStdString());
        appState_->document().setModified(false);

        FAP_TRACE_CAT(fap::diagnostic::EventCategory::App,
            ("open_seq_count_" + std::to_string(appState_->document().sequenceCount())).c_str());
        setWindowTitle(QString("Free Animation Power - %1").arg(path));
        layer_panel_->refreshLayerList();
        auto& doc = appState_->document();
        auto& seq = doc.activeSequence();
        if (canvas_) {
            canvas_->setCurrentFrame(seq.currentFrame());
            canvas_->setTotalFrames(seq.totalFrames());
            canvas_->setDurationFrames(seq.durationFrames());
            canvas_->setCurrentLayer(0);
            ViewState vs = dm.viewState();
            qDebug() << "RESTORE viewState zoom:" << vs.zoom
                     << "offsetX:" << vs.offsetX << "offsetY:" << vs.offsetY;
            QTimer::singleShot(0, canvas_, [this, vs]() {
                if (!canvas_) return;
                if (vs.zoom > 0) {
                    canvas_->setViewState(vs.zoom, vs.offsetX, vs.offsetY);
                } else {
                    canvas_->fit();
                }
            });
        }
        if (timeline_panel_) {
            timeline_panel_->setTotalFrames(seq.totalFrames());
            timeline_panel_->setDurationFrames(seq.durationFrames());
            timeline_panel_->setCurrentFrame(seq.currentFrame());
            timeline_panel_->setFPS(seq.fps());
            timeline_panel_->rebuildTracks();
        }
        appState_->setActiveSequence(0);
        updateUIState();
        statusBar()->showMessage("Project opened: " + path, 3000);
        toolbox_panel_->setActiveTool(0);

        // Restore audio tracks from loaded document
        if (timeline_panel_) {
            timeline_panel_->clearAudioTracks();
            for (const auto& at : appState_->document().audioTracks()) {
                timeline_panel_->addAudioTrackFromData(at);
            }
            timeline_panel_->clearVideoTracks();
            for (const auto& vt : appState_->document().videoTracks()) {
                timeline_panel_->addVideoTrackFromData(vt);
            }
        }
    } else {
        hideBusy();
        QMessageBox::warning(this, "Error", "Failed to open project:\n" + path);
    }
}

void MainWindowV2::saveProject()
{
    if (appState_->document().filepath().empty()) {
        saveProjectAs();
        return;
    }

    syncAudioToDocument();
    syncVideoToDocument();

    saving_ = true;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage("Saving...");
    showBusy("Guardando proyecto...");

    DocumentManager dm;
    ViewState vs;
    if (canvas_) {
        vs.zoom    = canvas_->viewZoom();
        vs.offsetX = canvas_->viewOffsetX();
        vs.offsetY = canvas_->viewOffsetY();
    }
    if (dm.save(appState_->document(),
                QString::fromStdString(appState_->document().filepath()), vs)) {
        appState_->document().setModified(false);
        hideBusy();
        statusBar()->showMessage("Project saved", 3000);
    } else {
        hideBusy();
        QMessageBox::warning(this, "Error",
                             QString("Failed to save project.\n\n") + dm.lastError());
    }

    QApplication::restoreOverrideCursor();
    saving_ = false;
}

void MainWindowV2::saveProjectAs()
{
    // Ask user: single .fap file or project folder?
    QMessageBox saveChoice(this);
    saveChoice.setWindowTitle(QString::fromUtf8("Save As..."));
    saveChoice.setText(QString::fromUtf8(
        "How do you want to save the project?"));
    saveChoice.setInformativeText(QString::fromUtf8(
        "File (.fap) — Everything packed in a single portable file.\n"
        "Folder — Project folder with .fap and audio files organized."));
    QPushButton* fileBtn = saveChoice.addButton(QString::fromUtf8("Single .fap File"), QMessageBox::AcceptRole);
    QPushButton* folderBtn = saveChoice.addButton(QString::fromUtf8("Project Folder"), QMessageBox::ActionRole);
    saveChoice.addButton(QMessageBox::Cancel);
    saveChoice.setDefaultButton(fileBtn);
    saveChoice.exec();

    QString path;
    bool folderMode = (saveChoice.clickedButton() == folderBtn);

    if (folderMode) {
        QString dir = QFileDialog::getExistingDirectory(
            this, "Select Project Folder", QString(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (dir.isEmpty()) return;
        bool ok = false;
        QString name = QInputDialog::getText(this, "Project Name",
            "Project name:", QLineEdit::Normal, "untitled", &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        path = dir + "/" + name.trimmed() + ".fap";
    } else if (saveChoice.clickedButton() == fileBtn) {
        path = QFileDialog::getSaveFileName(
            this, "Save Project As", "untitled.fap",
            "Free Animation Power (*.fap);;All Files (*)");
    } else {
        return;
    }

    if (path.isEmpty()) return;

    syncAudioToDocument();
    syncVideoToDocument();

    saving_ = true;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage("Saving...");
    showBusy("Guardando proyecto...");

    DocumentManager dm;
    ViewState vs;
    if (canvas_) {
        vs.zoom    = canvas_->viewZoom();
        vs.offsetX = canvas_->viewOffsetX();
        vs.offsetY = canvas_->viewOffsetY();
    }
    if (dm.save(appState_->document(), path, vs)) {
        appState_->document().setFilepath(path.toStdString());
        appState_->document().setModified(false);
        setWindowTitle(QString("Free Animation Power - %1").arg(path));

        if (folderMode && !appState_->document().audioTracks().empty()) {
            // Extract audio files alongside .fap for external access
            QDir projectDir = QFileInfo(path).absoluteDir();
            QDir audioDir(projectDir.filePath("audio"));
            if (!audioDir.exists()) projectDir.mkdir("audio");
            for (const auto& at : appState_->document().audioTracks()) {
                QFileInfo fi(QString::fromStdString(at.filepath));
                if (fi.exists()) {
                    QString destPath = audioDir.filePath(fi.fileName());
                    QFile::copy(fi.absoluteFilePath(), destPath);
                }
            }
        }

        statusBar()->showMessage(
            folderMode ? "Project saved: " + QFileInfo(path).absolutePath() : "Project saved: " + path, 3000);
    } else {
        QMessageBox::warning(this, "Error",
                             QString("Failed to save project.\n\n") + dm.lastError());
    }

    hideBusy();
    QApplication::restoreOverrideCursor();
    saving_ = false;
}

void MainWindowV2::showBusy(const QString& message)
{
    if (!busyOverlay_)
        busyOverlay_ = new BusyOverlay(this);
    if (!busyKeepAliveTimer_) {
        busyKeepAliveTimer_ = new QTimer(this);
        connect(busyKeepAliveTimer_, &QTimer::timeout, this, [this]() {
            if (busyOverlay_ && busyOverlay_->isVisible()) {
                busyOverlay_->raise();
                busyOverlay_->update();
            }
        });
    }
    busyOverlay_->setMessage(message);
    busyOverlay_->showOverlay();
    busyKeepAliveTimer_->start(150);
    for (int i = 0; i < 5; ++i)
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void MainWindowV2::hideBusy()
{
    if (busyKeepAliveTimer_)
        busyKeepAliveTimer_->stop();
    if (busyOverlay_)
        busyOverlay_->hideOverlay();
}

void MainWindowV2::exportVideo()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Export Video", "animation.mp4",
        "MP4 H.264 (*.mp4);;"
        "MOV QuickTime with Alpha (*.mov);;"
        "WebM VP9 with Alpha (*.webm);;"
        "All Files (*)");
    if (path.isEmpty())
        return;

    auto& doc = appState_->document();
    int cw = doc.width();
    int ch = doc.height();

    QDialog dlg(this);
    dlg.setWindowTitle("Video Resolution");
    dlg.setMinimumWidth(280);
    auto* form = new QFormLayout(&dlg);
    auto* spinW = new QSpinBox(&dlg);
    spinW->setRange(1, 8192);
    spinW->setValue(cw);
    auto* spinH = new QSpinBox(&dlg);
    spinH->setRange(1, 8192);
    spinH->setValue(ch);
    form->addRow("Width:", spinW);
    form->addRow("Height:", spinH);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    fap::ExportOptions opts;
    opts.outputWidth = spinW->value();
    opts.outputHeight = spinH->value();

    statusBar()->showMessage("Rendering and encoding video...");
    QApplication::processEvents();
    showBusy("Exportando video...");

    if (fap::exportVideo(doc, path, doc.fps(), opts)) {
        hideBusy();
        statusBar()->showMessage("Video exported: " + path, 5000);
    } else {
        hideBusy();
        QMessageBox::warning(this, "Error",
            "FFmpeg failed. Is FFmpeg installed and in PATH?");
        statusBar()->showMessage("Video export failed", 5000);
    }
}

void MainWindowV2::exportSVGCurrentFrame()
{
    auto& doc = appState_->document();
    int frame = doc.currentFrame();
    QString path = QFileDialog::getSaveFileName(
        this, "Export Current Frame as SVG",
        QString("frame_%1.svg").arg(frame, 4, 10, QChar('0')),
        "SVG Files (*.svg);;All Files (*)");
    if (path.isEmpty()) return;

    if (exportSVGFrame(doc, frame, path)) {
        statusBar()->showMessage("SVG exported: " + path, 5000);
    } else {
        QMessageBox::warning(this, "Error", "Failed to export SVG.");
    }
}

void MainWindowV2::exportSVGAllFrames()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "Export All Frames as SVG");
    if (dir.isEmpty()) return;

    auto& doc = appState_->document();
    int total = doc.totalFrames();
    statusBar()->showMessage("Exporting SVG frames...");
    showBusy("Exportando SVG...");

    if (exportSVGFrames(doc, dir)) {
        hideBusy();
        statusBar()->showMessage(QString("SVG exported: %1 frames → %2").arg(total).arg(dir), 5000);
    } else {
        hideBusy();
        QMessageBox::warning(this, "Error", "Failed to export some SVG frames.");
    }
}

void MainWindowV2::syncAudioToDocument()
{
    auto& doc = appState_->document();
    doc.clearAudioTracks();
    if (!timeline_panel_) return;
    for (auto* w : timeline_panel_->audioTrackWidgets()) {
        AudioTrackData at;
        at.filepath    = w->filepath().toStdString();
        at.displayName = w->displayName().toStdString();
        at.muted       = w->isMuted();
        at.volume      = w->volume();
        at.layoutPosition = timeline_panel_->widgetLayoutPosition(w);
        QFileInfo fi(w->filepath());
        at.zipEntry = QString("audio/track_%1.%2")
            .arg(doc.audioTracks().size())
            .arg(fi.suffix().toLower()).toStdString();
        doc.addAudioTrack(at);
    }
}

void MainWindowV2::syncVideoToDocument()
{
    auto& doc = appState_->document();
    doc.clearVideoTracks();
    if (!timeline_panel_) return;
    for (auto* w : timeline_panel_->videoTrackWidgets()) {
        VideoTrackData vt;
        vt.filepath    = w->filepath().toStdString();
        vt.displayName = w->displayName().toStdString();
        vt.muted       = w->isMuted();
        vt.volume      = w->volume();
        vt.opacity     = w->opacity();
        vt.width       = w->videoWidth();
        vt.height      = w->videoHeight();
        vt.fps         = static_cast<int>(w->videoFps());
        vt.totalFrames = w->totalFrames();
        vt.layoutPosition = timeline_panel_->widgetLayoutPosition(w);
        QFileInfo fi(w->filepath());
        vt.zipEntry = QString("video/track_%1.%2")
            .arg(doc.videoTracks().size())
            .arg(fi.suffix().toLower()).toStdString();
        doc.addVideoTrack(vt);
    }
}

} // namespace fap
