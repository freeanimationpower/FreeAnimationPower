#include "ui/main_window.hpp"
#include "ui/timeline_panel.hpp"
#include "ui/toolbox_panel.hpp"
#include "ui/color_panel.hpp"
#include "ui/layer_panel.hpp"
#include "ui/canvas_widget.hpp"
#include "core/document.hpp"
#include "core/tool_state.hpp"
#include "io/document_manager.hpp"
#include "engine/brush/brush_engine.hpp"
#include "core/types.hpp"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QDockWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QScrollArea>
#include <QPainter>
#include <QImage>
#include <QTemporaryDir>
#include <QProcess>
#include <QApplication>

namespace fap {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    document_ = std::make_unique<Document>();
    brush_engine_ = std::make_unique<BrushEngine>();
    tool_state_ = std::make_unique<ToolState>();

    setWindowTitle("FreeAnimation2dStyle - Untitled.fap");
    setMinimumSize(1024, 600);
    resize(1400, 900);

    setupDockWidgets();
    setupStatusBar();
    setupConnections();
    setupMenuBar();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupDockWidgets()
{
    auto* toolbox_scroll = new QScrollArea(this);
    toolbox_panel_ = new ToolboxPanel(tool_state_.get(), this);
    toolbox_scroll->setWidget(toolbox_panel_);
    toolbox_scroll->setWidgetResizable(true);
    auto* toolbox_dock = new QDockWidget("Tools", this);
    toolbox_dock->setWidget(toolbox_scroll);
    toolbox_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::LeftDockWidgetArea, toolbox_dock);

    timeline_panel_ = new TimelinePanel(this);
    auto* timeline_dock = new QDockWidget("Timeline", this);
    timeline_dock->setWidget(timeline_panel_);
    timeline_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    timeline_dock->setMinimumHeight(120);
    timeline_dock->setMaximumHeight(200);
    addDockWidget(Qt::BottomDockWidgetArea, timeline_dock);

    layer_panel_ = new LayerPanel(this);
    auto* layer_dock = new QDockWidget("Layers", this);
    layer_dock->setWidget(layer_panel_);
    layer_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    layer_dock->setMaximumWidth(260);
    addDockWidget(Qt::RightDockWidgetArea, layer_dock);

    auto* color_scroll = new QScrollArea(this);
    color_panel_ = new ColorPanel(this);
    color_scroll->setWidget(color_panel_);
    color_scroll->setWidgetResizable(true);
    auto* color_dock = new QDockWidget("Color", this);
    color_dock->setWidget(color_scroll);
    color_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::RightDockWidgetArea, color_dock);

    canvas_ = new CanvasWidget(document_.get(), brush_engine_.get(), this);
    canvas_->setMinimumSize(400, 300);
    setCentralWidget(canvas_);

    layer_panel_->setDocument(document_.get());
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready");

    auto* hint_label = new QLabel(
        "Shortcuts: B=Brush E=Eraser G=Fill I=Picker L=Line "
        "M=Move S=Select H=Hand [ ]=Size Ctrl+Z=Undo Ctrl+Y=Redo",
        this);
    statusBar()->addPermanentWidget(hint_label);
}

void MainWindow::setupConnections()
{
    QObject::connect(tool_state_.get(), &ToolState::activeToolChanged, this,
        [this](ToolType tool) { canvas_->setTool(static_cast<int>(tool)); });

    QObject::connect(tool_state_.get(), &ToolState::primaryColorChanged, this,
        [this](const QColor& color) {
            canvas_->setColor(color);
        });

    QObject::connect(timeline_panel_, &TimelinePanel::frameChanged, this,
        [this](int frame) {
            canvas_->setCurrentFrame(frame - 1);
            layer_panel_->setFrame(frame);
            statusBar()->showMessage(
                QString("Frame: %1").arg(frame));
        });

    QObject::connect(timeline_panel_, &TimelinePanel::frameCountChanged, this,
        [this](int count) {
            canvas_->setTotalFrames(count);
            document_->setTotalFrames(count);
        });

    QObject::connect(layer_panel_, &LayerPanel::layerChanged, this,
        [this](int modelIdx) {
            canvas_->setCurrentLayer(modelIdx);
            auto* layer = layer_panel_->currentLayer();
            if (layer) {
                layer_panel_->syncBlendCombo(layer);
                statusBar()->showMessage(
                    QString("Layer: %1").arg(QString::fromStdString(layer->name())));
            }
        });

    QObject::connect(toolbox_panel_, &ToolboxPanel::canvasResized, this,
        [this](int width, int height) {
            canvas_->resizeCanvas(width, height);
            canvas_->fit();
            canvas_->update();
        });

    QObject::connect(tool_state_.get(), &ToolState::toolSettingsChanged, this,
        [this]() {
            BrushPreset preset;
            preset.size = static_cast<float>(tool_state_->brushSize());
            preset.opacity = tool_state_->brushOpacity() / 100.0f;
            preset.dynamics.pressure_size = tool_state_->pressureSize();
            preset.dynamics.pressure_opacity = tool_state_->pressureOpacity();
            preset.dynamics.smoothing = tool_state_->stabilizerLevel() / 100.0f;
            auto qc = tool_state_->primaryColor();
            preset.color = Color::fromRGBA(
                static_cast<uint8_t>(qc.red()),
                static_cast<uint8_t>(qc.green()),
                static_cast<uint8_t>(qc.blue()),
                static_cast<uint8_t>(qc.alpha()));
            brush_engine_->setPreset(preset);
        });

    QObject::connect(timeline_panel_, &TimelinePanel::frameInserted, this,
        [this](int atFrame) {
            canvas_->shiftFrameData(atFrame, 1);
            document_->shiftFrameData(atFrame, 1);
        });

    QObject::connect(timeline_panel_, &TimelinePanel::frameRemoved, this,
        [this](int atFrame) {
            canvas_->removeFrameData(atFrame);
            canvas_->shiftFrameData(atFrame, -1);
            document_->removeFrameData(atFrame);
            document_->shiftFrameData(atFrame, -1);
        });

    QObject::connect(timeline_panel_, &TimelinePanel::frameDuplicated, this,
        [this](int sourceFrame, int newFrame) {
            QImage composite = canvas_->compositeFrame(sourceFrame - 1);
            auto* layer = layer_panel_->currentLayer();
            if (layer) {
                canvas_->writeLayerPixels(layer, composite);
            }
            updateFrameThumbnail(newFrame - 1);
        });

    QObject::connect(canvas_, &CanvasWidget::canvasUpdated, this,
        [this]() {
            canvas_->update();
            updateFrameThumbnail(canvas_->currentFrame());
        });

    QObject::connect(tool_state_.get(), &ToolState::onionEnabledChanged, this,
        [this](bool v) { canvas_->setOnionEnabled(v); });

    QObject::connect(tool_state_.get(), &ToolState::onionPrevFramesChanged, this,
        [this](int v) {
            canvas_->setOnionSettings(v, canvas_->onionNext(), canvas_->onionOpacity());
            timeline_panel_->setOnionSkin(v, tool_state_->onionNextFrames());
        });

    QObject::connect(tool_state_.get(), &ToolState::onionNextFramesChanged, this,
        [this](int v) {
            canvas_->setOnionSettings(canvas_->onionPrev(), v, canvas_->onionOpacity());
            timeline_panel_->setOnionSkin(tool_state_->onionPrevFrames(), v);
        });

    QObject::connect(tool_state_.get(), &ToolState::onionOpacityChanged, this,
        [this](int v) { canvas_->setOnionSettings(canvas_->onionPrev(), canvas_->onionNext(), v / 100.0f); });

    QObject::connect(canvas_, &CanvasWidget::togglePlayPause, this,
        [this]() {
            timeline_panel_->togglePlayback();
        });

    QObject::connect(canvas_, &CanvasWidget::colorPicked, this,
        [this](const QColor& color) {
            tool_state_->setPrimaryColor(color);
            toolbox_panel_->syncFromState();
            color_panel_->previewColor(color);
        });
}

void MainWindow::setupMenuBar()
{
    auto* file_menu = menuBar()->addMenu("&File");

    new_action_ = file_menu->addAction("&New");
    new_action_->setShortcut(QKeySequence("Ctrl+N"));
    QObject::connect(new_action_, &QAction::triggered, this, &MainWindow::newProject);

    open_action_ = file_menu->addAction("&Open");
    open_action_->setShortcut(QKeySequence("Ctrl+O"));
    QObject::connect(open_action_, &QAction::triggered, this,
        static_cast<void (MainWindow::*)()>(&MainWindow::openProject));

    save_action_ = file_menu->addAction("&Save");
    save_action_->setShortcut(QKeySequence("Ctrl+S"));
    QObject::connect(save_action_, &QAction::triggered, this, &MainWindow::saveProject);

    save_as_action_ = file_menu->addAction("Save &As");
    QObject::connect(save_as_action_, &QAction::triggered, this, &MainWindow::saveProjectAs);

    file_menu->addSeparator();

    export_action_ = file_menu->addAction("&Export Video");
    export_action_->setShortcut(QKeySequence("Ctrl+E"));
    QObject::connect(export_action_, &QAction::triggered, this, &MainWindow::exportVideo);

    file_menu->addSeparator();

    auto* exit_action = file_menu->addAction("E&xit");
    QObject::connect(exit_action, &QAction::triggered, this, &QMainWindow::close);

    auto* edit_menu = menuBar()->addMenu("&Edit");

    auto* undo_action = edit_menu->addAction("&Undo");
    undo_action->setShortcut(QKeySequence("Ctrl+Z"));
    QObject::connect(undo_action, &QAction::triggered, this, [this]() {
        undo();
    });

    auto* redo_action = edit_menu->addAction("&Redo");
    redo_action->setShortcut(QKeySequence("Ctrl+Y"));
    QObject::connect(redo_action, &QAction::triggered, this, [this]() {
        redo();
    });

    edit_menu->addSeparator();

    auto* purge_menu = edit_menu->addMenu("Pur&ge");

    auto* purge_undo_action = purge_menu->addAction("&Undo History");
    QObject::connect(purge_undo_action, &QAction::triggered, this, &MainWindow::purgeUndo);

    auto* purge_all_action = purge_menu->addAction("&All Memory");
    QObject::connect(purge_all_action, &QAction::triggered, this, &MainWindow::purgeAllMemory);
}

void MainWindow::undo() {
    canvas_->undo();
    canvas_->update();
}

void MainWindow::redo() {
    canvas_->redo();
    canvas_->update();
}

void MainWindow::purgeUndo() {
    canvas_->undoStack()->clear();
    canvas_->update();
    statusBar()->showMessage(QStringLiteral("Undo history purged"), 3000);
}

void MainWindow::purgeAllMemory() {
    canvas_->purgeMemory();
    timeline_panel_->purgeThumbnails();
    updateAllThumbnails();
    statusBar()->showMessage(QStringLiteral("Memory purged"), 3000);
}

void MainWindow::newProject()
{
    document_ = std::make_unique<Document>();
    brush_engine_ = std::make_unique<BrushEngine>();
    tool_state_->resetToDefaults();
    canvas_->setDocumentAndBrush(document_.get(), brush_engine_.get());
    canvas_->setCurrentFrame(0);
    canvas_->setTotalFrames(document_->totalFrames());
    canvas_->setCurrentLayer(0);
    canvas_->fit();
    canvas_->update();

    layer_panel_->setDocument(document_.get());
    layer_panel_->setFrame(1);

    timeline_panel_->setTotalFrames(document_->totalFrames());
    timeline_panel_->setCurrentFrame(1);

    setWindowTitle("FreeAnimation2dStyle - Untitled.fap");
    updateAllThumbnails();
    toolbox_panel_->syncFromState();
}

void MainWindow::openProject()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Open Project", QString(),
        "Free Animation Projects (*.fa2d);;All Files (*)");
    if (!path.isEmpty()) {
        openProject(path);
    }
}

void MainWindow::openProject(const QString& path)
{
    DocumentManager mgr;
    if (!mgr.load(*document_, path)) {
        QMessageBox::warning(this, "Open Failed",
            QString("Could not open project:\n%1").arg(mgr.lastError()));
        return;
    }

    canvas_->setDocumentAndBrush(document_.get(), brush_engine_.get());
    canvas_->setCurrentFrame(0);
    canvas_->setTotalFrames(document_->totalFrames());
    canvas_->setCurrentLayer(0);
    canvas_->fit();
    canvas_->update();

    layer_panel_->setDocument(document_.get());
    layer_panel_->setFrame(1);

    timeline_panel_->setTotalFrames(document_->totalFrames());
    timeline_panel_->setCurrentFrame(1);

    setWindowTitle(QString("FreeAnimation2dStyle - %1").arg(path));
    updateAllThumbnails();
    toolbox_panel_->syncFromState();
}

void MainWindow::saveProject()
{
    if (!document_->filepath().empty()) {
        DocumentManager mgr;
        if (!mgr.save(*document_, QString::fromStdString(document_->filepath()))) {
            QMessageBox::warning(this, "Save Failed",
                QString("Could not save project:\n%1").arg(mgr.lastError()));
        }
        document_->setModified(false);
    } else {
        saveProjectAs();
    }
}

void MainWindow::saveProjectAs()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Save Project As", QString(),
        "Free Animation Projects (*.fa2d);;All Files (*)");
    if (path.isEmpty()) return;

    if (!path.endsWith(".fa2d")) {
        path += ".fa2d";
    }

    DocumentManager mgr;
    if (!mgr.save(*document_, path)) {
        QMessageBox::warning(this, "Save Failed",
            QString("Could not save project:\n%1").arg(mgr.lastError()));
        return;
    }

    document_->setModified(false);
    setWindowTitle(QString("FreeAnimation2dStyle - %1").arg(path));
}

void MainWindow::exportVideo()
{
    QTemporaryDir tmp_dir;
    if (!tmp_dir.isValid()) {
        QMessageBox::warning(this, "Export Failed", "Could not create temporary directory.");
        return;
    }

    QString dir_path = tmp_dir.path();
    int total = document_->totalFrames();

    for (int f = 0; f < total; ++f) {
        QImage frame = canvas_->compositeFrame(f);
        QString filename = QString("%1/frame_%2.png")
            .arg(dir_path)
            .arg(f, 5, 10, QChar('0'));
        frame.save(filename, "PNG");
    }

    QString output_path = QFileDialog::getSaveFileName(
        this, "Export Video", QString(),
        "MP4 Video (*.mp4);;All Files (*)");
    if (output_path.isEmpty()) return;

    QProcess ffmpeg;
    ffmpeg.setProgram("ffmpeg");
    ffmpeg.setArguments({
        "-y",
        "-framerate", QString::number(document_->fps()),
        "-i", QString("%1/frame_%05d.png").arg(dir_path),
        "-c:v", "libx264",
        "-preset", "medium",
        "-crf", "18",
        "-pix_fmt", "yuv420p",
        output_path
    });

    ffmpeg.start();
    if (!ffmpeg.waitForFinished(120000)) {
        QMessageBox::warning(this, "Export Failed", "FFmpeg process timed out.");
        return;
    }

    if (ffmpeg.exitCode() != 0) {
        QMessageBox::warning(this, "Export Failed",
            QString("FFmpeg error:\n%1").arg(QString::fromUtf8(ffmpeg.readAllStandardError())));
        return;
    }

    QMessageBox::information(this, "Export Complete",
        QString("Video exported to:\n%1").arg(output_path));
}

void MainWindow::updateFrameThumbnail(int frame)
{
    QImage composite = canvas_->compositeFrame(frame);
    QImage thumb = composite.scaled(76, 38, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    timeline_panel_->setFrameThumbnail(frame, thumb);
}

void MainWindow::updateAllThumbnails()
{
    int total = document_->totalFrames();
    for (int f = 0; f < total; ++f) {
        updateFrameThumbnail(f);
    }
}

} // namespace fap
