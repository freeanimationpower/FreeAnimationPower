#include "property_editor_v2.hpp"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QFrame>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFontComboBox>
#include <QtWidgets/QPlainTextEdit>

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
    shapeCombo_->addItems({ "Round", "Square", "Flat", "Calligraphy", "Pencil", "Highlighter" });
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

    auto* lineStyleRow = new QHBoxLayout();
    lineStyleRow->setSpacing(6);
    lineStyleLabel_ = new QLabel("LINE STYLE", this);
    lineStyleLabel_->setStyleSheet(kLabelStyle);
    lineStyleLabel_->setFixedWidth(52);
    lineStyleRow->addWidget(lineStyleLabel_);

    lineStyleCombo_ = new QComboBox(this);
    lineStyleCombo_->addItems({ "Solid", "Tapered", "Dashed", "Dotted" });
    lineStyleCombo_->setCurrentIndex(0);
    lineStyleCombo_->setStyleSheet(shapeCombo_->styleSheet());
    lineStyleRow->addWidget(lineStyleCombo_, 1);
    brushLayout->addLayout(lineStyleRow);

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

    // === Pick Color group ===
    pickColorGroup_ = new QWidget(this);
    auto* pickerLayout = new QVBoxLayout(pickColorGroup_);
    pickerLayout->setContentsMargins(0, 0, 0, 0);
    pickerLayout->setSpacing(6);

    pickColorLabel_ = new QLabel("COLOR PICKER", pickColorGroup_);
    pickColorLabel_->setStyleSheet(kLabelStyle);
    pickerLayout->addWidget(pickColorLabel_);

    auto* swatchRow = new QHBoxLayout();
    swatchRow->setSpacing(8);
    colorSwatch_ = new QPushButton(pickColorGroup_);
    colorSwatch_->setFixedSize(48, 48);
    colorSwatch_->setCursor(Qt::PointingHandCursor);
    colorSwatch_->setToolTip("Sampled color. Click to apply as primary.");
    swatchRow->addWidget(colorSwatch_);

    colorHexLabel_ = new QLabel("#000000", pickColorGroup_);
    colorHexLabel_->setStyleSheet(kValueLabelStyle);
    swatchRow->addWidget(colorHexLabel_, 1);
    pickerLayout->addLayout(swatchRow);

    auto* sepPicker1 = new QFrame(pickColorGroup_);
    sepPicker1->setFrameShape(QFrame::HLine);
    sepPicker1->setStyleSheet(kSeparatorStyle);
    pickerLayout->addWidget(sepPicker1);

    auto* varTypeRow = new QHBoxLayout();
    varTypeRow->setSpacing(6);
    auto* varTypeLabel = new QLabel("VARIATION", pickColorGroup_);
    varTypeLabel->setStyleSheet(kLabelStyle);
    varTypeLabel->setFixedWidth(52);
    varTypeRow->addWidget(varTypeLabel);

    variationTypeCombo_ = new QComboBox(pickColorGroup_);
    variationTypeCombo_->addItems({ "Monochromatic", "Analogous", "Triadic" });
    variationTypeCombo_->setCurrentIndex(0);
    variationTypeCombo_->setStyleSheet(shapeCombo_->styleSheet());
    varTypeRow->addWidget(variationTypeCombo_, 1);
    pickerLayout->addLayout(varTypeRow);

    variationGrid_ = new QWidget(pickColorGroup_);
    auto* vgLayout = new QGridLayout(variationGrid_);
    vgLayout->setContentsMargins(0, 4, 0, 0);
    vgLayout->setSpacing(3);
    for (int i = 0; i < 9; ++i) {
        auto* btn = new QPushButton(variationGrid_);
        btn->setFixedSize(28, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip("Click to select this color variation");
        int row = i / 3;
        int col = i % 3;
        vgLayout->addWidget(btn, row, col);
        variationButtons_.push_back(btn);

        connect(btn, &QPushButton::clicked, [this, i]() {
            onVariationClicked(i);
        });
    }
    pickerLayout->addWidget(variationGrid_);

    mainLayout->addWidget(pickColorGroup_);

    connect(variationTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertyEditorV2::onColorVariationTypeChanged);

    // === Fill group ===
    fillGroup_ = new QWidget(this);
    auto* fillLayout = new QVBoxLayout(fillGroup_);
    fillLayout->setContentsMargins(0, 0, 0, 0);
    fillLayout->setSpacing(6);

    auto* fillTypeRow = new QHBoxLayout();
    fillTypeRow->setSpacing(6);
    fillTypeLabel_ = new QLabel("FILL TYPE", fillGroup_);
    fillTypeLabel_->setStyleSheet(kLabelStyle);
    fillTypeLabel_->setFixedWidth(52);
    fillTypeRow->addWidget(fillTypeLabel_);

    fillTypeCombo_ = new QComboBox(fillGroup_);
    fillTypeCombo_->addItems({ "Solid", "Fabric", "Ramp" });
    fillTypeCombo_->setCurrentIndex(0);
    fillTypeCombo_->setStyleSheet(shapeCombo_->styleSheet());
    fillTypeRow->addWidget(fillTypeCombo_, 1);
    fillLayout->addLayout(fillTypeRow);

    mainLayout->addWidget(fillGroup_);

    connect(fillTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertyEditorV2::onFillTypeComboChanged);

    // === Text group ===
    textGroup_ = new QWidget(this);
    auto* textLayout = new QVBoxLayout(textGroup_);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(6);

    auto* textTitleLabel = new QLabel("TEXT", textGroup_);
    textTitleLabel->setStyleSheet(kLabelStyle);

    auto* fontRow = new QHBoxLayout();
    fontRow->setSpacing(6);
    auto* fontLabel = new QLabel("Font", textGroup_);
    fontLabel->setStyleSheet(kLabelStyle);
    fontLabel->setFixedWidth(32);
    fontRow->addWidget(fontLabel);

    fontCombo_ = new QFontComboBox(textGroup_);
    fontCombo_->setStyleSheet(shapeCombo_->styleSheet());
    fontCombo_->setCurrentFont(QFont("Arial", 24));
    connect(fontCombo_, &QFontComboBox::currentFontChanged, [this](const QFont& f) {
        QFont merged = f;
        merged.setPointSize(fontSizeSpin_->value());
        merged.setBold(fontBoldCb_->isChecked());
        merged.setItalic(fontItalicCb_->isChecked());
        appState_->toolState().setTextFont(merged);
    });
    fontRow->addWidget(fontCombo_, 1);
    textLayout->addLayout(fontRow);

    auto* sizeStyleRow = new QHBoxLayout();
    sizeStyleRow->setSpacing(8);
    auto* sizeLabel = new QLabel("Size", textGroup_);
    sizeLabel->setStyleSheet(kLabelStyle);
    sizeLabel->setFixedWidth(28);
    sizeStyleRow->addWidget(sizeLabel);

    fontSizeSpin_ = new QSpinBox(textGroup_);
    fontSizeSpin_->setRange(8, 200);
    fontSizeSpin_->setValue(24);
    fontSizeSpin_->setSuffix(" px");
    fontSizeSpin_->setFixedWidth(62);
    fontSizeSpin_->setStyleSheet(kSpinStyle);
    connect(fontSizeSpin_, QOverload<int>::of(&QSpinBox::valueChanged), [this](int size) {
        QFont f = fontCombo_->currentFont();
        f.setPointSize(size);
        f.setBold(fontBoldCb_->isChecked());
        f.setItalic(fontItalicCb_->isChecked());
        appState_->toolState().setTextFont(f);
    });
    sizeStyleRow->addWidget(fontSizeSpin_);

    fontBoldCb_ = new QCheckBox("B", textGroup_);
    fontBoldCb_->setToolTip("Bold");
    fontBoldCb_->setStyleSheet(
        "QCheckBox{color:#8B8FA3;font-size:10px;spacing:4px;}"
        "QCheckBox::indicator{width:14px;height:14px;border:1px solid #3D4150;border-radius:3px;background:#1E2130;}"
        "QCheckBox::indicator:checked{background:#FF6B4A;border-color:#FF6B4A;}");
    connect(fontBoldCb_, &QCheckBox::toggled, [this](bool bold) {
        QFont f = fontCombo_->currentFont();
        f.setPointSize(fontSizeSpin_->value());
        f.setBold(bold);
        f.setItalic(fontItalicCb_->isChecked());
        appState_->toolState().setTextFont(f);
    });
    sizeStyleRow->addWidget(fontBoldCb_);

    fontItalicCb_ = new QCheckBox("I", textGroup_);
    fontItalicCb_->setToolTip("Italic");
    fontItalicCb_->setStyleSheet(
        "QCheckBox{color:#8B8FA3;font-size:10px;spacing:4px;}"
        "QCheckBox::indicator{width:14px;height:14px;border:1px solid #3D4150;border-radius:3px;background:#1E2130;}"
        "QCheckBox::indicator:checked{background:#FF6B4A;border-color:#FF6B4A;}");
    connect(fontItalicCb_, &QCheckBox::toggled, [this](bool italic) {
        QFont f = fontCombo_->currentFont();
        f.setPointSize(fontSizeSpin_->value());
        f.setBold(fontBoldCb_->isChecked());
        f.setItalic(italic);
        appState_->toolState().setTextFont(f);
    });
    sizeStyleRow->addWidget(fontItalicCb_);
    sizeStyleRow->addStretch();
    textLayout->addLayout(sizeStyleRow);

    textEdit_ = new QPlainTextEdit(textGroup_);
    textEdit_->setPlaceholderText("Type your text here...");
    textEdit_->setFixedHeight(70);
    textEdit_->setStyleSheet(QString(
        "QPlainTextEdit{background:%1;color:%2;border:1px solid #2D3139;border-radius:4px;padding:4px;font-size:10px;}"
        "QPlainTextEdit:focus{border-color:#FF6B4A;}")
        .arg("#1E2130", "#C8CCD8"));
    connect(textEdit_, &QPlainTextEdit::textChanged, [this]() {
        appState_->toolState().setTextString(textEdit_->toPlainText());
    });
    textLayout->addWidget(textEdit_);

    auto* textHintLabel = new QLabel("Click canvas to place text", textGroup_);
    textHintLabel->setStyleSheet("QLabel{color:#3D4150;font-size:9px;}");
    textHintLabel->setAlignment(Qt::AlignCenter);
    textLayout->addWidget(textHintLabel);

    mainLayout->addWidget(textGroup_);

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

    pickColorGroup_->hide();
    fillGroup_->hide();

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
    connect(lineStyleCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertyEditorV2::onLineStyleChanged);
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

    bool showBrush = false;
    bool shapesOnly = false;
    QString placeholderText;

    switch (tool) {
    case ToolType::Brush:
    case ToolType::Eraser:
        showBrush = true;
        break;
    case ToolType::Line:
    case ToolType::Rectangle:
    case ToolType::Ellipse:
        showBrush = true;
        shapesOnly = true;
        break;
    case ToolType::Fill:
        brushGroup_->hide();
        pickColorGroup_->hide();
        placeholderWidget_->hide();
        fillGroup_->show();
        syncFromToolState();
        break;
    case ToolType::Text:
        brushGroup_->hide();
        pickColorGroup_->hide();
        fillGroup_->hide();
        placeholderWidget_->hide();
        textGroup_->show();
        syncTextFromState();
        break;
    case ToolType::Move:
        placeholderText = "Drag on canvas to move\nlayer or selection content";
        break;
    case ToolType::Select:
        placeholderText = "Drag to create selection\nClick inside to move\nPress Enter to commit";
        break;
    case ToolType::ColorPicker:
        brushGroup_->hide();
        fillGroup_->hide();
        placeholderWidget_->hide();
        pickColorGroup_->show();
        updateColorVariations();
        break;
    case ToolType::Hand:
        placeholderText = "Drag to pan canvas\nScroll wheel to zoom\nPress F to fit";
        break;
    default:
        placeholderText = "No properties available";
        break;
    }

    bool needsPlaceholder = !placeholderText.isEmpty();
    if (showBrush) {
        showBrushControls();
        if (shapesOnly) {
            shapeCombo_->hide();
            shapeLabel_->hide();
            hardnessSlider_->hide();
            hardnessSpin_->hide();
            hardnessLabel_->hide();
            opacitySlider_->hide();
            opacitySpin_->hide();
            opacityLabel_->hide();
            stabilizerSlider_->hide();
            stabilizerSpin_->hide();
            stabilizerLabel_->hide();
            pressureSizeCb_->hide();
            pressureOpacityCb_->hide();
            sizeLabel_->setText("WIDTH");
            lineStyleLabel_->show();
            lineStyleCombo_->show();
            placeholderWidget_->hide();
        } else {
            showAllBrushControls();
            sizeLabel_->setText("SIZE");
        }
        syncFromToolState();
    } else if (needsPlaceholder) {
        brushGroup_->hide();
    pickColorGroup_->hide();
    fillGroup_->hide();
    textGroup_->hide();
        showPlaceholder();
        placeholderLabel_->setText(placeholderText);
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

    if (ts.lineStyle() >= 0 && ts.lineStyle() <= 3) {
        lineStyleCombo_->blockSignals(true);
        lineStyleCombo_->setCurrentIndex(ts.lineStyle());
        lineStyleCombo_->blockSignals(false);
    }

    updatingFromState_ = false;
}

void PropertyEditorV2::showBrushControls()
{
    brushGroup_->show();
    placeholderWidget_->hide();
    pickColorGroup_->hide();
    fillGroup_->hide();
    textGroup_->hide();
}

void PropertyEditorV2::showAllBrushControls()
{
    brushGroup_->show();
    placeholderWidget_->hide();
    pickColorGroup_->hide();
    fillGroup_->hide();
    textGroup_->hide();
    shapeCombo_->show();
    shapeLabel_->show();
    hardnessSlider_->show();
    hardnessSpin_->show();
    hardnessLabel_->show();
    opacitySlider_->show();
    opacitySpin_->show();
    opacityLabel_->show();
    stabilizerSlider_->show();
    stabilizerSpin_->show();
    stabilizerLabel_->show();
    pressureSizeCb_->show();
    pressureOpacityCb_->show();
    lineStyleLabel_->hide();
    lineStyleCombo_->hide();
}

void PropertyEditorV2::showPlaceholder()
{
    brushGroup_->hide();
    pickColorGroup_->hide();
    fillGroup_->hide();
    textGroup_->hide();
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

void PropertyEditorV2::showPickColorControls()
{
    brushGroup_->hide();
    fillGroup_->hide();
    textGroup_->hide();
    placeholderWidget_->hide();
    pickColorGroup_->show();
}

void PropertyEditorV2::showFillControls()
{
    brushGroup_->hide();
    pickColorGroup_->hide();
    placeholderWidget_->hide();
    textGroup_->hide();
    fillGroup_->show();
}

void PropertyEditorV2::showTextControls()
{
    brushGroup_->hide();
    pickColorGroup_->hide();
    fillGroup_->hide();
    placeholderWidget_->hide();
    textGroup_->show();
}

void PropertyEditorV2::syncTextFromState()
{
    if (!appState_) return;
    auto& ts = appState_->toolState();
    QFont f = ts.textFont();

    fontCombo_->blockSignals(true);
    fontCombo_->setCurrentFont(f);
    fontCombo_->blockSignals(false);

    fontSizeSpin_->blockSignals(true);
    fontSizeSpin_->setValue(f.pointSize());
    fontSizeSpin_->blockSignals(false);

    fontBoldCb_->blockSignals(true);
    fontBoldCb_->setChecked(f.bold());
    fontBoldCb_->blockSignals(false);

    fontItalicCb_->blockSignals(true);
    fontItalicCb_->setChecked(f.italic());
    fontItalicCb_->blockSignals(false);

    textEdit_->blockSignals(true);
    textEdit_->setPlainText(ts.textString());
    textEdit_->blockSignals(false);
}

void PropertyEditorV2::updateColorVariations()
{
    if (!appState_) return;
    auto& ts = appState_->toolState();
    QColor base = ts.sampledColor();
    if (!base.isValid()) base = ts.primaryColor();

    QString hex = QString("#%1%2%3")
        .arg(base.red(), 2, 16, QChar('0'))
        .arg(base.green(), 2, 16, QChar('0'))
        .arg(base.blue(), 2, 16, QChar('0'));
    colorHexLabel_->setText(hex.toUpper());

    QString swatchStyle = QString(
        "QPushButton { background-color:%1; border:2px solid #2D3139; border-radius:4px; }"
        "QPushButton:hover { border-color:#FF6B4A; }"
    ).arg(base.name());
    colorSwatch_->setStyleSheet(swatchStyle);

    int total = static_cast<int>(variationButtons_.size());
    for (int i = 0; i < total; ++i) {
        QColor var = generateVariation(base, i, total);
        QString vStyle = QString(
            "QPushButton { background-color:%1; border:1px solid #2D3139; border-radius:3px; }"
            "QPushButton:hover { border-color:#FF6B4A; }"
        ).arg(var.name());
        variationButtons_[static_cast<size_t>(i)]->setStyleSheet(vStyle);
    }

    int varTypeIdx = variationTypeCombo_->findText(
        ts.colorVariationType() == 0 ? "Monochromatic" :
        ts.colorVariationType() == 1 ? "Analogous" : "Triadic");
    if (varTypeIdx >= 0) {
        variationTypeCombo_->blockSignals(true);
        variationTypeCombo_->setCurrentIndex(varTypeIdx);
        variationTypeCombo_->blockSignals(false);
    }

    if (ts.fillType() >= 0 && ts.fillType() <= 2) {
        fillTypeCombo_->blockSignals(true);
        fillTypeCombo_->setCurrentIndex(ts.fillType());
        fillTypeCombo_->blockSignals(false);
    }
}

QColor PropertyEditorV2::generateVariation(const QColor& base, int index, int total) const
{
    float h = 0, s = 0, l = 0, a = 0;
    base.getHslF(&h, &s, &l, &a);

    int varType = 0;
    if (appState_) varType = appState_->toolState().colorVariationType();

    int cols = 3;
    int row = index / cols;
    int col = index % cols;

    switch (varType) {
    case 0: { // Monochromatic: vary lightness and saturation
        float newS = 0.2f + col * 0.4f;
        float newL = 0.1f + row * 0.4f;
        return QColor::fromHslF(h, std::clamp(newS, 0.0f, 1.0f),
                                std::clamp(newL, 0.0f, 1.0f), a);
    }
    case 1: { // Analogous: vary hue slightly
        float hueShift = (col - 1) * 0.05f;
        float newH = std::fmod(h + hueShift + 1.0f, 1.0f);
        float newL = 0.2f + row * 0.3f;
        return QColor::fromHslF(newH, s, std::clamp(newL, 0.0f, 1.0f), a);
    }
    case 2: { // Triadic: vary hue by 120 degrees
        float hueShift = col * 0.3333f;
        float newH = std::fmod(h + hueShift + 1.0f, 1.0f);
        float newL = 0.2f + row * 0.3f;
        return QColor::fromHslF(newH, s, std::clamp(newL, 0.0f, 1.0f), a);
    }
    default:
        return base;
    }
}

void PropertyEditorV2::onVariationClicked(int index)
{
    if (!appState_) return;
    QColor base = appState_->toolState().sampledColor();
    if (!base.isValid()) base = appState_->toolState().primaryColor();
    int total = static_cast<int>(variationButtons_.size());
    QColor selected = generateVariation(base, index, total);
    appState_->toolState().setPrimaryColor(selected);
    appState_->toolState().setSampledColor(selected);
    updateColorVariations();
    emit primaryColorChanged(selected);
}

void PropertyEditorV2::onFillTypeComboChanged(int index)
{
    if (updatingFromState_) return;
    if (appState_) {
        appState_->toolState().setFillType(index);
    }
    emit fillTypeChanged(index);
}

void PropertyEditorV2::onColorVariationTypeChanged(int index)
{
    if (updatingFromState_) return;
    if (appState_) {
        appState_->toolState().setColorVariationType(index);
    }
    updateColorVariations();
}

void PropertyEditorV2::onLineStyleChanged(int index)
{
    if (updatingFromState_) return;
    if (appState_) {
        appState_->toolState().setLineStyle(index);
    }
}

} // namespace fap
