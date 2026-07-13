#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <memory>

class QListWidget;
class QComboBox;
class QPushButton;

namespace fap {

class AppState;
class Document;
class Layer;
class GroupLayer;
using LayerUid = uint64_t;

class LayerPanelV2 : public QWidget {
    Q_OBJECT
public:
    explicit LayerPanelV2(std::shared_ptr<AppState> state, QWidget* parent = nullptr);

    void setCurrentFrame(int frame);
    void refreshLayerList();

    int currentLayerIndex() const { return currentLayerIndex_; }
    LayerUid currentLayerUid() const;

signals:
    void layerChanged(int index);
    void layerUidChanged(LayerUid uid);
    void layerDisplayPropertiesChanged();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void addRasterLayer();
    void duplicateLayer();
    void moveLayerUp();
    void moveLayerDown();
    void deleteLayer();
    void toggleLayerVisibility(int index);
    void toggleLayerLock(int index);
    void copyLayerToAllFrames();
    void onBlendModeChanged(int mode);
    void onRowSelected(int row);

    QWidget* createLayerRow(int index, const Layer* layer);
    void startRename(int index, QWidget* row);

    std::shared_ptr<AppState> appState_;
    QListWidget* list_ = nullptr;
    QComboBox* blendCombo_ = nullptr;
    int currentFrame_ = 0;
    int currentLayerIndex_ = 0;
};

} // namespace fap
