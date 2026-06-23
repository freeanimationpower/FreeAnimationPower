#pragma once

#include <QWidget>
#include <QPushButton>
#include <QColor>
#include <QString>
#include <vector>

class QButtonGroup;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QSlider;
class QLabel;
class QVBoxLayout;

namespace fap {

class ToolState;

class ColorSwatchButton : public QPushButton {
    Q_OBJECT
public:
    explicit ColorSwatchButton(QWidget* parent = nullptr);
    void setSwatchColor(const QColor& color);
    QColor swatchColor() const;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QColor color_ = Qt::black;
};

class AccordionSection : public QWidget {
    Q_OBJECT
public:
    explicit AccordionSection(const QString& title, QWidget* parent = nullptr);
    void setContentWidget(QWidget* content);
    void setExpanded(bool expanded);
    bool isExpanded() const;

private:
    QPushButton* header_btn_ = nullptr;
    QWidget* content_ = nullptr;
    QLabel* arrow_ = nullptr;
    bool expanded_ = false;
};

class ToolboxPanel : public QWidget {
    Q_OBJECT
public:
    explicit ToolboxPanel(ToolState* state, QWidget* parent = nullptr);

    void syncFromState();

signals:
    void canvasResized(int width, int height);

private:
    void buildToolsSection(QWidget* parent, QVBoxLayout* layout);
    void buildColorSection(QWidget* parent, QVBoxLayout* layout);
    void buildBrushSettingsSection(QWidget* parent, QVBoxLayout* layout);
    void buildOnionSkinSection(QWidget* parent, QVBoxLayout* layout);
    void buildCanvasSizeSection(QWidget* parent, QVBoxLayout* layout);
    void applyStyles();

    ToolState* state_ = nullptr;
    QButtonGroup* tool_group_ = nullptr;
    std::vector<QPushButton*> tool_buttons_;

    ColorSwatchButton* color_swatch_ = nullptr;

    QSlider* size_slider_ = nullptr;
    QSpinBox* size_spin_ = nullptr;
    QSlider* opacity_slider_ = nullptr;
    QSpinBox* opacity_spin_ = nullptr;
    QSlider* hardness_slider_ = nullptr;
    QSpinBox* hardness_spin_ = nullptr;
    QComboBox* shape_combo_ = nullptr;
    QCheckBox* pressure_size_cb_ = nullptr;
    QCheckBox* pressure_opacity_cb_ = nullptr;
    QSpinBox* stabilizer_spin_ = nullptr;

    QCheckBox* onion_enabled_cb_ = nullptr;
    QSpinBox* onion_prev_spin_ = nullptr;
    QSpinBox* onion_next_spin_ = nullptr;
    QSlider* onion_opacity_slider_ = nullptr;
    QSpinBox* onion_opacity_spin_ = nullptr;

    QSpinBox* canvas_width_spin_ = nullptr;
    QSpinBox* canvas_height_spin_ = nullptr;
    QPushButton* canvas_resize_btn_ = nullptr;
};

} // namespace fap
