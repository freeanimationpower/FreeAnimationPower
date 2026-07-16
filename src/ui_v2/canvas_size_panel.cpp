#include "canvas_size_panel.hpp"

#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace fap {

static const char* kGroupBoxStyle =
    "QGroupBox { color:#C8CCD8; font-size:9px; font-weight:bold; "
    "border:1px solid #2D3139; border-radius:4px; margin-top:8px; padding:8px 4px 4px 4px; }"
    "QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 4px; }";

static const char* kSpinStyle =
    "QSpinBox { background:#13161D; color:#E8ECF0; border:1px solid #2D3139; "
    "border-radius:3px; padding:2px 4px; font-size:10px; }";

CanvasSizePanel::CanvasSizePanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* group = new QGroupBox("Canvas Size");
    group->setStyleSheet(kGroupBoxStyle);
    auto* groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(4, 4, 4, 4);
    groupLayout->setSpacing(4);

    auto* sizeRow = new QHBoxLayout();
    widthSpin_ = new QSpinBox();
    widthSpin_->setRange(1, 8192);
    widthSpin_->setValue(1920);
    widthSpin_->setToolTip("Canvas Width (px)\nHorizontal size of the animation canvas. Range: 1-8192 pixels.");
    widthSpin_->setStyleSheet(kSpinStyle);
    sizeRow->addWidget(widthSpin_);

    auto* xLabel = new QLabel("\xC3\x97");
    xLabel->setStyleSheet("color:#6B7088;font-size:11px;");
    xLabel->setFixedWidth(12);
    xLabel->setAlignment(Qt::AlignCenter);
    sizeRow->addWidget(xLabel);

    heightSpin_ = new QSpinBox();
    heightSpin_->setRange(1, 8192);
    heightSpin_->setValue(1080);
    heightSpin_->setToolTip("Canvas Height (px)\nVertical size of the animation canvas. Range: 1-8192 pixels.");
    heightSpin_->setStyleSheet(kSpinStyle);
    sizeRow->addWidget(heightSpin_);
    groupLayout->addLayout(sizeRow);

    auto* applyBtn = new QPushButton("Apply Resize");
    applyBtn->setToolTip("Apply Canvas Resize\nResize the canvas to the specified dimensions. This operation cannot be undone.");
    applyBtn->setStyleSheet(
        "QPushButton{background:#FF4800;color:#fff;border:none;border-radius:4px;padding:3px;font-size:10px;font-weight:bold;}"
        "QPushButton:hover{background:#FF6A20;}");
    QObject::connect(applyBtn, &QPushButton::clicked, [this]() {
        emit canvasResized(widthSpin_->value(), heightSpin_->value());
    });
    groupLayout->addWidget(applyBtn);

    layout->addWidget(group);
    layout->addStretch();
}

int CanvasSizePanel::canvasWidth() const  { return widthSpin_->value(); }
int CanvasSizePanel::canvasHeight() const { return heightSpin_->value(); }

void CanvasSizePanel::setCanvasWidth(int w) {
    widthSpin_->blockSignals(true);
    widthSpin_->setValue(w);
    widthSpin_->blockSignals(false);
}

void CanvasSizePanel::setCanvasHeight(int h) {
    heightSpin_->blockSignals(true);
    heightSpin_->setValue(h);
    heightSpin_->blockSignals(false);
}

} // namespace fap
