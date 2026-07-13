#include "layer_panel_v2.hpp"
#include "layer_lock_button.hpp"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QListWidgetItem>
#include <QtWidgets/QLineEdit>
#include <QtCore/QPointer>
#include <QtWidgets/QFrame>
#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtGui/QMouseEvent>
#include <QtGui/QIcon>

#include "core/app_state.hpp"
#include "core/document.hpp"
#include "core/layer.hpp"
#include "core/sequence.hpp"
#include "core/types.hpp"
#include "core/undo_manager.hpp"
#include "core/diagnostic/tracer_macros.hpp"

namespace fap {
namespace {

class CopyLayerToFramesCommand : public UndoCommand {
public:
    CopyLayerToFramesCommand(std::shared_ptr<AppState> state,
                              int sourceFrame, int layerIndex,
                              std::vector<std::pair<int, std::unique_ptr<Layer>>> backups)
        : appState_(std::move(state))
        , sourceFrame_(sourceFrame)
        , layerIndex_(layerIndex)
        , backups_(std::move(backups))
    {}

    void undo() override {
        auto& seq = appState_->document().activeSequence();
        for (auto& [frame, saved] : backups_) {
            GroupLayer& root = seq.rootLayerForFrame(frame);
            if (static_cast<int>(root.layerCount()) > layerIndex_) {
                if (saved) {
                    root.layers()[static_cast<size_t>(layerIndex_)] = std::move(saved);
                } else {
                    root.removeLayer(layerIndex_);
                }
            }
        }
        appState_->document().setModified(true);
    }

    void redo() override {
        auto& seq = appState_->document().activeSequence();
        GroupLayer& srcRoot = seq.rootLayerForFrame(sourceFrame_);
        if (layerIndex_ < 0 || layerIndex_ >= static_cast<int>(srcRoot.layerCount())) return;
        const Layer* srcLayer = srcRoot.layerAt(layerIndex_);
        if (!srcLayer) return;

        for (int f = 0; f < seq.totalFrames(); ++f) {
            if (f == sourceFrame_) continue;
            GroupLayer& dstRoot = seq.rootLayerForFrame(f);
            while (static_cast<int>(dstRoot.layerCount()) <= layerIndex_) {
                dstRoot.addLayer(srcLayer->clone());
            }
            Layer* dstLayer = dstRoot.layerAt(layerIndex_);
            if (!dstLayer || dstLayer->type() != srcLayer->type()) continue;
            dstLayer->setName(srcLayer->name());
            dstLayer->setVisible(srcLayer->visible());
            dstLayer->setOpacity(srcLayer->opacity());
            dstLayer->setBlendMode(srcLayer->blendMode());
            dstLayer->setLocked(srcLayer->locked());
            if (srcLayer->type() == LayerType::Raster) {
                const auto* srcRl = static_cast<const RasterLayer*>(srcLayer);
                auto* dstRl = static_cast<RasterLayer*>(dstLayer);
                dstRl->shareDataFrom(*srcRl);
                dstRl->ensureUnique();
                dstRl->setHasContent(srcRl->hasContent());
            }
        }
        appState_->document().setModified(true);
    }

private:
    std::shared_ptr<AppState> appState_;
    int sourceFrame_;
    int layerIndex_;
    std::vector<std::pair<int, std::unique_ptr<Layer>>> backups_;
};

} // anonymous namespace

static const char* kPanelBg = "#1A1D24";
static const char* kTextColor = "#C8CCD8";
static const char* kMutedColor = "#6B7088";
static const char* kAccentColor = "#FF4800";
static const char* kBorderColor = "#2D3139";
static const char* kInputBg = "#252830";
static const char* kBtnBg = "#FF4800";
static const char* kBtnHover = "#FF6A30";
static const char* kDangerHover = "#4A2020";

static const char* kInlineBtnBase =
    "QPushButton{"
    "  background:transparent;"
    "  border:none;"
    "  border-radius:3px;"
    "  font-size:13px;"
    "  padding:0px;"
    "  color:%1;"
    "}"
    "QPushButton:hover{"
    "  background:%2;"
    "}";
static const char* kInlineBtnChecked =
    "QPushButton{"
    "  background:transparent;"
    "  border:none;"
    "  border-radius:3px;"
    "  font-size:13px;"
    "  padding:0px;"
    "  color:%1;"
    "}"
    "QPushButton:hover{"
    "  background:%2;"
    "}"
    "QPushButton:checked{"
    "  color:#FF4800;"
    "}";

static QPushButton* makeIconButton(const QString& iconPath, const QString& tip) {
    auto* btn = new QPushButton();
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(24, 20));
    btn->setFixedSize(28, 24);
    btn->setToolTip(tip);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;border-radius:3px;font-size:13px;}")
            .arg(kBtnBg, kTextColor, kBorderColor) +
        QString("QPushButton:hover{background:%1;border-color:%2;color:#E8ECF0;}")
            .arg(kBtnHover, kMutedColor));
    return btn;
}

LayerPanelV2::LayerPanelV2(std::shared_ptr<AppState> state, QWidget* parent)
    : QWidget(parent)
    , appState_(std::move(state))
{
    setStyleSheet(QString("background:%1;").arg(kPanelBg));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    auto* headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 4);

    headerRow->addStretch();

    auto* addRasterBtn = makeIconButton(":/icons/svg/new_layer.svg",
        "New Raster Layer\nCreate a new pixel-based layer for drawing and painting.");
    QObject::connect(addRasterBtn, &QPushButton::clicked, this, &LayerPanelV2::addRasterLayer);
    headerRow->addWidget(addRasterBtn);
    layout->addLayout(headerRow);

    list_ = new QListWidget();
    list_->setStyleSheet(
        QString("QListWidget{background:%1;color:%2;border:1px solid %3;border-radius:4px;outline:none;}")
            .arg(kInputBg, kTextColor, kBorderColor) +
        QString("QListWidget::item{background:transparent;padding:0px;border:none;margin:1px 0;}")
        + QString("QListWidget::item:selected{background:%1;border-radius:2px;}")
            .arg("#3A3D45") +
        QString("QListWidget::item:hover{background:%1;border-radius:2px;}")
            .arg("#2D3139"));
    list_->setSpacing(0);

    QObject::connect(list_, &QListWidget::currentRowChanged, this, &LayerPanelV2::onRowSelected);
    layout->addWidget(list_, 1);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(4);

    auto* dupBtn = makeIconButton(":/icons/svg/duplicate.svg",
        "Duplicate Layer\nCreate an identical copy of the selected layer.");
    QObject::connect(dupBtn, &QPushButton::clicked, this, &LayerPanelV2::duplicateLayer);
    btnRow->addWidget(dupBtn);

    auto* upBtn = makeIconButton(":/icons/svg/move_up.svg",
        "Move Layer Up\nMove the selected layer one position higher in the stack.");
    QObject::connect(upBtn, &QPushButton::clicked, this, &LayerPanelV2::moveLayerUp);
    btnRow->addWidget(upBtn);

    auto* downBtn = makeIconButton(":/icons/svg/move_down.svg",
        "Move Layer Down\nMove the selected layer one position lower in the stack.");
    QObject::connect(downBtn, &QPushButton::clicked, this, &LayerPanelV2::moveLayerDown);
    btnRow->addWidget(downBtn);

    auto* delBtn = makeIconButton(":/icons/svg/delete.svg",
        "Delete Layer\nRemove the selected layer permanently. Cannot delete the last remaining layer.");
    delBtn->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;border-radius:3px;font-size:13px;}")
            .arg(kBtnBg, kTextColor, kBorderColor) +
        QString("QPushButton:hover{background:%1;border-color:#E05050;color:#E05050;}")
            .arg(kDangerHover));
    QObject::connect(delBtn, &QPushButton::clicked, this, &LayerPanelV2::deleteLayer);
    btnRow->addWidget(delBtn);

    auto* copyFramesBtn = makeIconButton(":/icons/svg/duplicate.svg",
        "Copy to All Frames\nCopy this layer's content and name to the same layer\n"
        "across every frame in the sequence.");
    QObject::connect(copyFramesBtn, &QPushButton::clicked, this, &LayerPanelV2::copyLayerToAllFrames);
    btnRow->addWidget(copyFramesBtn);

    layout->addLayout(btnRow);

    auto* blendRow = new QHBoxLayout();
    blendRow->setSpacing(6);
    blendRow->setContentsMargins(0, 4, 0, 0);

    auto* blendLabel = new QLabel("Blend");
    blendLabel->setStyleSheet(QString("color:%1;font-size:10px;").arg(kMutedColor));
    blendRow->addWidget(blendLabel);

    blendCombo_ = new QComboBox();
    blendCombo_->addItems({"Normal", "Multiply", "Screen", "Overlay", "Add",
                           "Subtract", "Darken", "Lighten", "ColorBurn",
                           "ColorDodge", "SoftLight", "HardLight"});
    blendCombo_->setToolTip(
        "Blend Mode\nHow this layer's pixels combine with layers below it. "
        "Normal = no blending. Multiply = darken composite. Screen = lighten composite.");
    blendCombo_->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;border-radius:3px;padding:3px 6px;font-size:11px;}")
            .arg(kInputBg, kTextColor, kBorderColor) +
        QString("QComboBox:hover{border-color:%1;}")
            .arg(kAccentColor) +
        QString("QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%3;selection-color:#fff;border:1px solid %4;}")
            .arg(kInputBg, kTextColor, kAccentColor, kBorderColor) +
        QString("QComboBox QAbstractItemView::item:hover{background:%1;color:#fff;}")
            .arg(kAccentColor));
    QObject::connect(blendCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this, &LayerPanelV2::onBlendModeChanged);
    blendRow->addWidget(blendCombo_, 1);
    layout->addLayout(blendRow);

    refreshLayerList();
}

QWidget* LayerPanelV2::createLayerRow(int index, const Layer* layer) {
    auto* row = new QWidget();
    row->setAutoFillBackground(false);
    row->setStyleSheet("background:transparent;");
    row->setProperty("layerIndex", index);
    row->installEventFilter(this);

    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(6, 2, 6, 2);
    rowLayout->setSpacing(6);

    bool visible = layer->visible();
    bool locked = layer->locked();

    const char* rowColor = locked ? "#FF0000" : kTextColor;

    auto* visBtn = new QPushButton();
    visBtn->setFixedSize(22, 22);
    visBtn->setCheckable(true);
    visBtn->setChecked(visible);
    visBtn->setIcon(QIcon(visible ? ":/icons/svg/eye_visible.svg" : ":/icons/svg/eye_hidden.svg"));
    visBtn->setIconSize(QSize(18, 18));
    visBtn->setToolTip(visible ? "Hide this layer" : "Show this layer");
    visBtn->setCursor(Qt::PointingHandCursor);
    visBtn->setStyleSheet(
        QString(kInlineBtnBase)
            .arg(visible ? rowColor : kMutedColor, "#2D3139"));
    QObject::connect(visBtn, &QPushButton::clicked, [this, index]() {
        list_->setCurrentRow(index);
        toggleLayerVisibility(index);
    });
    rowLayout->addWidget(visBtn);

    QString typeTag;
    switch (layer->type()) {
    case LayerType::Raster: typeTag = "[R]"; break;
    case LayerType::Vector: typeTag = "[V]"; break;
    case LayerType::Group:  typeTag = "[G]"; break;
    default:                typeTag = "[-]"; break;
    }

    QString layerText = QString("%1 %2").arg(typeTag, QString::fromStdString(layer->name()));

    auto* nameLabel = new QLabel(layerText);
    nameLabel->setStyleSheet(
        QString("QLabel{background:transparent;color:%1;font-size:11px;}").arg(rowColor));
    rowLayout->addWidget(nameLabel, 1);

    auto* lockBtn = new LayerLockButton(locked);
    QObject::connect(lockBtn, &LayerLockButton::toggled, [this, index](bool) {
        list_->setCurrentRow(index);
        toggleLayerLock(index);
    });
    rowLayout->addWidget(lockBtn);

    return row;
}

bool LayerPanelV2::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto* w = qobject_cast<QWidget*>(obj);
        if (w && w != list_) {
            auto* me = static_cast<QMouseEvent*>(event);
            QWidget* child = w->childAt(me->pos());
            if (!qobject_cast<QPushButton*>(child)) {
                QVariant v = w->property("layerIndex");
                if (v.isValid()) {
                    list_->setCurrentRow(v.toInt());
                    return true;
                }
            }
        }
    } else if (event->type() == QEvent::MouseButtonDblClick) {
        auto* w = qobject_cast<QWidget*>(obj);
        if (w && w != list_) {
            auto* me = static_cast<QMouseEvent*>(event);
            QWidget* child = w->childAt(me->pos());
            if (!qobject_cast<QPushButton*>(child)) {
                QVariant v = w->property("layerIndex");
                if (v.isValid()) {
                    startRename(v.toInt(), w);
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void LayerPanelV2::setCurrentFrame(int frame) {
    currentFrame_ = frame;
    refreshLayerList();
}

LayerUid LayerPanelV2::currentLayerUid() const {
    auto& doc = appState_->document();
    const GroupLayer* root = doc.peekRootLayerForFrame(currentFrame_);
    if (!root) return 0;
    const Layer* layer = root->layerAt(currentLayerIndex_);
    return layer ? layer->uid() : 0;
}

void LayerPanelV2::addRasterLayer() {
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    int n = static_cast<int>(root.layerCount()) + 1;
    auto newLayer = std::make_unique<RasterLayer>(
        "Layer " + std::to_string(n),
        doc.width(), doc.height());
    LayerUid uid = newLayer->uid();
    root.addLayer(std::move(newLayer));
    doc.setModified(true);
    refreshLayerList();
    int idx = static_cast<int>(root.layerCount()) - 1;
    list_->setCurrentRow(idx);
    appState_->setActiveLayerIndex(idx);
    emit layerDisplayPropertiesChanged();
    emit layerChanged(idx);
    FAP_TRACE_LAYER("add", idx, uid, "Layer " + std::to_string(n));
}

void LayerPanelV2::duplicateLayer() {
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    int row = list_->currentRow();
    if (row < 0 || row >= static_cast<int>(root.layerCount())) return;
    Layer* dup = root.duplicateLayer(row);
    doc.setModified(true);
    refreshLayerList();
    list_->setCurrentRow(row + 1);
    emit layerDisplayPropertiesChanged();
    emit layerChanged(row + 1);
    FAP_TRACE_LAYER("duplicate", row + 1, dup ? dup->uid() : 0,
                     dup ? dup->name() : "");
}

void LayerPanelV2::moveLayerUp() {
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    int row = list_->currentRow();
    if (row > 0) {
        root.moveLayer(row, row - 1);
        doc.setModified(true);
        refreshLayerList();
        list_->setCurrentRow(row - 1);
        emit layerDisplayPropertiesChanged();
        emit layerChanged(row - 1);
        FAP_TRACE_LAYER("move_up", row - 1,
                         root.layerAt(row - 1) ? root.layerAt(row - 1)->uid() : 0, "");
    }
}

void LayerPanelV2::moveLayerDown() {
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    int row = list_->currentRow();
    int count = static_cast<int>(root.layerCount());
    if (row >= 0 && row < count - 1) {
        root.moveLayer(row, row + 1);
        doc.setModified(true);
        refreshLayerList();
        list_->setCurrentRow(row + 1);
        emit layerDisplayPropertiesChanged();
        emit layerChanged(row + 1);
        FAP_TRACE_LAYER("move_down", row + 1,
                         root.layerAt(row + 1) ? root.layerAt(row + 1)->uid() : 0, "");
    }
}

void LayerPanelV2::deleteLayer() {
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    if (root.layerCount() <= 1) return;
    int row = list_->currentRow();
    if (row < 0 || row >= static_cast<int>(root.layerCount())) return;
    Layer* l = root.layerAt(row);
    LayerUid uid = l ? l->uid() : 0;
    std::string name = l ? l->name() : "";
    root.removeLayer(row);
    doc.setModified(true);
    refreshLayerList();
    int newRow = std::min(row, static_cast<int>(root.layerCount()) - 1);
    if (newRow >= 0) {
        list_->setCurrentRow(newRow);
        emit layerDisplayPropertiesChanged();
        emit layerChanged(newRow);
    }
    FAP_TRACE_LAYER("delete", row, uid, name);
}

void LayerPanelV2::toggleLayerVisibility(int index) {
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    if (index < 0 || index >= static_cast<int>(root.layerCount())) return;
    Layer* layer = root.layerAt(index);
    if (layer) {
        bool vis = !layer->visible();
        layer->setVisible(vis);
        doc.setModified(true);
        refreshLayerList();
        emit layerDisplayPropertiesChanged();
        FAP_TRACE_LAYER(vis ? "visible_on" : "visible_off", index, layer->uid(), "");
    }
}

void LayerPanelV2::toggleLayerLock(int index) {
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    if (index < 0 || index >= static_cast<int>(root.layerCount())) return;
    Layer* layer = root.layerAt(index);
    if (layer) {
        bool lk = !layer->locked();
        layer->setLocked(lk);
        doc.setModified(true);
        QTimer::singleShot(0, this, [this]() { refreshLayerList(); });
        emit layerChanged(index);
        FAP_TRACE_LAYER(lk ? "lock_on" : "lock_off", index, layer->uid(), "");
    }
}

void LayerPanelV2::onBlendModeChanged(int mode) {
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    int row = list_->currentRow();
    if (row < 0 || row >= static_cast<int>(root.layerCount())) return;
    Layer* layer = root.layerAt(row);
    if (layer && mode >= 0 && mode <= static_cast<int>(BlendMode::HardLight)) {
        layer->setBlendMode(static_cast<BlendMode>(mode));
        doc.setModified(true);
        FAP_TRACE_LAYER("blend_mode", row, layer->uid(), std::to_string(mode));
        emit layerDisplayPropertiesChanged();
        emit layerChanged(row);
    }
}

void LayerPanelV2::onRowSelected(int row) {
    if (row < 0) return;
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    if (row >= static_cast<int>(root.layerCount())) return;

    currentLayerIndex_ = row;
    appState_->setActiveLayerIndex(row);

    const Layer* layer = root.layerAt(row);
    if (layer) {
        blendCombo_->blockSignals(true);
        blendCombo_->setCurrentIndex(static_cast<int>(layer->blendMode()));
        blendCombo_->blockSignals(false);
    }

    emit layerChanged(row);
    emit layerUidChanged(currentLayerUid());
}

void LayerPanelV2::startRename(int index, QWidget* row) {
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    if (index < 0 || index >= static_cast<int>(root.layerCount())) return;
    Layer* layer = root.layerAt(index);
    if (!layer) return;

    qDebug() << "RENAME start: frame=" << currentFrame_
             << "index=" << index
             << "layer=" << layer->name().c_str()
             << "total layers=" << root.layerCount();

    auto* hlay = qobject_cast<QHBoxLayout*>(row->layout());
    if (!hlay) return;
    if (hlay->count() < 3) return;

    auto* nameLabel = qobject_cast<QLabel*>(hlay->itemAt(1)->widget());
    if (!nameLabel) return;

    auto* editor = new QLineEdit(row);
    editor->setText(QString::fromStdString(layer->name()));
    editor->selectAll();
    editor->setStyleSheet(QString(
        "QLineEdit{background:%1;color:%2;border:1px solid %3;border-radius:3px;padding:2px 4px;font-size:11px;}")
        .arg(kInputBg, kTextColor, kAccentColor));

    hlay->insertWidget(1, editor);
    nameLabel->hide();
    editor->setFocus();

    auto committed = std::make_shared<bool>(false);
    QPointer<QHBoxLayout> pHlay(hlay);
    QPointer<QLineEdit> pEditor(editor);
    QPointer<QLabel> pNameLabel(nameLabel);
    auto commitRename = [pHlay, pEditor, pNameLabel, layer, committed, this]() mutable {
        if (*committed) return;
        *committed = true;

        if (!pHlay || !pEditor || !pNameLabel) return;

        QString text = pEditor->text().trimmed();
        qDebug() << "RENAME commit: text=" << text << "layer was=" << layer->name().c_str();
        if (!text.isEmpty()) {
            layer->setName(text.toStdString());
            qDebug() << "RENAME done: layer now=" << layer->name().c_str();
        }
        pHlay->removeWidget(pEditor);
        pEditor->deleteLater();
        pNameLabel->show();
        appState_->document().setModified(true);
        refreshLayerList();
        qDebug() << "RENAME refresh done";
        emit layerChanged(currentLayerIndex_);
    };

    QObject::connect(editor, &QLineEdit::returnPressed, this, commitRename);
    QObject::connect(editor, &QLineEdit::editingFinished, this, commitRename);
}

void LayerPanelV2::refreshLayerList() {
    auto& doc = appState_->document();
    const GroupLayer* root = doc.peekRootLayerForFrame(currentFrame_);
    if (!root) {
        qDebug() << "REFRESH: no root for frame" << currentFrame_;
        list_->blockSignals(true);
        list_->clear();
        list_->blockSignals(false);
        return;
    }

    qDebug() << "REFRESH: frame=" << currentFrame_ << "layerCount=" << root->layerCount();
    for (size_t i = 0; i < root->layerCount(); ++i) {
        const Layer* l = root->layerAt(static_cast<int>(i));
        if (l) qDebug() << "  layer[" << i << "] name=" << l->name().c_str();
    }

    int prevRow = list_->currentRow();
    list_->blockSignals(true);

    // Remove old item widgets before clearing (QListWidget::clear() does
    // not delete widgets set via setItemWidget(), leaving them as invisible
    // orphans that overlap with new widgets on the next paint).
    // Use deleteLater() to avoid crashing in-progress renames that hold
    // raw pointers to these widgets (see startRename lambda captures).
    for (int i = 0; i < list_->count(); ++i) {
        auto* item = list_->item(i);
        if (auto* w = list_->itemWidget(item)) {
            list_->removeItemWidget(item);
            w->hide();
            w->deleteLater();
        }
    }
    list_->clear();

    int count = static_cast<int>(root->layerCount());
    for (int i = 0; i < count; ++i) {
        const Layer* layer = root->layerAt(i);
        if (!layer) continue;

        auto* item = new QListWidgetItem();
        item->setSizeHint(QSize(0, 32));
        item->setData(Qt::UserRole, i);

        if (!layer->visible()) {
            item->setForeground(QColor(kMutedColor));
        } else if (layer->locked()) {
            item->setForeground(QColor("#E05050"));
        } else {
            item->setForeground(QColor(kTextColor));
        }

        list_->addItem(item);
        QWidget* rowWidget = createLayerRow(i, layer);
        list_->setItemWidget(item, rowWidget);
    }

    list_->blockSignals(false);

    if (prevRow >= 0 && prevRow < list_->count()) {
        list_->setCurrentRow(prevRow);
    } else if (list_->count() > 0) {
        list_->setCurrentRow(0);
    }
}

void LayerPanelV2::copyLayerToAllFrames() {
    auto& doc = appState_->document();
    auto& seq = doc.activeSequence();
    int srcFrame = seq.currentFrame();
    int layerIdx = currentLayerIndex_;

    GroupLayer& srcRoot = seq.rootLayerForFrame(srcFrame);
    if (layerIdx < 0 || layerIdx >= static_cast<int>(srcRoot.layerCount()))
        return;

    const Layer* srcLayer = srcRoot.layerAt(layerIdx);
    if (!srcLayer) return;

    // Build backups for undo
    std::vector<std::pair<int, std::unique_ptr<Layer>>> backups;
    for (int f = 0; f < seq.totalFrames(); ++f) {
        if (f == srcFrame) continue;
        auto* root = seq.peekRootLayerForFrame(f);
        if (root && static_cast<int>(root->layerCount()) > layerIdx) {
            backups.emplace_back(f, root->layerAt(layerIdx)->clone());
        } else {
            backups.emplace_back(f, nullptr);
        }
    }

    // Apply copy
    seq.copyLayerToAllFrames(srcFrame, layerIdx);

    doc.setModified(true);
    refreshLayerList();
    emit layerDisplayPropertiesChanged();

    // Push undo command
    auto cmd = std::make_unique<CopyLayerToFramesCommand>(
        appState_, srcFrame, layerIdx, std::move(backups));
    seq.undoManager().pushApplied(std::move(cmd));

    emit layerChanged(layerIdx);
}

} // namespace fap
