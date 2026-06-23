#pragma once

#include <deque>

#include <QtWidgets/QWidget>
#include <QtWidgets/QPushButton>
#include <QtGui/QColor>

class QFrame;
class QSpinBox;
class QLineEdit;

namespace fap {

class ColorPanel : public QWidget {
    Q_OBJECT
public:
    explicit ColorPanel(QWidget* parent = nullptr);

    void setColor(const QColor& color);
    void previewColor(const QColor& color);
    void addRecentColor(const QColor& color);
    QColor currentColor() const;

signals:
    void colorChanged(const QColor& color);

private:
    void updateFromSpinboxes();
    void updateSwatch();
    void updateRecentSlots();
    void emitColorChanged();
    void blockSpinSignals(bool block);

    QFrame* swatch_ = nullptr;
    QSpinBox* r_spin_ = nullptr;
    QSpinBox* g_spin_ = nullptr;
    QSpinBox* b_spin_ = nullptr;
    QSpinBox* a_spin_ = nullptr;
    QLineEdit* hex_input_ = nullptr;
    QPushButton* recent_slots_[10] = {};

    static constexpr int kMaxRecent = 10;
    std::deque<QColor> recentColors_;
    QColor current_color_ = Qt::black;
};

} // namespace fap
