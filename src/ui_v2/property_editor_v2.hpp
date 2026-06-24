#pragma once

#include <QtWidgets/QWidget>
#include <memory>

class QSlider;
class QLabel;
class QSpinBox;
class QComboBox;
class QCheckBox;

namespace fap {

class AppState;

class PropertyEditorV2 : public QWidget {
    Q_OBJECT
public:
    explicit PropertyEditorV2(std::shared_ptr<AppState> state, QWidget* parent = nullptr);

    void refreshFields();

signals:
    void brushSizeChanged(int size);
    void brushOpacityChanged(int opacity);
    void brushHardnessChanged(int hardness);
    void brushShapeChanged(const QString& shape);
    void stabilizerChanged(int level);

private:
    void setupUI();
    void onSizeSliderChanged(int value);
    void onOpacitySliderChanged(int value);
    void onSizeSpinChanged(int value);
    void onOpacitySpinChanged(int value);
    void onHardnessSliderChanged(int value);
    void onHardnessSpinChanged(int value);
    void onStabilizerSliderChanged(int value);
    void onStabilizerSpinChanged(int value);
    void onShapeChanged(int index);
    void onToolChanged();
    void syncFromToolState();
    void showBrushControls();
    void showAllBrushControls();
    void showPlaceholder();

    std::shared_ptr<AppState> appState_;

    QWidget* brushGroup_ = nullptr;
    QWidget* placeholderWidget_ = nullptr;

    QLabel* sizeLabel_ = nullptr;
    QSlider* sizeSlider_ = nullptr;
    QSpinBox* sizeSpin_ = nullptr;

    QLabel* opacityLabel_ = nullptr;
    QSlider* opacitySlider_ = nullptr;
    QSpinBox* opacitySpin_ = nullptr;

    QLabel* hardnessLabel_ = nullptr;
    QSlider* hardnessSlider_ = nullptr;
    QSpinBox* hardnessSpin_ = nullptr;

    QLabel* shapeLabel_ = nullptr;
    QComboBox* shapeCombo_ = nullptr;

    QLabel* stabilizerLabel_ = nullptr;
    QSlider* stabilizerSlider_ = nullptr;
    QSpinBox* stabilizerSpin_ = nullptr;

    QCheckBox* pressureSizeCb_ = nullptr;
    QCheckBox* pressureOpacityCb_ = nullptr;

    QLabel* placeholderLabel_ = nullptr;

    bool updatingFromState_ = false;
};

} // namespace fap
