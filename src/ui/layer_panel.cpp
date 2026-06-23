#include "layer_panel.hpp"
#include "core/document.hpp"
#include "core/layer.hpp"
#include "ui/icons.hpp"
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QListWidgetItem>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtCore/QFileInfo>
#include <QtGui/QImageReader>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QSignalBlocker>
#include <algorithm>
#include <deque>

namespace fap {

static const int kLayerUidRole = Qt::UserRole + 1;

static LayerUid itemLayerUid(QListWidgetItem* item) {
    return item->data(kLayerUidRole).value<quintptr>();
}

static void setItemLayerUid(QListWidgetItem* item, LayerUid uid) {
    item->setData(kLayerUidRole, QVariant::fromValue(static_cast<quintptr>(uid)));
}

static QWidget* makeLayerRow(Layer* layer, LayerPanel* panel) {
    auto* row = new QFrame();
    row->setObjectName("layerRow");
    row->setAutoFillBackground(true);
    row->setCursor(Qt::PointingHandCursor);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(4, 1, 4, 1);
    lay->setSpacing(4);

    auto* visBtn = new QPushButton();
    visBtn->setFixedSize(22, 22);
    visBtn->setFlat(true);
    visBtn->setFocusPolicy(Qt::NoFocus);
    visBtn->setStyleSheet("QPushButton{background:transparent;border:none;}QPushButton:hover{background:#3a3a3d;}");
    LayerUid layerUid = layer->uid();
    QObject::connect(visBtn, &QPushButton::clicked, panel, [panel, layerUid]() {
        if (!panel->document()) return;
        Layer* l = panel->document()->rootLayerForFrame(panel->currentFrame()).layerByUid(layerUid);
        if (!l) return;
        l->setVisible(!l->visible());
        panel->updateLayerRow(l);
    });
    lay->addWidget(visBtn);

    auto* nameLabel = new QLabel();
    nameLabel->setCursor(Qt::PointingHandCursor);
    lay->addWidget(nameLabel, 1);

    auto* lockBtn = new QPushButton();
    lockBtn->setFixedSize(22, 22);
    lockBtn->setFlat(true);
    lockBtn->setFocusPolicy(Qt::NoFocus);
    lockBtn->setStyleSheet("QPushButton{background:transparent;border:none;}QPushButton:hover{background:#3a3a3d;}");
    QObject::connect(lockBtn, &QPushButton::clicked, panel, [panel, layerUid]() {
        if (!panel->document()) return;
        Layer* l = panel->document()->rootLayerForFrame(panel->currentFrame()).layerByUid(layerUid);
        if (!l) return;
        l->setLocked(!l->locked());
        panel->updateLayerRow(l);
    });
    lay->addWidget(lockBtn);

    panel->updateLayerRowWidgets(visBtn, nameLabel, lockBtn, layer);
    return row;
}

LayerPanel::LayerPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* headerRow = new QHBoxLayout();
    auto* titleLabel = new QLabel("Layers");
    titleLabel->setStyleSheet("color: #75beff; font-weight: bold; font-size: 12px;");
    headerRow->addWidget(titleLabel);
    headerRow->addStretch();

    auto* addBtn = new QPushButton();
    addBtn->setIcon(icons::NewLayer());
    addBtn->setIconSize(QSize(16, 16));
    addBtn->setFixedSize(28, 24);
    addBtn->setToolTip("Add new layer on top");
    addBtn->setStyleSheet("QPushButton{background:#3c3c3c;color:#cccccc;border:1px solid #3c3c3c;border-radius:3px;}QPushButton:hover{background:#4a4a4a;}");
    QObject::connect(addBtn, &QPushButton::clicked, [this]() {
        if (!doc_) return;
        int w = doc_->width(), h = doc_->height();
        auto& lroot = this->root();
        int curRow = list_widget_->currentRow();
        int modelIdx = curRow >= 0 ? static_cast<int>(lroot.layerCount()) - curRow
                                   : static_cast<int>(lroot.layerCount());
        lroot.addLayer(std::make_unique<RasterLayer>(
            QString("Layer %1").arg(lroot.layerCount() + 1).toStdString(), w, h));
        refreshLayerList();
        int newRow = curRow >= 0 ? curRow : 0;
        list_widget_->setCurrentRow(newRow);
        emit layerChanged(currentModelIndex());
    });
    headerRow->addWidget(addBtn);
    layout->addLayout(headerRow);

    list_widget_ = new QListWidget();
    list_widget_->setDragDropMode(QAbstractItemView::InternalMove);
    list_widget_->setDefaultDropAction(Qt::MoveAction);
    list_widget_->setDragEnabled(true);
    list_widget_->setAcceptDrops(true);
    list_widget_->setDropIndicatorShown(true);
    list_widget_->setStyleSheet(
        "QListWidget{background:#252526;color:#e0e0e0;border:1px solid #3e3e42;border-radius:3px;outline:none;}"
        "QListWidget::item{padding:1px 4px;border-bottom:1px solid #333337;}"
        "QListWidget::item:selected{background:#094771;}"
        "QListWidget::item:hover{background:#2a2d2e;}");
    QObject::connect(list_widget_, &QListWidget::currentRowChanged, [this](int) {
        emit layerChanged(currentModelIndex());
    });
    QObject::connect(list_widget_->model(), &QAbstractItemModel::rowsMoved, [this](const QModelIndex&, int, int, const QModelIndex&, int) {
        if (!doc_) return;
        auto& lroot = this->root();
        int n = static_cast<int>(lroot.layerCount());
        if (n != list_widget_->count()) return;

        LayerUid activeUid = currentLayer() ? currentLayer()->uid() : 0;

        std::deque<std::unique_ptr<Layer>> newOrder;
        for (int listRow = n - 1; listRow >= 0; --listRow) {
            LayerUid uid = itemLayerUid(list_widget_->item(listRow));
            Layer* layer = lroot.layerByUid(uid);
            if (!layer) continue;
            auto it = std::find_if(lroot.layers().begin(), lroot.layers().end(),
                [layer](const auto& uptr) { return uptr.get() == layer; });
            if (it != lroot.layers().end()) {
                newOrder.push_back(std::move(*it));
                lroot.layers().erase(it);
            }
        }
        lroot.layers() = std::move(newOrder);

        if (activeUid != 0) {
            Layer* restored = lroot.layerByUid(activeUid);
            if (restored) {
                int newModelIdx = lroot.indexOfLayer(restored);
                emit layerChanged(newModelIdx);
                return;
            }
        }
        emit layerChanged(currentModelIndex());
    });
    QObject::connect(list_widget_, &QListWidget::itemDoubleClicked, [this](QListWidgetItem* item) {
        if (!doc_) return;
        LayerUid uid = itemLayerUid(item);
        Layer* layer = this->root().layerByUid(uid);
        if (!layer) return;

        auto* row = qobject_cast<QWidget*>(list_widget_->itemWidget(item));
        if (!row || !row->layout() || row->layout()->count() < 3) return;
        auto* nameLabel = qobject_cast<QLabel*>(row->layout()->itemAt(1)->widget());
        if (!nameLabel) return;

        auto* editor = new QLineEdit(row);
        editor->setText(QString::fromStdString(layer->name()));
        editor->selectAll();
        editor->setStyleSheet(
            "background:#3c3c3c;color:#e0e0e0;border:1px solid #007acc;"
            "padding:1px 4px;font-size:11px;");
        auto* hlay = qobject_cast<QHBoxLayout*>(row->layout());
        int labelIdx = hlay->indexOf(nameLabel);
        hlay->insertWidget(labelIdx, editor);
        nameLabel->hide();
        editor->setFocus();

        auto committed = std::make_shared<bool>(false);
        auto commitRename = [hlay, editor, nameLabel, layer, this, committed]() {
            if (*committed) return;
            *committed = true;
            QString text = editor->text().trimmed();
            if (!text.isEmpty()) {
                layer->setName(text.toStdString());
            }
            hlay->removeWidget(editor);
            editor->deleteLater();
            nameLabel->show();
            updateLayerRow(layer);
            emit layerChanged(currentModelIndex());
        };
        QObject::connect(editor, &QLineEdit::returnPressed, this, commitRename);
        QObject::connect(editor, &QLineEdit::editingFinished, this, commitRename);
    });

    list_widget_->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(list_widget_, &QListWidget::customContextMenuRequested, [this](const QPoint& pos) {
        QListWidgetItem* item = list_widget_->itemAt(pos);
        if (!item || !doc_) return;
        LayerUid uid = itemLayerUid(item);
        Layer* layer = root().layerByUid(uid);
        if (!layer || layer->type() != LayerType::Raster) return;
        QMenu menu;
        QAction* importAct = menu.addAction("Import Image...");
        if (menu.exec(list_widget_->mapToGlobal(pos)) == importAct) {
            importImage();
        }
    });
    layout->addWidget(list_widget_, 1);

    auto* btnFrame = new QFrame();
    btnFrame->setStyleSheet("QFrame{background:#2d2d30;border:1px solid #3e3e42;border-radius:4px;padding:4px;}");
    auto* btnGrid = new QVBoxLayout(btnFrame);
    btnGrid->setSpacing(2);
    btnGrid->setContentsMargins(2, 2, 2, 2);

    auto makeBtn = [](const QIcon& icon, const QString& label, const QString& tip) {
        auto* btn = new QPushButton(label);
        btn->setIcon(icon);
        btn->setIconSize(QSize(14, 14));
        btn->setToolTip(tip);
        btn->setMinimumHeight(26);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton{background:#3c3c3c;color:#cccccc;border:1px solid #3c3c3c;border-radius:3px;"
            "font-size:11px;text-align:left;padding-left:8px;}"
            "QPushButton:hover{background:#4a4a4a;color:#ffffff;}");
        return btn;
    };

    auto* newBtn = makeBtn(icons::NewLayer(), "  New Layer", "Insert new layer above selected");
    newBtn->setStyleSheet(newBtn->styleSheet().replace("#cccccc", "#88cc88"));
    QObject::connect(newBtn, &QPushButton::clicked, [this]() {
        if (!doc_) return;
        int w = doc_->width(), h = doc_->height();
        auto& lroot = this->root();
        int curRow = list_widget_->currentRow();
        int modelIdx = curRow >= 0 ? static_cast<int>(lroot.layerCount()) - curRow
                                   : static_cast<int>(lroot.layerCount());
        lroot.layers().insert(lroot.layers().begin() + modelIdx,
            std::make_unique<RasterLayer>(
                QString("Layer %1").arg(lroot.layerCount() + 1).toStdString(), w, h));
        refreshLayerList();
        int newRow = curRow >= 0 ? curRow : 0;
        list_widget_->setCurrentRow(newRow);
        emit layerChanged(currentModelIndex());
    });
    btnGrid->addWidget(newBtn);

    auto* newVecBtn = makeBtn(icons::NewLayer(), "  New Vector Layer", "Insert new vector layer above selected");
    newVecBtn->setStyleSheet(newVecBtn->styleSheet().replace("#cccccc", "#88aacc"));
    QObject::connect(newVecBtn, &QPushButton::clicked, [this]() {
        if (!doc_) return;
        auto& lroot = this->root();
        int curRow = list_widget_->currentRow();
        int modelIdx = curRow >= 0 ? static_cast<int>(lroot.layerCount()) - curRow
                                   : static_cast<int>(lroot.layerCount());
        lroot.layers().insert(lroot.layers().begin() + modelIdx,
            std::make_unique<VectorLayer>(
                QString("Vector %1").arg(lroot.layerCount() + 1).toStdString()));
        refreshLayerList();
        int newRow = curRow >= 0 ? curRow : 0;
        list_widget_->setCurrentRow(newRow);
        emit layerChanged(currentModelIndex());
    });
    btnGrid->addWidget(newVecBtn);

    auto* dupBtn = makeBtn(icons::Duplicate(), "  Duplicate", "Duplicate selected layer");
    dupBtn->setStyleSheet(dupBtn->styleSheet().replace("#cccccc", "#88cc88"));
    QObject::connect(dupBtn, &QPushButton::clicked, [this]() {
        if (!doc_) return;
        auto& lroot = this->root();
        int curRow = list_widget_->currentRow();
        if (curRow >= 0) {
            int modelIdx = static_cast<int>(lroot.layerCount()) - 1 - curRow;
            lroot.duplicateLayer(modelIdx);
            refreshLayerList();
            int newRow = std::max(0, curRow - 1);
            list_widget_->setCurrentRow(newRow);
            emit layerChanged(currentModelIndex());
        }
    });
    btnGrid->addWidget(dupBtn);

    auto* upBtn = makeBtn(icons::MoveUp(), "  Move Up", "Move layer toward front");
    QObject::connect(upBtn, &QPushButton::clicked, [this]() {
        if (!doc_) return;
        auto& lroot = this->root();
        int curRow = list_widget_->currentRow();
        if (curRow > 0) {
            int modelIdx = static_cast<int>(lroot.layerCount()) - 1 - curRow;
            lroot.moveLayer(modelIdx, modelIdx + 1);
            refreshLayerList();
            list_widget_->setCurrentRow(curRow - 1);
            emit layerChanged(currentModelIndex());
        }
    });
    btnGrid->addWidget(upBtn);

    auto* downBtn = makeBtn(icons::MoveDown(), "  Move Down", "Move layer toward back");
    QObject::connect(downBtn, &QPushButton::clicked, [this]() {
        if (!doc_) return;
        auto& lroot = this->root();
        int curRow = list_widget_->currentRow();
        if (curRow >= 0 && static_cast<size_t>(curRow + 1) < lroot.layerCount()) {
            int modelIdx = static_cast<int>(lroot.layerCount()) - 1 - curRow;
            lroot.moveLayer(modelIdx, modelIdx - 1);
            refreshLayerList();
            list_widget_->setCurrentRow(curRow + 1);
            emit layerChanged(currentModelIndex());
        }
    });
    btnGrid->addWidget(downBtn);

    auto* delBtn = makeBtn(icons::Delete(), "  Delete", "Delete selected layer");
    delBtn->setStyleSheet(delBtn->styleSheet().replace("#4a4a4a", "#6e3030").replace("#cccccc", "#e05050"));
    QObject::connect(delBtn, &QPushButton::clicked, [this]() {
        if (!doc_) return;
        auto& lroot = this->root();
        int curRow = list_widget_->currentRow();
        if (curRow >= 0 && lroot.layerCount() > 1) {
            int modelIdx = static_cast<int>(lroot.layerCount()) - 1 - curRow;
            Layer* layer = lroot.layers()[modelIdx].get();
            LayerUid layerUid = layer->uid();
            list_widget_->blockSignals(true);
            for (int i = 0; i < list_widget_->count(); ++i) {
                if (itemLayerUid(list_widget_->item(i)) == layerUid) {
                    delete list_widget_->takeItem(i);
                    break;
                }
            }
            lroot.removeLayer(modelIdx);
            list_widget_->blockSignals(false);
            emit layerChanged(currentModelIndex());
        }
    });
    btnGrid->addWidget(delBtn);

    auto* importBtn = makeBtn(icons::ImportImage(), "  Import Image...", "Load PNG or JPG image into selected layer");
    importBtn->setStyleSheet(importBtn->styleSheet().replace("#cccccc", "#d4a844"));
    QObject::connect(importBtn, &QPushButton::clicked, [this]() {
        importImage();
    });
    btnGrid->addWidget(importBtn);

    layout->addWidget(btnFrame);

    auto* blendRow = new QHBoxLayout();
    auto* blendLabel = new QLabel("Blend:");
    blendLabel->setStyleSheet("color:#c0c0c0;font-size:11px;");
    blend_combo_ = new QComboBox();
    blend_combo_->addItems({"Normal","Multiply","Screen","Overlay","Add","Subtract","Darken","Lighten"});
    blend_combo_->setStyleSheet("QComboBox{background:#3c3c3c;color:#cccccc;border:1px solid #3c3c3c;border-radius:3px;padding:3px;}QComboBox QAbstractItemView{background:#252526;color:#cccccc;selection-background-color:#094771;}");
    QObject::connect(blend_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int mode) {
        if (!doc_) return;
        int curRow = list_widget_->currentRow();
        if (curRow >= 0) {
            int modelIdx = static_cast<int>(root().layerCount()) - 1 - curRow;
            root().layers()[modelIdx]->setBlendMode(static_cast<BlendMode>(mode));
            emit layerChanged(currentModelIndex());
        }
    });
    blendRow->addWidget(blendLabel);
    blendRow->addWidget(blend_combo_, 1);
    layout->addLayout(blendRow);
}

void LayerPanel::setDocument(Document* doc) {
    doc_ = doc;
    if (doc_) {
        refreshLayerList();
        if (list_widget_->count() > 0) list_widget_->setCurrentRow(0);
    }
}

void LayerPanel::setFrame(int frame) {
    int zeroBased = frame - 1;
    if (currentFrame_ == zeroBased) return;
    currentFrame_ = zeroBased;
    if (doc_) {
        list_widget_->clear();
        refreshLayerList();
        if (list_widget_->count() > 0) list_widget_->setCurrentRow(0);
        emit layerChanged(currentModelIndex());
    }
}

GroupLayer& LayerPanel::root() const {
    return doc_->rootLayerForFrame(currentFrame_);
}

bool LayerPanel::isLayerLocked(int index) const {
    if (!doc_) return false;
    auto& lroot = this->root();
    if (index >= 0 && static_cast<size_t>(index) < lroot.layerCount())
        return lroot.layerAt(index)->locked();
    return false;
}

int LayerPanel::currentModelIndex() const {
    if (!doc_) return 0;
    int listRow = list_widget_->currentRow();
    int n = static_cast<int>(root().layerCount());
    if (listRow >= 0 && n > 0)
        return n - 1 - listRow;
    return 0;
}

Layer* LayerPanel::currentLayer() const {
    int row = list_widget_->currentRow();
    if (!doc_ || row < 0 || row >= list_widget_->count()) return nullptr;
    return root().layerByUid(itemLayerUid(list_widget_->item(row)));
}

LayerUid LayerPanel::currentLayerUid() const {
    int row = list_widget_->currentRow();
    if (!doc_ || row < 0 || row >= list_widget_->count()) return 0;
    return itemLayerUid(list_widget_->item(row));
}

void LayerPanel::syncBlendCombo(Layer* layer) {
    if (!layer || !blend_combo_) return;
    QSignalBlocker block(blend_combo_);
    blend_combo_->setCurrentIndex(static_cast<int>(layer->blendMode()));
}

void LayerPanel::updateLayerRow(Layer* layer) {
    if (!doc_ || !layer) return;
    for (int i = 0; i < list_widget_->count(); ++i) {
        auto* item = list_widget_->item(i);
        if (itemLayerUid(item) == layer->uid()) {
            auto* w = list_widget_->itemWidget(item);
            if (w && w->layout() && w->layout()->count() >= 3) {
                auto* visBtn = qobject_cast<QPushButton*>(w->layout()->itemAt(0)->widget());
                auto* nameLabel = qobject_cast<QLabel*>(w->layout()->itemAt(1)->widget());
                auto* lockBtn = qobject_cast<QPushButton*>(w->layout()->itemAt(2)->widget());
                if (visBtn && nameLabel && lockBtn)
                    updateLayerRowWidgets(visBtn, nameLabel, lockBtn, layer);
            }
            if (layer->locked()) {
                item->setBackground(QColor(185, 50, 50));
                if (w)
                    w->setStyleSheet("QFrame#layerRow{background-color:#b92d2d; border-radius:2px;}");
            } else {
                item->setBackground(QColor(37, 37, 38));
                if (w)
                    w->setStyleSheet("QFrame#layerRow{background-color:#252526;}");
            }
            return;
        }
    }
}

void LayerPanel::updateLayerRowWidgets(QPushButton* visBtn, QLabel* nameLabel,
                                        QPushButton* lockBtn, Layer* layer) {
    visBtn->setIcon(layer->visible() ? icons::Eye()
                                     : icons::tinted(icons::Eye(), QColor("#555")));
    visBtn->setToolTip(layer->visible() ? "Hide layer" : "Show layer");
    nameLabel->setText(QString::fromStdString(layer->name()));
    nameLabel->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;")
        .arg(layer->locked() ? "#ffffff" : (layer->visible() ? "#e0e0e0" : "#666")));
    lockBtn->setIcon(layer->locked() ? icons::LockClosed() : icons::LockOpen());
    lockBtn->setToolTip(layer->locked() ? "Unlock layer" : "Lock layer");
}

void LayerPanel::refreshLayerList() {
    if (!doc_) return;
    auto& lroot = this->root();
    int n = static_cast<int>(lroot.layerCount());
    int prevRow = list_widget_->currentRow();

    list_widget_->blockSignals(true);

    while (list_widget_->count() > n) {
        delete list_widget_->takeItem(list_widget_->count() - 1);
    }

    for (int listIdx = 0; listIdx < n; ++listIdx) {
        int modelIdx = n - 1 - listIdx;
        Layer* layer = lroot.layers()[modelIdx].get();
        LayerUid layerUid = layer->uid();

        QListWidgetItem* item = nullptr;
        if (listIdx < list_widget_->count()) {
            item = list_widget_->item(listIdx);
            if (itemLayerUid(item) != layerUid) {
                bool foundElsewhere = false;
                for (int j = listIdx + 1; j < list_widget_->count(); ++j) {
                    if (itemLayerUid(list_widget_->item(j)) == layerUid) {
                        delete list_widget_->takeItem(j);
                        foundElsewhere = true;
                        break;
                    }
                }
                if (!foundElsewhere) {
                    delete list_widget_->takeItem(listIdx);
                    item = nullptr;
                    --listIdx;
                    continue;
                }
                item = nullptr;
            }
        }

        if (!item) {
            item = new QListWidgetItem();
            item->setSizeHint(QSize(0, 30));
            setItemLayerUid(item, layerUid);
            if (listIdx < list_widget_->count())
                list_widget_->insertItem(listIdx, item);
            else
                list_widget_->addItem(item);
            auto* row = makeLayerRow(layer, const_cast<LayerPanel*>(this));
            list_widget_->setItemWidget(item, row);
        } else {
            if (item->listWidget() != list_widget_) {
                list_widget_->insertItem(listIdx, list_widget_->takeItem(list_widget_->row(item)));
            }
            updateLayerRow(layer);
        }

        if (layer->locked()) {
            item->setBackground(QColor(185, 50, 50));
            if (auto* w = list_widget_->itemWidget(item))
                w->setStyleSheet("QFrame#layerRow{background-color:#b92d2d; border-radius:2px;}");
        } else {
            item->setBackground(QColor(37, 37, 38));
            if (auto* w = list_widget_->itemWidget(item))
                w->setStyleSheet("QFrame#layerRow{background-color:#252526;}");
        }
    }

    list_widget_->blockSignals(false);

    if (prevRow >= 0 && prevRow < list_widget_->count())
        list_widget_->setCurrentRow(prevRow);
}

void LayerPanel::importImage() {
    if (!doc_) return;

    QString path = QFileDialog::getOpenFileName(
        this, "Import Image", QString(),
        "Images (*.png *.jpg *.jpeg *.bmp);;PNG (*.png);;JPEG (*.jpg *.jpeg);;All Files (*)");
    if (path.isEmpty()) return;

    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, "Import Error", "Failed to load image: " + path);
        return;
    }

    Layer* current = currentLayer();
    RasterLayer* target = nullptr;
    bool createdNew = false;

    if (current && current->type() == LayerType::Raster) {
        target = static_cast<RasterLayer*>(current);
    } else {
        QString name = QFileInfo(path).baseName();
        auto newLayer = std::make_unique<RasterLayer>(name.toStdString(), img.width(), img.height());
        target = newLayer.get();
        auto& lroot = root();
        lroot.addLayer(std::move(newLayer));
        createdNew = true;
    }

    if (img.width() != target->width() || img.height() != target->height()) {
        target->resize(img.width(), img.height());
    }

    QImage converted = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < converted.height(); ++y) {
        const uint32_t* src = reinterpret_cast<const uint32_t*>(converted.constScanLine(y));
        uint32_t* dst = target->pixelData();
        if (src && dst) {
            std::copy(src, src + converted.width(), dst + y * target->width());
        }
    }
    target->bufferEpochTick();

    refreshLayerList();

    if (createdNew) {
        list_widget_->setCurrentRow(0);
    }
    emit layerChanged(currentModelIndex());
}

} // namespace fap
#include "layer_panel.moc"
