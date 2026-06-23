#pragma once

#include <QtWidgets/QWidget>

class QSpinBox;
class QLabel;

namespace fap {

class PropertyEditor : public QWidget {
    Q_OBJECT
public:
    explicit PropertyEditor(QWidget* parent = nullptr);

    void setValues(int width, int height, int fps);

signals:
    void canvasSizeChanged(int width, int height);
    void fpsChanged(int fps);

private:
    QSpinBox* width_spin_ = nullptr;
    QSpinBox* height_spin_ = nullptr;
    QSpinBox* fps_spin_ = nullptr;
    QLabel* memory_label_ = nullptr;
    QLabel* storage_label_ = nullptr;
};

} // namespace fap
