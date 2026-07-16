#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QSlider>

namespace fap {

class OnionSkinPanel : public QWidget {
    Q_OBJECT
public:
    explicit OnionSkinPanel(QWidget* parent = nullptr);

    bool enabled() const { return enableCb_->isChecked(); }
    int prevFrames() const { return prevSpin_->value(); }
    int nextFrames() const { return nextSpin_->value(); }
    int opacity() const { return opacitySlider_->value(); }

    void setEnabled(bool e);
    void setPrevFrames(int n);
    void setNextFrames(int n);
    void setOpacity(int v);

signals:
    void settingsChanged();

private:
    QCheckBox* enableCb_ = nullptr;
    QSpinBox* prevSpin_ = nullptr;
    QSpinBox* nextSpin_ = nullptr;
    QSlider* opacitySlider_ = nullptr;
};

} // namespace fap
