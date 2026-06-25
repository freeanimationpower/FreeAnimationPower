#pragma once

#include <QMainWindow>
#include <memory>

namespace fap {

class Document;
class BrushEngine;
class ToolState;
class CanvasWidget;
class TimelinePanel;
class ToolboxPanel;
class ColorPanel;
class LayerPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void newProject();
    void openProject();
    void openProject(const QString& path);
    void saveProject();
    void saveProjectAs();
    void exportVideo();

private:
    void setupDockWidgets();
    void setupStatusBar();
    void setupConnections();
    void setupMenuBar();
    void undo();
    void redo();
    void purgeUndo();
    void purgeAllMemory();
    void updateFrameThumbnail(int frame);
    void updateAllThumbnails();

    std::unique_ptr<Document> document_;
    std::unique_ptr<BrushEngine> brush_engine_;
    std::unique_ptr<ToolState> tool_state_;

    CanvasWidget* canvas_ = nullptr;
    TimelinePanel* timeline_panel_ = nullptr;
    ToolboxPanel* toolbox_panel_ = nullptr;
    ColorPanel* color_panel_ = nullptr;
    LayerPanel* layer_panel_ = nullptr;

    QAction* new_action_ = nullptr;
    QAction* open_action_ = nullptr;
    QAction* save_action_ = nullptr;
    QAction* save_as_action_ = nullptr;
    QAction* export_action_ = nullptr;
};

} // namespace fap
