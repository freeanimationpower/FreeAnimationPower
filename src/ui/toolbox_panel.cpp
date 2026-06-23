#include "ui/toolbox_panel.hpp"
#include "ui/icons.hpp"
#include "core/tool_state.hpp"
#include "core/types.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QPainter>
#include <QColorDialog>
#include <QScrollArea>
#include <QFrame>
#include <algorithm>

namespace fap {

// ============================================================================
// ColorSwatchButton
// ============================================================================

ColorSwatchButton::ColorSwatchButton(QWidget* parent)
    : QPushButton(parent) {
    setFixedSize(36, 36);
    setCursor(Qt::PointingHandCursor);
    setToolTip("Pick primary color");
}

void ColorSwatchButton::setSwatchColor(const QColor& color) {
    color_ = color;
    update();
}

QColor ColorSwatchButton::swatchColor() const { return color_; }

void ColorSwatchButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(40, 40, 40));
    p.drawRoundedRect(rect(), 4, 4);

    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.setBrush(color_);
    p.drawRoundedRect(rect().adjusted(3, 3, -3, -3), 3, 3);

    int cx = width() / 2, cy = height() / 2;
    p.setPen(QPen(color_.lightness() > 128 ? QColor(0, 0, 0, 60) : QColor(255, 255, 255, 60), 1));
    p.drawLine(cx - 2, cy, cx + 2, cy);
    p.drawLine(cx, cy - 2, cx, cy + 2);
}

// ============================================================================
// AccordionSection
// ============================================================================

AccordionSection::AccordionSection(const QString& title, QWidget* parent)
    : QWidget(parent) {
    auto* selfLayout = new QVBoxLayout(this);
    selfLayout->setContentsMargins(0, 0, 0, 0);
    selfLayout->setSpacing(0);

    header_btn_ = new QPushButton(this);
    header_btn_->setCursor(Qt::PointingHandCursor);
    header_btn_->setFlat(true);

    arrow_ = new QLabel("\u25B6", header_btn_);
    arrow_->setStyleSheet("color:#888;font-size:9px;background:transparent;");
    arrow_->setFixedWidth(14);

    auto* titleLbl = new QLabel(title, header_btn_);
    titleLbl->setStyleSheet("color:#75beff;font-weight:bold;font-size:11px;background:transparent;");

    auto* hlay = new QHBoxLayout(header_btn_);
    hlay->setContentsMargins(4, 0, 4, 0);
    hlay->addWidget(arrow_);
    hlay->addWidget(titleLbl);
    hlay->addStretch();

    header_btn_->setStyleSheet(
        "QPushButton{background:#2d2d30;text-align:left;padding:6px 4px;"
        "border:none;border-bottom:1px solid #3e3e42;}"
        "QPushButton:hover{background:#333337;}");

    selfLayout->addWidget(header_btn_);

    QObject::connect(header_btn_, &QPushButton::clicked, [this]() {
        setExpanded(!expanded_);
    });
}

void AccordionSection::setContentWidget(QWidget* content) {
    if (content_) {
        layout()->removeWidget(content_);
        content_->deleteLater();
    }
    content_ = content;
    if (content_) {
        content_->setVisible(expanded_);
        qobject_cast<QVBoxLayout*>(layout())->addWidget(content_);
    }
}

void AccordionSection::setExpanded(bool expanded) {
    expanded_ = expanded;
    arrow_->setText(expanded_ ? "\u25BC" : "\u25B6");
    if (content_) content_->setVisible(expanded_);
}

bool AccordionSection::isExpanded() const { return expanded_; }

// ============================================================================
// ToolboxPanel
// ============================================================================

ToolboxPanel::ToolboxPanel(ToolState* state, QWidget* parent)
    : QWidget(parent)
    , state_(state) {
    setFixedWidth(210);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet(
        "QScrollArea{background:#252526;border:none;}"
        "QScrollBar:vertical{background:#1e1e1e;width:8px;border:none;}"
        "QScrollBar::handle:vertical{background:#555;min-height:30px;border-radius:4px;}"
        "QScrollBar::handle:vertical:hover{background:#777;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}");

    auto* inner = new QWidget(scroll);
    auto* innerLayout = new QVBoxLayout(inner);
    innerLayout->setContentsMargins(6, 6, 6, 6);
    innerLayout->setSpacing(8);

    scroll->setWidget(inner);
    outer->addWidget(scroll);

    applyStyles();

    auto addHeader = [&](const QString& text) {
        auto* h = new QLabel(text, inner);
        h->setStyleSheet("color:#75beff;font-weight:bold;font-size:11px;padding:4px 0 2px 0;background:transparent;");
        innerLayout->addWidget(h);
    };

    addHeader("TOOLS");
    buildToolsSection(inner, innerLayout);
    innerLayout->addSpacing(6);
    addHeader("COLOR");
    buildColorSection(inner, innerLayout);
    innerLayout->addSpacing(8);
    addHeader("BRUSH SETTINGS");
    buildBrushSettingsSection(inner, innerLayout);
    innerLayout->addSpacing(8);
    addHeader("ONION SKIN");
    buildOnionSkinSection(inner, innerLayout);
    innerLayout->addSpacing(8);
    buildCanvasSizeSection(inner, innerLayout);
    innerLayout->addStretch();

    syncFromState();
}

void ToolboxPanel::applyStyles() {
    setStyleSheet(
        "QWidget{background:#252526;color:#cccccc;font-size:11px;}"
        "QSlider::groove:horizontal{background:#3c3c3c;height:6px;border-radius:3px;}"
        "QSlider::handle:horizontal{background:#d4782a;width:14px;height:14px;margin:-4px 0;border-radius:7px;}"
        "QSlider::handle:horizontal:hover{background:#e88c3a;}"
        "QSlider::sub-page:horizontal{background:#094771;height:6px;border-radius:3px;}"
        "QSpinBox{background:#333337;color:#e0e0e0;border:1px solid #555;border-radius:3px;"
        "  padding:2px 4px;min-width:48px;max-width:58px;font-size:10px;}"
        "QSpinBox:hover{border-color:#007acc;}"
        "QSpinBox::up-button,QSpinBox::down-button{subcontrol-origin:border;width:14px;background:#3c3c3c;}"
        "QComboBox{background:#333337;color:#e0e0e0;border:1px solid #555;border-radius:3px;"
        "  padding:3px 6px;font-size:10px;}"
        "QComboBox:hover{border-color:#007acc;}"
        "QComboBox::drop-down{width:20px;border-left:1px solid #555;}"
        "QComboBox QAbstractItemView{background:#2d2d30;color:#e0e0e0;selection-background-color:#094771;"
        "  border:1px solid #555;}"
        "QCheckBox{spacing:6px;font-size:10px;}"
        "QCheckBox::indicator{width:15px;height:15px;border:1px solid #555;border-radius:2px;background:#333337;}"
        "QCheckBox::indicator:checked{background:#007acc;border-color:#007acc;}"
    );
}

// ============================================================================
// TOOLS Section
// ============================================================================

struct ToolDef { QIcon icon; QString tip; };

void ToolboxPanel::buildToolsSection(QWidget* parent, QVBoxLayout* layout) {
    tool_group_ = new QButtonGroup(parent);
    tool_group_->setExclusive(true);

    ToolDef tools[] = {
        { icons::Brush(),     "Brush (B)"      },
        { icons::Eraser(),    "Eraser (E)"     },
        { icons::PickColor(), "Pick Color (I)" },
        { icons::Fill(),      "Fill (G)"       },
        { icons::Text(),      "Text (T)"       },
        { icons::Line(),      "Line (L)"       },
        { icons::Rectangle(), "Rectangle (U)"  },
        { icons::Ellipse(),   "Ellipse (Y)"    },
        { icons::Move(),      "Move (M)"       },
        { icons::Select(),    "Select (S)"     },
        { icons::Hand(),      "Hand (H)"       },
    };

    for (int row = 0; row < 4; ++row) {
        auto* rowLayout = new QHBoxLayout();
        rowLayout->setSpacing(4);
        int cols = (row == 3) ? 1 : 3;
        for (int col = 0; col < cols; ++col) {
            int idx = row * 3 + col;
            if (idx >= 11) break;
            auto* btn = new QPushButton(parent);
            btn->setIcon(tools[idx].icon);
            btn->setIconSize(QSize(20, 20));
            btn->setToolTip(tools[idx].tip);
            btn->setCheckable(true);
            btn->setFixedSize(32, 32);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(
                "QPushButton{background:#3c3c3c;border:1px solid #4a4a4d;border-radius:4px;}"
                "QPushButton:hover{background:#4a4a4a;border-color:#666;}"
                "QPushButton:checked{background:#094771;border:1px solid #007acc;}");
            tool_group_->addButton(btn, idx);
            tool_buttons_.push_back(btn);
            rowLayout->addWidget(btn);
        }
        if (row < 3) rowLayout->addStretch();
        layout->addLayout(rowLayout);
    }

    QObject::connect(tool_group_, &QButtonGroup::idClicked, [this](int id) {
        state_->setActiveToolByIndex(id);
    });
}

// ============================================================================
// COLOR Section
// ============================================================================

void ToolboxPanel::buildColorSection(QWidget* parent, QVBoxLayout* layout) {
    auto* colorRow = new QHBoxLayout();
    colorRow->setSpacing(8);

    color_swatch_ = new ColorSwatchButton(parent);
    QObject::connect(color_swatch_, &QPushButton::clicked, [this]() {
        QColor c = QColorDialog::getColor(state_->primaryColor(), this, "Pick Color");
        if (c.isValid()) {
            state_->setPrimaryColor(c);
        }
    });
    colorRow->addWidget(color_swatch_);

    auto* pickLabel = new QLabel("Pick Color", parent);
    pickLabel->setStyleSheet("color:#aaa;font-size:10px;background:transparent;");
    pickLabel->setCursor(Qt::PointingHandCursor);
    colorRow->addWidget(pickLabel);
    colorRow->addStretch();

    layout->addLayout(colorRow);
}

// ============================================================================
// BRUSH SETTINGS Section
// ============================================================================

static QWidget* makeSliderWithSpin(QWidget* parent, const QString& label,
                                    int min, int max, int val,
                                    QSlider** outSlider, QSpinBox** outSpin,
                                    std::function<void(int)> onChanged) {
    auto* row = new QWidget(parent);
    auto* hlay = new QHBoxLayout(row);
    hlay->setContentsMargins(0, 1, 0, 1);
    hlay->setSpacing(6);

    auto* lbl = new QLabel(label, row);
    lbl->setStyleSheet("color:#aaa;font-size:10px;background:transparent;");
    lbl->setFixedWidth(44);

    auto* slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(min, max);
    slider->setValue(val);
    slider->setFixedHeight(20);

    auto* spin = new QSpinBox(row);
    spin->setRange(min, max);
    spin->setValue(val);
    spin->setFixedWidth(52);
    spin->setAlignment(Qt::AlignCenter);

    bool syncing = false;

    QObject::connect(slider, &QSlider::valueChanged, [&syncing, spin, onChanged](int v) {
        if (syncing) return;
        syncing = true;
        spin->setValue(v);
        onChanged(v);
        syncing = false;
    });

    QObject::connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                     [&syncing, slider, onChanged](int v) {
        if (syncing) return;
        syncing = true;
        slider->setValue(v);
        onChanged(v);
        syncing = false;
    });

    hlay->addWidget(lbl);
    hlay->addWidget(slider);
    hlay->addWidget(spin);

    if (outSlider) *outSlider = slider;
    if (outSpin) *outSpin = spin;
    return row;
}

void ToolboxPanel::buildBrushSettingsSection(QWidget* parent, QVBoxLayout* layout) {
    layout->addWidget(makeSliderWithSpin(parent, "Size", 1, 100, state_->brushSize(),
        &size_slider_, &size_spin_, [this](int v) { state_->setBrushSize(v); }));

    layout->addWidget(makeSliderWithSpin(parent, "Opacity", 1, 100, state_->brushOpacity(),
        &opacity_slider_, &opacity_spin_, [this](int v) { state_->setBrushOpacity(v); }));

    layout->addWidget(makeSliderWithSpin(parent, "Hardness", 1, 100, state_->brushHardness(),
        &hardness_slider_, &hardness_spin_, [this](int v) { state_->setBrushHardness(v); }));

    auto* shapeRow = new QHBoxLayout();
    auto* shapeLabel = new QLabel("Shape", parent);
    shapeLabel->setStyleSheet("color:#aaa;font-size:10px;background:transparent;");
    shapeLabel->setFixedWidth(44);
    shapeRow->addWidget(shapeLabel);

    shape_combo_ = new QComboBox(parent);
    shape_combo_->addItems({"Round", "Square", "Flat", "Calligraphy"});
    shape_combo_->setCurrentText(state_->brushShape());
    QObject::connect(shape_combo_, &QComboBox::currentTextChanged, [this](const QString& s) {
        state_->setBrushShape(s);
    });
    shapeRow->addWidget(shape_combo_);
    shapeRow->addStretch();
    layout->addLayout(shapeRow);

    layout->addSpacing(4);

    auto* pressureLabel = new QLabel("Tablet Pressure", parent);
    pressureLabel->setStyleSheet("color:#75beff;font-weight:bold;font-size:10px;padding:4px 0 2px 0;background:transparent;");
    layout->addWidget(pressureLabel);

    pressure_size_cb_ = new QCheckBox("Pressure affects size", parent);
    pressure_size_cb_->setChecked(state_->pressureSize());
    QObject::connect(pressure_size_cb_, &QCheckBox::toggled, [this](bool v) {
        state_->setPressureSize(v);
    });
    layout->addWidget(pressure_size_cb_);

    pressure_opacity_cb_ = new QCheckBox("Pressure affects opacity", parent);
    pressure_opacity_cb_->setChecked(state_->pressureOpacity());
    QObject::connect(pressure_opacity_cb_, &QCheckBox::toggled, [this](bool v) {
        state_->setPressureOpacity(v);
    });
    layout->addWidget(pressure_opacity_cb_);

    layout->addSpacing(4);

    auto* stabRow = new QHBoxLayout();
    auto* stabLabel = new QLabel("Stabilizer", parent);
    stabLabel->setStyleSheet("color:#aaa;font-size:10px;background:transparent;");
    stabLabel->setFixedWidth(56);
    stabRow->addWidget(stabLabel);

    stabilizer_spin_ = new QSpinBox(parent);
    stabilizer_spin_->setRange(0, 50);
    stabilizer_spin_->setValue(state_->stabilizerLevel());
    stabilizer_spin_->setSuffix(" pts");
    stabilizer_spin_->setToolTip("Higher values smooth the stroke but add latency");
    QObject::connect(stabilizer_spin_, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v) {
        state_->setStabilizerLevel(v);
    });
    stabRow->addWidget(stabilizer_spin_);
    stabRow->addStretch();
    layout->addLayout(stabRow);
}

// ============================================================================
// ONION SKIN Section
// ============================================================================

void ToolboxPanel::buildOnionSkinSection(QWidget* parent, QVBoxLayout* layout) {
    onion_enabled_cb_ = new QCheckBox("Enable Onion Skin", parent);
    onion_enabled_cb_->setChecked(state_->onionEnabled());
    QObject::connect(onion_enabled_cb_, &QCheckBox::toggled, [this](bool v) {
        state_->setOnionEnabled(v);
    });
    layout->addWidget(onion_enabled_cb_);

    layout->addSpacing(4);

    auto* prevRow = new QHBoxLayout();
    auto* prevLabel = new QLabel("Prev frames", parent);
    prevLabel->setStyleSheet("color:#aaa;font-size:10px;background:transparent;");
    prevLabel->setFixedWidth(60);
    prevRow->addWidget(prevLabel);

    onion_prev_spin_ = new QSpinBox(parent);
    onion_prev_spin_->setRange(0, 10);
    onion_prev_spin_->setValue(state_->onionPrevFrames());
    onion_prev_spin_->setToolTip("Previous frames visible in red tint");
    QObject::connect(onion_prev_spin_, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v) {
        state_->setOnionPrevFrames(v);
    });
    prevRow->addWidget(onion_prev_spin_);
    prevRow->addStretch();
    layout->addLayout(prevRow);

    auto* nextRow = new QHBoxLayout();
    auto* nextLabel = new QLabel("Next frames", parent);
    nextLabel->setStyleSheet("color:#aaa;font-size:10px;background:transparent;");
    nextLabel->setFixedWidth(60);
    nextRow->addWidget(nextLabel);

    onion_next_spin_ = new QSpinBox(parent);
    onion_next_spin_->setRange(0, 10);
    onion_next_spin_->setValue(state_->onionNextFrames());
    onion_next_spin_->setToolTip("Next frames visible in blue tint");
    QObject::connect(onion_next_spin_, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v) {
        state_->setOnionNextFrames(v);
    });
    nextRow->addWidget(onion_next_spin_);
    nextRow->addStretch();
    layout->addLayout(nextRow);

    layout->addWidget(makeSliderWithSpin(parent, "Opacity", 5, 100, state_->onionOpacity(),
        &onion_opacity_slider_, &onion_opacity_spin_, [this](int v) {
            state_->setOnionOpacity(v);
        }));
}

// ============================================================================
// CANVAS SIZE Section (Accordion)
// ============================================================================

void ToolboxPanel::buildCanvasSizeSection(QWidget* parent, QVBoxLayout* layout) {
    auto* accordion = new AccordionSection("CANVAS SIZE", parent);

    auto* content = new QWidget(parent);
    auto* clayout = new QVBoxLayout(content);
    clayout->setContentsMargins(8, 6, 8, 6);
    clayout->setSpacing(6);

    auto* wRow = new QHBoxLayout();
    auto* wLabel = new QLabel("Width", content);
    wLabel->setStyleSheet("color:#aaa;font-size:10px;background:transparent;");
    wLabel->setFixedWidth(42);
    wRow->addWidget(wLabel);

    canvas_width_spin_ = new QSpinBox(content);
    canvas_width_spin_->setRange(1, 8192);
    canvas_width_spin_->setValue(state_->canvasWidth());
    canvas_width_spin_->setToolTip("Canvas width in pixels");
    QObject::connect(canvas_width_spin_, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v) {
        state_->setCanvasWidth(v);
    });
    wRow->addWidget(canvas_width_spin_);
    wRow->addStretch();
    clayout->addLayout(wRow);

    auto* hRow = new QHBoxLayout();
    auto* hLabel = new QLabel("Height", content);
    hLabel->setStyleSheet("color:#aaa;font-size:10px;background:transparent;");
    hLabel->setFixedWidth(42);
    hRow->addWidget(hLabel);

    canvas_height_spin_ = new QSpinBox(content);
    canvas_height_spin_->setRange(1, 8192);
    canvas_height_spin_->setValue(state_->canvasHeight());
    canvas_height_spin_->setToolTip("Canvas height in pixels");
    QObject::connect(canvas_height_spin_, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v) {
        state_->setCanvasHeight(v);
    });
    hRow->addWidget(canvas_height_spin_);
    hRow->addStretch();
    clayout->addLayout(hRow);

    canvas_resize_btn_ = new QPushButton("Apply Resize", content);
    canvas_resize_btn_->setCursor(Qt::PointingHandCursor);
    canvas_resize_btn_->setStyleSheet(
        "QPushButton{background:#d4782a;color:white;border:none;border-radius:3px;"
        "padding:5px 12px;font-size:10px;font-weight:bold;}"
        "QPushButton:hover{background:#e88c3a;}"
        "QPushButton:pressed{background:#b06520;}");
    QObject::connect(canvas_resize_btn_, &QPushButton::clicked, [this]() {
        emit canvasResized(state_->canvasWidth(), state_->canvasHeight());
    });
    clayout->addWidget(canvas_resize_btn_);

    accordion->setContentWidget(content);
    accordion->setExpanded(false);
    layout->addWidget(accordion);
}

// ============================================================================
// syncFromState
// ============================================================================

void ToolboxPanel::syncFromState() {
    if (!state_) return;

    int newId = static_cast<int>(state_->activeTool());
    auto* btn = tool_group_->button(newId);
    if (btn) btn->setChecked(true);

    color_swatch_->setSwatchColor(state_->primaryColor());
    shape_combo_->setCurrentText(state_->brushShape());
    pressure_size_cb_->setChecked(state_->pressureSize());
    pressure_opacity_cb_->setChecked(state_->pressureOpacity());
    stabilizer_spin_->setValue(state_->stabilizerLevel());
    onion_enabled_cb_->setChecked(state_->onionEnabled());
    onion_prev_spin_->setValue(state_->onionPrevFrames());
    onion_next_spin_->setValue(state_->onionNextFrames());
    onion_opacity_spin_->setValue(state_->onionOpacity());
    canvas_width_spin_->setValue(state_->canvasWidth());
    canvas_height_spin_->setValue(state_->canvasHeight());
}

} // namespace fap
