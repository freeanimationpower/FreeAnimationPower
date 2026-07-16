#pragma once

#include <QtWidgets/QWidget>

class QSpinBox;
class QPushButton;

namespace fap {

class CanvasSizePanel : public QWidget {
    Q_OBJECT
public:
    explicit CanvasSizePanel(QWidget* parent = nullptr);

    int canvasWidth() const;
    int canvasHeight() const;
    void setCanvasWidth(int w);
    void setCanvasHeight(int h);

signals:
    void canvasResized(int width, int height);

private:
    QSpinBox* widthSpin_ = nullptr;
    QSpinBox* heightSpin_ = nullptr;
};

} // namespace fap
