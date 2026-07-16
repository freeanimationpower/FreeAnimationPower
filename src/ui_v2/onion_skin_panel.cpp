#include "onion_skin_panel.hpp"

#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

namespace fap {

static const char* kGroupBoxStyle =
    "QGroupBox { color:#C8CCD8; font-size:9px; font-weight:bold; "
    "border:1px solid #2D3139; border-radius:4px; margin-top:8px; padding:8px 4px 4px 4px; }"
    "QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 4px; }";

static const char* kCheckStyle =
    "QCheckBox { color:#C8CCD8; font-size:10px; }"
    "QCheckBox::indicator { width:12px; height:12px; }";

static const char* kSpinStyle =
    "QSpinBox { background:#13161D; color:#E8ECF0; border:1px solid #2D3139; "
    "border-radius:3px; padding:2px 4px; font-size:10px; }";

static const char* kSliderStyle =
    "QSlider::groove:horizontal { background:#2D3139; height:4px; border-radius:2px; }"
    "QSlider::handle:horizontal { background:#FF4800; width:10px; height:10px; "
    "margin:-3px 0; border-radius:5px; }"
    "QSlider::sub-page:horizontal { background:#FF4800; border-radius:2px; }";

static const char* kLabelStyle =
    "QLabel { color:#8B8FA3; font-size:10px; }";

OnionSkinPanel::OnionSkinPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* group = new QGroupBox("Onion Skin");
    group->setStyleSheet(kGroupBoxStyle);
    auto* onionLayout = new QVBoxLayout(group);
    onionLayout->setContentsMargins(0, 0, 0, 0);
    onionLayout->setSpacing(4);

    enableCb_ = new QCheckBox("Enabled");
    enableCb_->setToolTip("Onion Skin\nShow transparent overlays of previous and next frames for reference while animating.");
    enableCb_->setStyleSheet(kCheckStyle);
    QObject::connect(enableCb_, &QCheckBox::toggled, this, &OnionSkinPanel::settingsChanged);
    onionLayout->addWidget(enableCb_);

    auto* prevRow = new QHBoxLayout();
    auto* prevLabel = new QLabel("Prev:");
    prevLabel->setStyleSheet(kLabelStyle);
    prevRow->addWidget(prevLabel);
    prevSpin_ = new QSpinBox();
    prevSpin_->setRange(0, 10);
    prevSpin_->setValue(3);
    prevSpin_->setToolTip("Previous Frames\nNumber of previous frames shown as red-tinted onion skin overlays.");
    prevSpin_->setStyleSheet(kSpinStyle);
    prevSpin_->setFixedWidth(48);
    QObject::connect(prevSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
                     this, &OnionSkinPanel::settingsChanged);
    prevRow->addWidget(prevSpin_);
    prevRow->addStretch();
    onionLayout->addLayout(prevRow);

    auto* nextRow = new QHBoxLayout();
    auto* nextLabel = new QLabel("Next:");
    nextLabel->setStyleSheet(kLabelStyle);
    nextRow->addWidget(nextLabel);
    nextSpin_ = new QSpinBox();
    nextSpin_->setRange(0, 10);
    nextSpin_->setValue(1);
    nextSpin_->setToolTip("Next Frames\nNumber of future frames shown as green-tinted onion skin overlays.");
    nextSpin_->setStyleSheet(kSpinStyle);
    nextSpin_->setFixedWidth(48);
    QObject::connect(nextSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
                     this, &OnionSkinPanel::settingsChanged);
    nextRow->addWidget(nextSpin_);
    nextRow->addStretch();
    onionLayout->addLayout(nextRow);

    opacitySlider_ = new QSlider(Qt::Horizontal);
    opacitySlider_->setRange(5, 100);
    opacitySlider_->setValue(35);
    opacitySlider_->setToolTip("Onion Opacity (5-100%)\nTransparency of onion skin overlays. Lower = more faded ghost frames.");
    opacitySlider_->setStyleSheet(kSliderStyle);
    opacitySlider_->setFixedHeight(14);
    QObject::connect(opacitySlider_, &QSlider::valueChanged,
                     this, &OnionSkinPanel::settingsChanged);
    onionLayout->addWidget(opacitySlider_);

    layout->addWidget(group);
    layout->addStretch();
}

void OnionSkinPanel::setEnabled(bool e) {
    enableCb_->blockSignals(true);
    enableCb_->setChecked(e);
    enableCb_->blockSignals(false);
}

void OnionSkinPanel::setPrevFrames(int n) {
    prevSpin_->blockSignals(true);
    prevSpin_->setValue(n);
    prevSpin_->blockSignals(false);
}

void OnionSkinPanel::setNextFrames(int n) {
    nextSpin_->blockSignals(true);
    nextSpin_->setValue(n);
    nextSpin_->blockSignals(false);
}

void OnionSkinPanel::setOpacity(int v) {
    opacitySlider_->blockSignals(true);
    opacitySlider_->setValue(v);
    opacitySlider_->blockSignals(false);
}

} // namespace fap
