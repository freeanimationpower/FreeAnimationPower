#include "toolbox_panel_v2.hpp"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QFrame>
#include <QtGui/QPainter>
#include <QtGui/QLinearGradient>
#include <QtGui/QIcon>

#include "core/app_state.hpp"
#include "core/tool_state.hpp"
#include "core/types.hpp"

namespace fap {

class ColorSwatchButtonV2 : public QPushButton {
    Q_OBJECT
public:
    explicit ColorSwatchButtonV2(QWidget* parent = nullptr)
        : QPushButton(parent)
    {
        setFixedSize(28, 28);
        setCursor(Qt::PointingHandCursor);
        setToolTip("Foreground Color\nClick to open color picker dialog.");
        setStyleSheet(
            "QPushButton { background: transparent; border: 2px solid #2D3139; "
            "border-radius: 6px; }"
            "QPushButton:hover { border-color: #FF4800; }");
    }

    void setSwatchColor(const QColor& color) {
        color_ = color;
        update();
    }

    QColor swatchColor() const { return color_; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(color_);
        p.drawRoundedRect(4, 4, width() - 8, height() - 8, 4, 4);
    }

private:
    QColor color_ = Qt::black;
};

static const struct {
    const char* name;
    const char* shortcut;
    const char* iconPath;
    const char* description;
} kTools[] = {
    { "Brush",       "B", ":/icons/tools/brush.png",       "Draw freehand strokes on raster layers." },
    { "Eraser",      "E", ":/icons/tools/eraser.png",      "Erase pixels from raster layers." },
    { "Pick Color",  "I", ":/icons/tools/pick_color.png",  "Sample a color from the canvas with the eyedropper." },
    { "Fill",        "G", ":/icons/tools/fill.png",        "Fill a contiguous area with the current foreground color." },
    { "Text",        "T", ":/icons/tools/text.png",        "Place text annotations on the canvas." },
    { "Line",        "L", ":/icons/tools/line.png",        "Draw straight line segments." },
    { "Rectangle",   "U", ":/icons/tools/rectangle.png",   "Draw rectangles by dragging." },
    { "Ellipse",     "Y", ":/icons/tools/ellipse.png",     "Draw ellipses by dragging." },
    { "Move",        "M", ":/icons/tools/move.png",        "Move layer content on the canvas." },
    { "Select",      "S", ":/icons/tools/select.png",      "Select a rectangular region of the canvas." },
    { "Hand",        "H", ":/icons/tools/hand.png",        "Pan and scroll the canvas view. Hold Space for quick access." },
};

static const int kToolCount = sizeof(kTools) / sizeof(kTools[0]);

static const char* kToolBaseStyle =
    "QPushButton {"
    "  background: transparent;"
    "  border: none;"
    "  border-radius: 4px;"
    "  color: #C0C3CC;"
    "  font-size: 16px;"
    "  padding: 0px;"
    "}"
    "QPushButton:hover {"
    "  background: #1E2130;"
    "  color: #E0E2EA;"
    "}"
    "QPushButton:checked {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
    "    stop:0 #FF4800, stop:1 #D43C00);"
    "  color: #FFFFFF;"
    "}";

static const char* kSliderStyle =
    "QSlider::groove:horizontal {"
    "  background: #1E2130;"
    "  height: 4px;"
    "  border-radius: 2px;"
    "}"
    "QSlider::handle:horizontal {"
    "  background: #FF4800;"
    "  width: 12px;"
    "  height: 12px;"
    "  margin: -5px 0;"
    "  border-radius: 6px;"
    "}"
    "QSlider::sub-page:horizontal {"
    "  background: #FF4800;"
    "  height: 4px;"
    "  border-radius: 2px;"
    "}";

static const char* kSpinStyle =
    "QSpinBox {"
    "  background: #1E2130;"
    "  color: #E0E2EA;"
    "  border: 1px solid #2D3139;"
    "  border-radius: 4px;"
    "  padding: 2px 4px;"
    "  font-size: 11px;"
    "}"
    "QSpinBox:hover { border-color: #3D4149; }"
    "QSpinBox::up-button, QSpinBox::down-button {"
    "  border: none;"
    "  width: 14px;"
    "}";

static const char* kCheckStyle =
    "QCheckBox {"
    "  color: #C0C3CC;"
    "  font-size: 11px;"
    "  spacing: 6px;"
    "}"
    "QCheckBox::indicator {"
    "  width: 14px;"
    "  height: 14px;"
    "  border: 1px solid #2D3139;"
    "  border-radius: 3px;"
    "  background: #1E2130;"
    "}"
    "QCheckBox::indicator:checked {"
    "  background: #FF4800;"
    "  border-color: #FF4800;"
    "}";

static const char* kGroupBoxStyle =
    "QGroupBox {"
    "  color: #FF4800;"
    "  font-size: 10px;"
    "  font-weight: 600;"
    "  text-transform: uppercase;"
    "  letter-spacing: 1px;"
    "  border: 1px solid #2D3139;"
    "  border-radius: 6px;"
    "  margin-top: 14px;"
    "  padding: 10px 8px 8px 8px;"
    "  background: #1A1D24;"
    "}"
    "QGroupBox::title {"
    "  subcontrol-origin: margin;"
    "  left: 8px;"
    "  padding: 0 6px;"
    "  background: #1A1D24;"
    "  border-radius: 3px;"
    "}";

QPushButton* ToolboxPanelV2::createToolButton(const QString& iconPath, const QString& tooltip, int id) {
    auto* btn = new QPushButton();
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(36, 32));
    btn->setCheckable(true);
    btn->setFixedSize(36, 32);
    btn->setToolTip(tooltip);
    btn->setStyleSheet(kToolBaseStyle);
    toolGroup_->addButton(btn, id);
    return btn;
}

ToolboxPanelV2::ToolboxPanelV2(std::shared_ptr<AppState> state, QWidget* parent)
    : QWidget(parent)
    , appState_(std::move(state))
{
    setMinimumWidth(200);
    setStyleSheet("background: #252830;");

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { border: none; background: #252830; }");

    auto* container = new QWidget();
    container->setStyleSheet("background: #252830;");
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 6, 4, 6);
    layout->setSpacing(2);

    toolGroup_ = new QButtonGroup(this);
    toolGroup_->setExclusive(true);

    for (int i = 0; i < kToolCount; ++i) {
        QString tip = QString("%1 Tool (%2)\n%3")
            .arg(kTools[i].name, kTools[i].shortcut, kTools[i].description);
        auto* btn = createToolButton(kTools[i].iconPath, tip, i);
        layout->addWidget(btn, 0, Qt::AlignHCenter);
    }

    QObject::connect(toolGroup_, &QButtonGroup::idClicked, [this](int id) {
        appState_->toolState().setActiveTool(static_cast<ToolType>(id));
        emit toolChanged(id);
    });

    layout->addSpacing(8);

    auto* sepLine = new QFrame();
    sepLine->setFrameShape(QFrame::HLine);
    sepLine->setStyleSheet("QFrame{color:#2D3139;max-height:1px;}");
    layout->addWidget(sepLine);

    layout->addSpacing(6);

    colorSwatch_ = new ColorSwatchButtonV2();
    QObject::connect(colorSwatch_, &QPushButton::clicked, [this]() {
        QColor chosen = QColorDialog::getColor(colorSwatch_->swatchColor(), this, "Choose Foreground Color");
        if (chosen.isValid()) {
            colorSwatch_->setSwatchColor(chosen);
            appState_->toolState().setPrimaryColor(chosen);
            emit colorChanged(chosen);
        }
    });
    layout->addWidget(colorSwatch_, 0, Qt::AlignHCenter);

    layout->addSpacing(10);

    auto* canvasGroup = new QGroupBox("Canvas");
    canvasGroup->setStyleSheet(kGroupBoxStyle);
    auto* canvasLayout = new QVBoxLayout(canvasGroup);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    canvasLayout->setSpacing(4);

    auto* sizeRow = new QHBoxLayout();
    canvasWidthSpin_ = new QSpinBox();
    canvasWidthSpin_->setRange(1, 8192);
    canvasWidthSpin_->setValue(appState_->toolState().canvasWidth());
    canvasWidthSpin_->setToolTip("Canvas Width (px)\nHorizontal size of the animation canvas. Range: 1-8192 pixels.");
    canvasWidthSpin_->setStyleSheet(kSpinStyle);
    sizeRow->addWidget(canvasWidthSpin_);

    auto* xLabel = new QLabel("\xC3\x97");
    xLabel->setStyleSheet("color:#6B7088;font-size:11px;");
    xLabel->setFixedWidth(12);
    xLabel->setAlignment(Qt::AlignCenter);
    sizeRow->addWidget(xLabel);

    canvasHeightSpin_ = new QSpinBox();
    canvasHeightSpin_->setRange(1, 8192);
    canvasHeightSpin_->setValue(appState_->toolState().canvasHeight());
    canvasHeightSpin_->setToolTip("Canvas Height (px)\nVertical size of the animation canvas. Range: 1-8192 pixels.");
    canvasHeightSpin_->setStyleSheet(kSpinStyle);
    sizeRow->addWidget(canvasHeightSpin_);
    canvasLayout->addLayout(sizeRow);

    QObject::connect(canvasWidthSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
        [this](int v) { appState_->toolState().setCanvasWidth(v); });
    QObject::connect(canvasHeightSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
        [this](int v) { appState_->toolState().setCanvasHeight(v); });

    auto* applyBtn = new QPushButton("Apply Resize");
    applyBtn->setToolTip("Apply Canvas Resize\nResize the canvas to the specified dimensions. This operation cannot be undone.");
    applyBtn->setStyleSheet(
        "QPushButton{background:#FF4800;color:#fff;border:none;border-radius:4px;padding:3px;font-size:10px;font-weight:bold;}"
        "QPushButton:hover{background:#FF6A20;}");
    QObject::connect(applyBtn, &QPushButton::clicked, [this]() {
        int w = canvasWidthSpin_->value();
        int h = canvasHeightSpin_->value();
        appState_->toolState().setCanvasSize(w, h);
        emit canvasResized(w, h);
    });
    canvasLayout->addWidget(applyBtn);

    layout->addWidget(canvasGroup);
    layout->addStretch();

    scroll->setWidget(container);
    outer->addWidget(scroll);

    syncFromState();
}

void ToolboxPanelV2::syncFromState() {
    auto& ts = appState_->toolState();

    int toolIdx = static_cast<int>(ts.activeTool());
    if (auto* btn = toolGroup_->button(toolIdx)) {
        btn->setChecked(true);
    }

    colorSwatch_->setSwatchColor(ts.primaryColor());

    canvasWidthSpin_->blockSignals(true);
    canvasWidthSpin_->setValue(ts.canvasWidth());
    canvasWidthSpin_->blockSignals(false);

    canvasHeightSpin_->blockSignals(true);
    canvasHeightSpin_->setValue(ts.canvasHeight());
    canvasHeightSpin_->blockSignals(false);
}

void ToolboxPanelV2::setColor(const QColor& color) {
    colorSwatch_->setSwatchColor(color);
    appState_->toolState().setPrimaryColor(color);
    emit colorChanged(color);
}

void ToolboxPanelV2::setActiveTool(int toolIndex) {
    if (auto* btn = toolGroup_->button(toolIndex)) {
        btn->setChecked(true);
    }
    appState_->toolState().setActiveToolByIndex(toolIndex);
}

QColor ToolboxPanelV2::currentColor() const     { return colorSwatch_->swatchColor(); }

int ToolboxPanelV2::canvasWidth() const         { return canvasWidthSpin_->value(); }
int ToolboxPanelV2::canvasHeight() const        { return canvasHeightSpin_->value(); }

} // namespace fap
#include "toolbox_panel_v2.moc"
