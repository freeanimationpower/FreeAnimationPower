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
#include <QtWidgets/QFrame>
#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtGui/QMouseEvent>
#include <QtGui/QIcon>

#include "core/app_state.hpp"
#include "core/document.hpp"
#include "core/layer.hpp"
#include "core/types.hpp"

namespace fap {

static const char* kPanelBg = "#1A1D24";
static const char* kTextColor = "#C8CCD8";
static const char* kMutedColor = "#6B7088";
static const char* kAccentColor = "#FF6B4A";
static const char* kBorderColor = "#2D3139";
static const char* kInputBg = "#252830";
static const char* kBtnBg = "#252830";
static const char* kBtnHover = "#2D3139";
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
    "  color:#FF6B4A;"
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

    auto* titleLabel = new QLabel("LAYERS");
    titleLabel->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:bold;letter-spacing:1px;").arg(kMutedColor));
    headerRow->addWidget(titleLabel);
    headerRow->addStretch();

    auto* addRasterBtn = makeIconButton(":/icons/layers/new_raster.png",
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

    auto* dupBtn = makeIconButton(":/icons/layers/duplicate.png",
        "Duplicate Layer\nCreate an identical copy of the selected layer.");
    QObject::connect(dupBtn, &QPushButton::clicked, this, &LayerPanelV2::duplicateLayer);
    btnRow->addWidget(dupBtn);

    auto* upBtn = makeIconButton(":/icons/layers/move_up.png",
        "Move Layer Up\nMove the selected layer one position higher in the stack.");
    QObject::connect(upBtn, &QPushButton::clicked, this, &LayerPanelV2::moveLayerUp);
    btnRow->addWidget(upBtn);

    auto* downBtn = makeIconButton(":/icons/layers/move_down.png",
        "Move Layer Down\nMove the selected layer one position lower in the stack.");
    QObject::connect(downBtn, &QPushButton::clicked, this, &LayerPanelV2::moveLayerDown);
    btnRow->addWidget(downBtn);

    auto* delBtn = makeIconButton(":/icons/layers/delete.png",
        "Delete Layer\nRemove the selected layer permanently. Cannot delete the last remaining layer.");
    delBtn->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;border-radius:3px;font-size:13px;}")
            .arg(kBtnBg, kTextColor, kBorderColor) +
        QString("QPushButton:hover{background:%1;border-color:#E05050;color:#E05050;}")
            .arg(kDangerHover));
    QObject::connect(delBtn, &QPushButton::clicked, this, &LayerPanelV2::deleteLayer);
    btnRow->addWidget(delBtn);

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
            .arg(kMutedColor) +
        QString("QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%3;border:1px solid %4;}")
            .arg(kInputBg, kTextColor, kAccentColor, kBorderColor));
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
    visBtn->setIcon(QIcon(visible ? ":/icons/layers/visibility_on.png" : ":/icons/layers/visibility_off.png"));
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
    root.addLayer(std::make_unique<RasterLayer>(
        "Layer " + std::to_string(n),
        doc.width(), doc.height()));
    doc.setModified(true);
    refreshLayerList();
    int idx = static_cast<int>(root.layerCount()) - 1;
    list_->setCurrentRow(idx);
    appState_->setActiveLayerIndex(idx);
    emit layerDisplayPropertiesChanged();
    emit layerChanged(idx);
}

void LayerPanelV2::duplicateLayer() {
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    int row = list_->currentRow();
    if (row < 0 || row >= static_cast<int>(root.layerCount())) return;
    root.duplicateLayer(row);
    doc.setModified(true);
    refreshLayerList();
    list_->setCurrentRow(row + 1);
    emit layerDisplayPropertiesChanged();
    emit layerChanged(row + 1);
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
    }
}

void LayerPanelV2::deleteLayer() {
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    if (root.layerCount() <= 1) return;
    int row = list_->currentRow();
    if (row < 0 || row >= static_cast<int>(root.layerCount())) return;
    root.removeLayer(row);
    doc.setModified(true);
    refreshLayerList();
    int newRow = std::min(row, static_cast<int>(root.layerCount()) - 1);
    if (newRow >= 0) {
        list_->setCurrentRow(newRow);
        emit layerDisplayPropertiesChanged();
        emit layerChanged(newRow);
    }
}

void LayerPanelV2::toggleLayerVisibility(int index) {
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    if (index < 0 || index >= static_cast<int>(root.layerCount())) return;
    Layer* layer = root.layerAt(index);
    if (layer) {
        layer->setVisible(!layer->visible());
        doc.setModified(true);
        refreshLayerList();
        emit layerDisplayPropertiesChanged();
    }
}

void LayerPanelV2::toggleLayerLock(int index) {
    auto& doc = appState_->document();
    GroupLayer& root = doc.rootLayerForFrame(currentFrame_);
    if (index < 0 || index >= static_cast<int>(root.layerCount())) return;
    Layer* layer = root.layerAt(index);
    if (layer) {
        layer->setLocked(!layer->locked());
        doc.setModified(true);
        QTimer::singleShot(0, this, [this]() { refreshLayerList(); });
        emit layerChanged(index);
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

    auto commitRename = [hlay, editor, nameLabel, layer, this]() {
        QString text = editor->text().trimmed();
        if (!text.isEmpty()) {
            layer->setName(text.toStdString());
        }
        hlay->removeWidget(editor);
        editor->deleteLater();
        nameLabel->show();
        appState_->document().setModified(true);
        refreshLayerList();
        emit layerChanged(currentLayerIndex_);
    };

    QObject::connect(editor, &QLineEdit::returnPressed, this, commitRename);
    QObject::connect(editor, &QLineEdit::editingFinished, this, commitRename);
}

void LayerPanelV2::refreshLayerList() {
    auto& doc = appState_->document();
    const GroupLayer* root = doc.peekRootLayerForFrame(currentFrame_);
    if (!root) {
        list_->blockSignals(true);
        list_->clear();
        list_->blockSignals(false);
        return;
    }

    int prevRow = list_->currentRow();
    list_->blockSignals(true);
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

} // namespace fap
