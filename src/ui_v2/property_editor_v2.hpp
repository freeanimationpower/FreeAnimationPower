#pragma once

#include <QtWidgets/QWidget>
#include <memory>
#include <vector>

class QSlider;
class QLabel;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;

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
    void fillTypeChanged(int type);
    void primaryColorChanged(const QColor& color);

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
    void showPickColorControls();
    void showFillControls();
    void updateColorVariations();
    void onVariationClicked(int index);
    void onFillTypeComboChanged(int index);
    void onColorVariationTypeChanged(int index);
    void onLineStyleChanged(int index);
    QColor generateVariation(const QColor& base, int index, int total) const;

    std::shared_ptr<AppState> appState_;

    QWidget* brushGroup_ = nullptr;
    QWidget* placeholderWidget_ = nullptr;
    QWidget* pickColorGroup_ = nullptr;
    QWidget* fillGroup_ = nullptr;

    // Brush controls
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

    // Pick Color controls
    QPushButton* colorSwatch_ = nullptr;
    QLabel* colorHexLabel_ = nullptr;
    QLabel* pickColorLabel_ = nullptr;
    QComboBox* variationTypeCombo_ = nullptr;
    QWidget* variationGrid_ = nullptr;
    std::vector<QPushButton*> variationButtons_;

    // Fill controls
    QLabel* fillTypeLabel_ = nullptr;
    QComboBox* fillTypeCombo_ = nullptr;

    // Line tool controls
    QLabel* lineStyleLabel_ = nullptr;
    QComboBox* lineStyleCombo_ = nullptr;

    bool updatingFromState_ = false;
};

} // namespace fap
