#include "layer_lock_button.hpp"

#include <QtGui/QKeySequence>

namespace fap {

const char* LayerLockButton::kUnlockedIcon = "\xF0\x9F\x94\x93";
const char* LayerLockButton::kLockedIcon   = "\xF0\x9F\x94\x92";
const char* LayerLockButton::kNeutralColor = "#C8CCD8";
const char* LayerLockButton::kLockedColor  = "#FF0000";
const char* LayerLockButton::kHoverBg      = "#2D3139";

const char* LayerLockButton::kBaseStyle =
    "QPushButton{"
    "  background:transparent;"
    "  border:none;"
    "  border-radius:3px;"
    "  font-size:13px;"
    "  padding:0px;"
    "}"
    "QPushButton:hover{"
    "  background:%1;"
    "}";

LayerLockButton::LayerLockButton(bool locked, QWidget* parent)
    : QPushButton(parent)
    , locked_(locked)
{
    setFixedSize(kSize, kSize);
    setCheckable(true);
    setChecked(locked_);
    setCursor(Qt::PointingHandCursor);

    setAccessibleName("Layer lock");
    setShortcut(QKeySequence("Ctrl+L"));

    connect(this, &QPushButton::clicked, this, [this]() {
        setLocked(!locked_);
    });

    updateAppearance();
}

void LayerLockButton::setLocked(bool locked)
{
    if (locked_ == locked) return;
    locked_ = locked;
    setChecked(locked_);
    updateAppearance();
    emit toggled(locked_);
}

void LayerLockButton::updateAppearance()
{
    const char* icon  = locked_ ? kLockedIcon : kUnlockedIcon;
    const char* color = locked_ ? kLockedColor : kNeutralColor;

    setText(icon);
    setToolTip(locked_ ? "Unlock this layer" : "Lock this layer");
    setAccessibleDescription(locked_
        ? "Layer is locked. Click to unlock."
        : "Layer is unlocked. Click to lock.");

    setStyleSheet(
        QString(kBaseStyle).arg(kHoverBg) +
        QString("QPushButton{color:%1;}").arg(color));
}

} // namespace fap
