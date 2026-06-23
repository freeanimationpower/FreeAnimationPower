#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QString>
#include <QtGui/QColor>
#include "core/layer.hpp"

class QListWidget;
class QComboBox;
class QPushButton;
class QLabel;

namespace fap {

class Document;
class Layer;

class LayerPanel : public QWidget {
    Q_OBJECT
public:
    explicit LayerPanel(QWidget* parent = nullptr);

    void setDocument(Document* doc);
    void setFrame(int frame);
    int currentFrame() const { return currentFrame_; }
    Document* document() const { return doc_; }
    bool isLayerLocked(int index) const;
    int currentModelIndex() const;
    Layer* currentLayer() const;
    LayerUid currentLayerUid() const;
    QListWidget* listWidget() const { return list_widget_; }

    void updateLayerRow(Layer* layer);
    void syncBlendCombo(Layer* layer);
    static void updateLayerRowWidgets(QPushButton* visBtn, QLabel* nameLabel,
                                      QPushButton* lockBtn, Layer* layer);

signals:
    void layerChanged(int index);

private:
    void refreshLayerList();
    GroupLayer& root() const;
    void importImage();

    Document* doc_ = nullptr;
    int currentFrame_ = 0;
    QListWidget* list_widget_ = nullptr;
    QComboBox* blend_combo_ = nullptr;
};

} // namespace fap
