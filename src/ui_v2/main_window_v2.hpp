#pragma once
#include <QtWidgets/QMainWindow>
#include <memory>
#include "core/app_state.hpp"

class QAction;

namespace fap {

class CanvasWidgetV2;
class TimelinePanelV2;
class ToolboxPanelV2;
class ColorPanelV2;
class LayerPanelV2;
class PropertyEditorV2;

class MainWindowV2 : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindowV2(std::shared_ptr<AppState> state, QWidget* parent = nullptr);
    ~MainWindowV2() override;

    void openProject(const QString& path);

private:
    void setupUI();
    void setupTopBar();
    void setupDocks();
    void setupStatusBar();
    void setupConnections();
    void updateUIState();

    void newProject();
    void openProjectDialog();
    void saveProject();
    void saveProjectAs();
    void exportVideo();
    void exportSVGCurrentFrame();
    void exportSVGAllFrames();
    void syncAudioToDocument();
    void closeEvent(QCloseEvent* event) override;

    std::shared_ptr<AppState> appState_;

    CanvasWidgetV2* canvas_ = nullptr;
    TimelinePanelV2* timeline_panel_ = nullptr;
    ToolboxPanelV2* toolbox_panel_ = nullptr;
    ColorPanelV2* color_panel_ = nullptr;
    LayerPanelV2* layer_panel_ = nullptr;
    PropertyEditorV2* property_editor_ = nullptr;

    QAction* undoAct_ = nullptr;
    QAction* redoAct_ = nullptr;

    bool saving_ = false;
};

} // namespace fap
