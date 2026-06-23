#include "color_panel.hpp"

#include <algorithm>

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFrame>
#include <QtGui/QRegularExpressionValidator>

namespace fap {

ColorPanel::ColorPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    swatch_ = new QFrame();
    swatch_->setFixedHeight(50);
    swatch_->setStyleSheet(
        "QFrame { border: 2px solid #555; border-radius: 4px; background: #000; }");
    layout->addWidget(swatch_);

    auto* rgbaLayout = new QGridLayout();
    rgbaLayout->setSpacing(3);

    auto makeSpin = [this, &rgbaLayout](const QString& label, int max, int def, int row) {
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet("color: #c0c0c0; font-size: 11px;");
        lbl->setFixedWidth(14);
        auto* spin = new QSpinBox();
        spin->setRange(0, max);
        spin->setValue(def);
        spin->setStyleSheet(
            "QSpinBox { background: #3e3e42; color: #e0e0e0; border: 1px solid #555; "
            "border-radius: 3px; padding: 3px; }"
            "QSpinBox:focus { border-color: #007acc; }");
        QObject::connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int) { updateFromSpinboxes(); });

        rgbaLayout->addWidget(lbl, row, 0);
        rgbaLayout->addWidget(spin, row, 1);
        return spin;
    };

    r_spin_ = makeSpin("R", 255, 0, 0);
    g_spin_ = makeSpin("G", 255, 0, 1);
    b_spin_ = makeSpin("B", 255, 0, 2);
    a_spin_ = makeSpin("A", 255, 255, 3);

    layout->addLayout(rgbaLayout);

    auto* hexRow = new QHBoxLayout();
    auto* hexLabel = new QLabel("#");
    hexLabel->setStyleSheet("color: #c0c0c0; font-size: 13px;");
    hex_input_ = new QLineEdit();
    hex_input_->setText("000000");
    hex_input_->setMaxLength(7);
    hex_input_->setPlaceholderText("RRGGBB");
    hex_input_->setStyleSheet(
        "QLineEdit { background: #3e3e42; color: #e0e0e0; border: 1px solid #555; "
        "border-radius: 3px; padding: 4px; font-family: 'Consolas'; }"
            "QLineEdit:focus { border-color: #007acc; }");
    hex_input_->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[0-9A-Fa-f]{0,6}"), hex_input_));

    QObject::connect(hex_input_, &QLineEdit::editingFinished, [this]() {
        QString hex = hex_input_->text().trimmed();
        if (hex.length() == 6) {
            bool ok;
            int val = hex.toInt(&ok, 16);
            if (ok) {
                int r = (val >> 16) & 0xFF;
                int g = (val >> 8) & 0xFF;
                int b = val & 0xFF;
                blockSpinSignals(true);
                r_spin_->setValue(r);
                g_spin_->setValue(g);
                b_spin_->setValue(b);
                blockSpinSignals(false);
                updateSwatch();
                emitColorChanged();
            }
        }
    });

    hexRow->addWidget(hexLabel);
    hexRow->addWidget(hex_input_, 1);
    layout->addLayout(hexRow);

    auto* recentLabel = new QLabel("Recent Colors");
    recentLabel->setStyleSheet("color: #c0c0c0; font-size: 11px;");
    layout->addWidget(recentLabel);

    auto* recentGrid = new QGridLayout();
    recentGrid->setSpacing(2);

    QColor defaults[10] = {
        Qt::black, Qt::white, QColor("#d4782a"),
        Qt::red, Qt::green, Qt::blue,
        Qt::yellow, Qt::cyan, Qt::magenta, Qt::gray
    };
    for (int i = 0; i < 10; ++i)
        recentColors_.push_back(defaults[i]);

    for (int i = 0; i < 10; ++i) {
        auto* slot = new QPushButton();
        slot->setFixedSize(20, 20);
        slot->setMinimumSize(16, 16);
        slot->setCursor(Qt::PointingHandCursor);
        slot->setStyleSheet(
            "QPushButton { background: " + recentColors_[i].name() + "; "
            "border: 1px solid #555; border-radius: 2px; }"
            "QPushButton:hover { border-color: #007acc; }");
        recent_slots_[i] = slot;

        QObject::connect(slot, &QPushButton::clicked, [this, i]() {
            if (i < static_cast<int>(recentColors_.size())) {
                QColor col = recentColors_[i];
                setColor(col);
                emit colorChanged(col);
            }
        });

        recentGrid->addWidget(slot, i / 5, i % 5);
    }
    layout->addLayout(recentGrid);
    layout->addStretch();

    setColor(Qt::black);
}

void ColorPanel::setColor(const QColor& color) {
    current_color_ = color;
    blockSpinSignals(true);
    r_spin_->setValue(color.red());
    g_spin_->setValue(color.green());
    b_spin_->setValue(color.blue());
    a_spin_->setValue(color.alpha());
    blockSpinSignals(false);
    hex_input_->setText(QString("%1").arg(
        (color.red() << 16) | (color.green() << 8) | color.blue(), 6, 16, QLatin1Char('0')).toUpper());
    updateSwatch();
    addRecentColor(color);
}

void ColorPanel::previewColor(const QColor& color)
{
    current_color_ = color;
    blockSpinSignals(true);
    r_spin_->setValue(color.red());
    g_spin_->setValue(color.green());
    b_spin_->setValue(color.blue());
    a_spin_->setValue(color.alpha());
    blockSpinSignals(false);
    hex_input_->setText(QString("%1").arg(
        (color.red() << 16) | (color.green() << 8) | color.blue(), 6, 16, QLatin1Char('0')).toUpper());
    updateSwatch();
}

QColor ColorPanel::currentColor() const { return current_color_; }

void ColorPanel::updateFromSpinboxes() {
    QColor c(r_spin_->value(), g_spin_->value(), b_spin_->value(), a_spin_->value());
    current_color_ = c;
    hex_input_->setText(QString("%1").arg(
        (c.red() << 16) | (c.green() << 8) | c.blue(), 6, 16, QLatin1Char('0')).toUpper());
    updateSwatch();
    addRecentColor(c);
    emit colorChanged(c);
}

void ColorPanel::updateSwatch() {
    swatch_->setStyleSheet(
        QString("QFrame { border: 2px solid #555; border-radius: 4px; "
                "background: rgba(%1,%2,%3,%4); }")
            .arg(current_color_.red())
            .arg(current_color_.green())
            .arg(current_color_.blue())
            .arg(current_color_.alphaF()));
}

void ColorPanel::addRecentColor(const QColor& color)
{
    if (!color.isValid()) return;

    if (!recentColors_.empty() && recentColors_.front() == color)
        return;

    auto it = std::find(recentColors_.begin(), recentColors_.end(), color);
    if (it != recentColors_.end())
        recentColors_.erase(it);

    recentColors_.push_front(color);
    while (static_cast<int>(recentColors_.size()) > kMaxRecent)
        recentColors_.pop_back();

    updateRecentSlots();
}

void ColorPanel::updateRecentSlots()
{
    for (int i = 0; i < kMaxRecent; ++i) {
        QColor col = (i < static_cast<int>(recentColors_.size()))
            ? recentColors_[i] : Qt::transparent;
        recent_slots_[i]->setStyleSheet(
            "QPushButton { background: " + col.name() + "; "
            "border: 1px solid #555; border-radius: 3px; }"
            "QPushButton:hover { border-color: #007acc; }");
    }
}

void ColorPanel::emitColorChanged() {
    emit colorChanged(current_color_);
}

void ColorPanel::blockSpinSignals(bool block) {
    r_spin_->blockSignals(block);
    g_spin_->blockSignals(block);
    b_spin_->blockSignals(block);
    a_spin_->blockSignals(block);
}

} // namespace fap
#include "color_panel.moc"
