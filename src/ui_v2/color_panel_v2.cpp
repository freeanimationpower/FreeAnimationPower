#include "color_panel_v2.hpp"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFrame>
#include <QtWidgets/QColorDialog>

#include <algorithm>

namespace fap {

static const char* kPanelBg = "#161A24";
static const char* kTextColor = "#C8CCD8";
static const char* kAccentColor = "#FF4800";
static const char* kBorderColor = "#2D3139";
static const char* kInputBg = "#252830";

static const QColor kDefaultPalette[] = {
    Qt::black,
    Qt::white,
    QColor("#E05050"),
    QColor("#FF8C00"),
    QColor("#FFD700"),
    QColor("#4CAF50"),
    QColor("#2196F3"),
    QColor("#9C27B0"),
    QColor("#E91E94")
};

ColorPanelV2::ColorPanelV2(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QString("background:%1;").arg(kPanelBg));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    swatch_ = new QPushButton();
    swatch_->setFixedSize(48, 48);
    swatch_->setCursor(Qt::PointingHandCursor);
    swatch_->setStyleSheet(
        QString("QPushButton{border:2px solid %1;border-radius:24px;background:#000;}").arg(kBorderColor) +
        QString("QPushButton:hover{border-color:%1;}").arg("#C8CCD8"));
    swatch_->setToolTip("Click to choose color");

    QObject::connect(swatch_, &QPushButton::clicked, [this]() {
        QColor chosen = QColorDialog::getColor(current_color_, this, "Choose Color",
            QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            setColor(chosen);
            emitColorChanged();
        }
    });

    auto* swatchRow = new QHBoxLayout();
    swatchRow->addStretch();
    swatchRow->addWidget(swatch_);
    swatchRow->addStretch();
    layout->addLayout(swatchRow);

    palette_colors_.assign(std::begin(kDefaultPalette), std::end(kDefaultPalette));

    auto* paletteRow = new QHBoxLayout();
    paletteRow->setSpacing(4);

    for (int i = 0; i < kMaxPalette; ++i) {
        auto* circle = new QPushButton();
        circle->setFixedSize(16, 16);
        circle->setCursor(Qt::PointingHandCursor);
        QObject::connect(circle, &QPushButton::clicked, [this, i]() {
            if (i < static_cast<int>(palette_colors_.size())) {
                setColor(palette_colors_[i]);
                emitColorChanged();
            }
        });
        paletteRow->addWidget(circle);
        palette_circles_.push_back(circle);
    }

    layout->addLayout(paletteRow);

    updatePalette();
    setColor(Qt::black);
}

void ColorPanelV2::setColor(const QColor& color) {
    current_color_ = color;
    updateSwatch();

    auto it = std::find(palette_colors_.begin(), palette_colors_.end(), color);
    if (it != palette_colors_.end()) {
        palette_colors_.erase(it);
    }
    palette_colors_.insert(palette_colors_.begin(), color);
    if (static_cast<int>(palette_colors_.size()) > kMaxPalette) {
        palette_colors_.resize(kMaxPalette);
    }
    updatePalette();
}

QColor ColorPanelV2::currentColor() const {
    return current_color_;
}

void ColorPanelV2::updateSwatch() {
    QString style = QString("QPushButton{border:2px solid %1;border-radius:24px;background:rgba(%2,%3,%4,%5);}")
                        .arg(kBorderColor)
                        .arg(current_color_.red())
                        .arg(current_color_.green())
                        .arg(current_color_.blue())
                        .arg(current_color_.alphaF())
                    + QString("QPushButton:hover{border-color:%1;}").arg("#C8CCD8");
    swatch_->setStyleSheet(style);
}

void ColorPanelV2::updatePalette() {
    for (int i = 0; i < kMaxPalette; ++i) {
        QColor col = (i < static_cast<int>(palette_colors_.size()))
                         ? palette_colors_[i]
                         : QColor(64, 64, 64);
        palette_circles_[i]->setStyleSheet(
            QString("QPushButton{background:%1;border:1px solid %2;border-radius:8px;}")
                .arg(col.name(), kBorderColor) +
            QString("QPushButton:hover{border-color:%1;border-width:2px;}")
                .arg("#C8CCD8"));
    }
}

void ColorPanelV2::emitColorChanged() {
    emit colorChanged(current_color_);
}

} // namespace fap
