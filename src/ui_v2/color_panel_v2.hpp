#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <vector>

class QPushButton;
class QSpinBox;

namespace fap {

class ColorPanelV2 : public QWidget {
    Q_OBJECT
public:
    explicit ColorPanelV2(QWidget* parent = nullptr);

    void setColor(const QColor& color);
    QColor currentColor() const;

signals:
    void colorChanged(const QColor& color);

private:
    void updateSwatch();
    void updatePalette();
    void emitColorChanged();
    void blockSpinSignals(bool block);

    QPushButton* swatch_ = nullptr;
    QSpinBox* r_spin_ = nullptr;
    QSpinBox* g_spin_ = nullptr;
    QSpinBox* b_spin_ = nullptr;
    QSpinBox* a_spin_ = nullptr;
    QColor current_color_ = Qt::black;

    std::vector<QPushButton*> palette_circles_;
    std::vector<QColor> palette_colors_;
    static constexpr int kMaxPalette = 9;
};

} // namespace fap
