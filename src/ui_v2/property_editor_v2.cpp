#include "property_editor_v2.hpp"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QFrame>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>

#include "core/app_state.hpp"
#include "core/document.hpp"
#include "core/tool_state.hpp"
#include "core/types.hpp"

namespace fap {

static const QString kSliderStyle = QStringLiteral(
    "QSlider::groove:horizontal {"
    "  background:#22252F; height:4px; border-radius:2px;"
    "}"
    "QSlider::handle:horizontal {"
    "  background:#FF6B4A; width:14px; height:14px; margin:-5px 0; border-radius:7px;"
    "}"
    "QSlider::sub-page:horizontal {"
    "  background:#FF6B4A; border-radius:2px;"
    "}"
);

static const QString kSpinStyle = QStringLiteral(
    "QSpinBox {"
    "  background:#1E2130; color:#C8CCD8; border:1px solid #2D3139;"
    "  padding:2px 6px; border-radius:5px; font-size:11px;"
    "  font-family:'JetBrains Mono',monospace;"
    "  min-width:48px;"
    "}"
    "QSpinBox:focus { border-color:#FF6B4A; }"
);

static const QString kLabelStyle = QStringLiteral(
    "QLabel { color:#6B7088; font-size:9px; font-weight:600;"
    " text-transform:uppercase; letter-spacing:0.5px; }"
);

static const QString kValueLabelStyle = QStringLiteral(
    "QLabel { color:#C8CCD8; font-size:10px;"
    " font-family:'JetBrains Mono',monospace; }"
);

static const QString kSeparatorStyle = QStringLiteral(
    "QFrame { background:#2D3139; max-height:1px; }"
);

PropertyEditorV2::PropertyEditorV2(std::shared_ptr<AppState> state, QWidget* parent)
    : QWidget(parent)
    , appState_(std::move(state))
{
    setupUI();

    if (appState_) {
        connect(&appState_->toolState(), &ToolState::activeToolChanged,
                this, &PropertyEditorV2::onToolChanged);
    }

    refreshFields();
}

void PropertyEditorV2::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    brushGroup_ = new QWidget(this);
    auto* brushLayout = new QVBoxLayout(brushGroup_);
    brushLayout->setContentsMargins(0, 0, 0, 0);
    brushLayout->setSpacing(6);

    auto* sizeRow = new QHBoxLayout();
    sizeRow->setSpacing(6);
    sizeLabel_ = new QLabel("SIZE", this);
    sizeLabel_->setStyleSheet(kLabelStyle);
    sizeLabel_->setFixedWidth(40);
    sizeRow->addWidget(sizeLabel_);

    sizeSlider_ = new QSlider(Qt::Horizontal, this);
    sizeSlider_->setRange(1, 200);
    sizeSlider_->setValue(20);
    sizeSlider_->setStyleSheet(kSliderStyle);
    sizeRow->addWidget(sizeSlider_, 1);

    sizeSpin_ = new QSpinBox(this);
    sizeSpin_->setRange(1, 200);
    sizeSpin_->setValue(20);
    sizeSpin_->setStyleSheet(kSpinStyle);
    sizeSpin_->setFixedWidth(52);
    sizeRow->addWidget(sizeSpin_);
    brushLayout->addLayout(sizeRow);

    auto* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet(kSeparatorStyle);
    brushLayout->addWidget(sep1);

    auto* opacityRow = new QHBoxLayout();
    opacityRow->setSpacing(6);
    opacityLabel_ = new QLabel("OPACITY", this);
    opacityLabel_->setStyleSheet(kLabelStyle);
    opacityLabel_->setFixedWidth(40);
    opacityRow->addWidget(opacityLabel_);

    opacitySlider_ = new QSlider(Qt::Horizontal, this);
    opacitySlider_->setRange(0, 100);
    opacitySlider_->setValue(100);
    opacitySlider_->setStyleSheet(kSliderStyle);
    opacityRow->addWidget(opacitySlider_, 1);

    opacitySpin_ = new QSpinBox(this);
    opacitySpin_->setRange(0, 100);
    opacitySpin_->setValue(100);
    opacitySpin_->setSuffix("%");
    opacitySpin_->setStyleSheet(kSpinStyle);
    opacitySpin_->setFixedWidth(52);
    opacityRow->addWidget(opacitySpin_);
    brushLayout->addLayout(opacityRow);

    auto* sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet(kSeparatorStyle);
    brushLayout->addWidget(sep2);

    auto* hardnessRow = new QHBoxLayout();
    hardnessRow->setSpacing(6);
    hardnessLabel_ = new QLabel("HARDNESS", this);
    hardnessLabel_->setStyleSheet(kLabelStyle);
    hardnessLabel_->setFixedWidth(52);
    hardnessRow->addWidget(hardnessLabel_);

    hardnessSlider_ = new QSlider(Qt::Horizontal, this);
    hardnessSlider_->setRange(0, 100);
    hardnessSlider_->setValue(80);
    hardnessSlider_->setStyleSheet(kSliderStyle);
    hardnessRow->addWidget(hardnessSlider_, 1);

    hardnessSpin_ = new QSpinBox(this);
    hardnessSpin_->setRange(0, 100);
    hardnessSpin_->setValue(80);
    hardnessSpin_->setStyleSheet(kSpinStyle);
    hardnessSpin_->setFixedWidth(52);
    hardnessRow->addWidget(hardnessSpin_);
    brushLayout->addLayout(hardnessRow);

    auto* sep3 = new QFrame(this);
    sep3->setFrameShape(QFrame::HLine);
    sep3->setStyleSheet(kSeparatorStyle);
    brushLayout->addWidget(sep3);

    auto* shapeRow = new QHBoxLayout();
    shapeRow->setSpacing(6);
    shapeLabel_ = new QLabel("SHAPE", this);
    shapeLabel_->setStyleSheet(kLabelStyle);
    shapeLabel_->setFixedWidth(40);
    shapeRow->addWidget(shapeLabel_);

    shapeCombo_ = new QComboBox(this);
    shapeCombo_->addItems({ "Round", "Square", "Flat", "Calligraphy" });
    shapeCombo_->setCurrentIndex(0);
    shapeCombo_->setStyleSheet(
        "QComboBox {"
        "  background:#1E2130; color:#C8CCD8; border:1px solid #2D3139;"
        "  padding:2px 6px; border-radius:5px; font-size:10px;"
        "}"
        "QComboBox:hover { border-color:#3D4150; }"
        "QComboBox::drop-down { border:none; width:16px; }"
        "QComboBox QAbstractItemView {"
        "  background:#1A1D24; color:#C8CCD8;"
        "  selection-background-color:#FF6B4A; border:1px solid #2D3139;"
        "}");
    shapeRow->addWidget(shapeCombo_, 1);
    brushLayout->addLayout(shapeRow);

    auto* sep4 = new QFrame(this);
    sep4->setFrameShape(QFrame::HLine);
    sep4->setStyleSheet(kSeparatorStyle);
    brushLayout->addWidget(sep4);

    auto* stabilizerRow = new QHBoxLayout();
    stabilizerRow->setSpacing(6);
    stabilizerLabel_ = new QLabel("STABILIZER", this);
    stabilizerLabel_->setStyleSheet(kLabelStyle);
    stabilizerLabel_->setFixedWidth(52);
    stabilizerRow->addWidget(stabilizerLabel_);

    stabilizerSlider_ = new QSlider(Qt::Horizontal, this);
    stabilizerSlider_->setRange(0, 50);
    stabilizerSlider_->setValue(0);
    stabilizerSlider_->setStyleSheet(kSliderStyle);
    stabilizerRow->addWidget(stabilizerSlider_, 1);

    stabilizerSpin_ = new QSpinBox(this);
    stabilizerSpin_->setRange(0, 50);
    stabilizerSpin_->setValue(0);
    stabilizerSpin_->setStyleSheet(kSpinStyle);
    stabilizerSpin_->setFixedWidth(52);
    stabilizerRow->addWidget(stabilizerSpin_);
    brushLayout->addLayout(stabilizerRow);

    pressureSizeCb_ = new QCheckBox("Pressure controls size", this);
    pressureSizeCb_->setChecked(false);
    pressureSizeCb_->setStyleSheet(
        "QCheckBox { color:#8B8FA3; font-size:10px; spacing:6px; }"
        "QCheckBox::indicator { width:14px; height:14px; border:1px solid #3D4150;"
        "  border-radius:3px; background:#1E2130; }"
        "QCheckBox::indicator:checked { background:#FF6B4A; border-color:#FF6B4A; }");
    brushLayout->addWidget(pressureSizeCb_);

    pressureOpacityCb_ = new QCheckBox("Pressure controls opacity", this);
    pressureOpacityCb_->setChecked(false);
    pressureOpacityCb_->setStyleSheet(
        "QCheckBox { color:#8B8FA3; font-size:10px; spacing:6px; }"
        "QCheckBox::indicator { width:14px; height:14px; border:1px solid #3D4150;"
        "  border-radius:3px; background:#1E2130; }"
        "QCheckBox::indicator:checked { background:#FF6B4A; border-color:#FF6B4A; }");
    brushLayout->addWidget(pressureOpacityCb_);

    mainLayout->addWidget(brushGroup_);

    placeholderWidget_ = new QWidget(this);
    auto* placeholderLayout = new QVBoxLayout(placeholderWidget_);
    placeholderLayout->setContentsMargins(0, 0, 0, 0);
    placeholderLabel_ = new QLabel(this);
    placeholderLabel_->setWordWrap(true);
    placeholderLabel_->setAlignment(Qt::AlignCenter);
    placeholderLabel_->setStyleSheet(
        "QLabel { color:#3D4150; font-size:10px; padding:12px 4px; }");
    placeholderLayout->addWidget(placeholderLabel_);
    mainLayout->addWidget(placeholderWidget_);

    mainLayout->addStretch();

    connect(sizeSlider_, &QSlider::valueChanged,
            this, &PropertyEditorV2::onSizeSliderChanged);
    connect(sizeSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PropertyEditorV2::onSizeSpinChanged);
    connect(opacitySlider_, &QSlider::valueChanged,
            this, &PropertyEditorV2::onOpacitySliderChanged);
    connect(opacitySpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PropertyEditorV2::onOpacitySpinChanged);
    connect(stabilizerSlider_, &QSlider::valueChanged,
            this, &PropertyEditorV2::onStabilizerSliderChanged);
    connect(stabilizerSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PropertyEditorV2::onStabilizerSpinChanged);
    connect(hardnessSlider_, &QSlider::valueChanged,
            this, &PropertyEditorV2::onHardnessSliderChanged);
    connect(hardnessSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PropertyEditorV2::onHardnessSpinChanged);
    connect(shapeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertyEditorV2::onShapeChanged);
    connect(pressureSizeCb_, &QCheckBox::toggled, [this](bool checked) {
        if (appState_) { appState_->toolState().setPressureSize(checked); }
    });
    connect(pressureOpacityCb_, &QCheckBox::toggled, [this](bool checked) {
        if (appState_) { appState_->toolState().setPressureOpacity(checked); }
    });
}

void PropertyEditorV2::onToolChanged()
{
    refreshFields();
}

void PropertyEditorV2::refreshFields()
{
    if (!appState_) return;

    auto tool = appState_->toolState().activeTool();

    if (tool == ToolType::Brush || tool == ToolType::Eraser) {
        showBrushControls();
        syncFromToolState();
    } else {
        showPlaceholder();
        const char* names[] = {
            "Brush", "Eraser", "Color Picker", "Fill",
            "Text", "Line", "Rectangle", "Ellipse",
            "Move", "Select", "Hand"
        };
        int idx = static_cast<int>(tool);
        QString toolName = (idx >= 0 && idx < 11) ? names[idx] : "Unknown";
        placeholderLabel_->setText(
            QString("No properties\nfor %1").arg(toolName));
    }
}

void PropertyEditorV2::syncFromToolState()
{
    if (!appState_) return;
    auto& ts = appState_->toolState();

    updatingFromState_ = true;

    sizeSlider_->setValue(ts.brushSize());
    sizeSpin_->setValue(ts.brushSize());

    opacitySlider_->setValue(ts.brushOpacity());
    opacitySpin_->setValue(ts.brushOpacity());

    hardnessSlider_->setValue(ts.brushHardness());
    hardnessSpin_->setValue(ts.brushHardness());

    int shapeIdx = shapeCombo_->findText(ts.brushShape());
    if (shapeIdx >= 0) {
        shapeCombo_->blockSignals(true);
        shapeCombo_->setCurrentIndex(shapeIdx);
        shapeCombo_->blockSignals(false);
    }

    stabilizerSlider_->setValue(ts.stabilizerLevel());
    stabilizerSpin_->setValue(ts.stabilizerLevel());

    pressureSizeCb_->blockSignals(true);
    pressureSizeCb_->setChecked(ts.pressureSize());
    pressureSizeCb_->blockSignals(false);

    pressureOpacityCb_->blockSignals(true);
    pressureOpacityCb_->setChecked(ts.pressureOpacity());
    pressureOpacityCb_->blockSignals(false);

    updatingFromState_ = false;
}

void PropertyEditorV2::showBrushControls()
{
    brushGroup_->show();
    placeholderWidget_->hide();
}

void PropertyEditorV2::showPlaceholder()
{
    brushGroup_->hide();
    placeholderWidget_->show();
}

void PropertyEditorV2::onSizeSliderChanged(int value)
{
    if (updatingFromState_) return;
    sizeSpin_->blockSignals(true);
    sizeSpin_->setValue(value);
    sizeSpin_->blockSignals(false);

    if (appState_) {
        appState_->toolState().setBrushSize(value);
        appState_->document().setModified(true);
    }
    emit brushSizeChanged(value);
}

void PropertyEditorV2::onOpacitySliderChanged(int value)
{
    if (updatingFromState_) return;
    opacitySpin_->blockSignals(true);
    opacitySpin_->setValue(value);
    opacitySpin_->blockSignals(false);

    if (appState_) {
        appState_->toolState().setBrushOpacity(value);
        appState_->document().setModified(true);
    }
    emit brushOpacityChanged(value);
}

void PropertyEditorV2::onSizeSpinChanged(int value)
{
    if (updatingFromState_) return;
    sizeSlider_->blockSignals(true);
    sizeSlider_->setValue(value);
    sizeSlider_->blockSignals(false);

    if (appState_) {
        appState_->toolState().setBrushSize(value);
        appState_->document().setModified(true);
    }
    emit brushSizeChanged(value);
}

void PropertyEditorV2::onOpacitySpinChanged(int value)
{
    if (updatingFromState_) return;
    opacitySlider_->blockSignals(true);
    opacitySlider_->setValue(value);
    opacitySlider_->blockSignals(false);

    if (appState_) {
        appState_->toolState().setBrushOpacity(value);
        appState_->document().setModified(true);
    }
    emit brushOpacityChanged(value);
}

void PropertyEditorV2::onHardnessSliderChanged(int value)
{
    if (updatingFromState_) return;
    hardnessSpin_->blockSignals(true);
    hardnessSpin_->setValue(value);
    hardnessSpin_->blockSignals(false);

    if (appState_) {
        appState_->toolState().setBrushHardness(value);
    }
    emit brushHardnessChanged(value);
}

void PropertyEditorV2::onHardnessSpinChanged(int value)
{
    if (updatingFromState_) return;
    hardnessSlider_->blockSignals(true);
    hardnessSlider_->setValue(value);
    hardnessSlider_->blockSignals(false);

    if (appState_) {
        appState_->toolState().setBrushHardness(value);
    }
    emit brushHardnessChanged(value);
}

void PropertyEditorV2::onShapeChanged(int)
{
    if (updatingFromState_) return;
    QString shape = shapeCombo_->currentText();
    if (appState_) {
        appState_->toolState().setBrushShape(shape);
    }
    emit brushShapeChanged(shape);
}

void PropertyEditorV2::onStabilizerSliderChanged(int value)
{
    if (updatingFromState_) return;
    stabilizerSpin_->blockSignals(true);
    stabilizerSpin_->setValue(value);
    stabilizerSpin_->blockSignals(false);

    if (appState_) {
        appState_->toolState().setStabilizerLevel(value);
    }
    emit stabilizerChanged(value);
}

void PropertyEditorV2::onStabilizerSpinChanged(int value)
{
    if (updatingFromState_) return;
    stabilizerSlider_->blockSignals(true);
    stabilizerSlider_->setValue(value);
    stabilizerSlider_->blockSignals(false);

    if (appState_) {
        appState_->toolState().setStabilizerLevel(value);
    }
    emit stabilizerChanged(value);
}

} // namespace fap
