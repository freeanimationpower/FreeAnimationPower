#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QPushButton>
#include <QtGui/QColor>
#include <memory>

class QButtonGroup;
class QCheckBox;
class QSpinBox;
class QSlider;

namespace fap {

class AppState;
class ColorSwatchButtonV2;

class ToolboxPanelV2 : public QWidget {
    Q_OBJECT
public:
    explicit ToolboxPanelV2(std::shared_ptr<AppState> state, QWidget* parent = nullptr);

    void setColor(const QColor& color);
    void setActiveTool(int toolIndex);
    void syncFromState();

    QColor currentColor() const;

    bool onionEnabled() const;
    int onionPrevFrames() const;
    int onionNextFrames() const;
    int onionOpacity() const;

    int canvasWidth() const;
    int canvasHeight() const;

signals:
    void toolChanged(int toolIndex);
    void colorChanged(const QColor& color);
    void settingsChanged();
    void onionSettingsChanged();
    void canvasResized(int width, int height);

private:
    QPushButton* createToolButton(const QString& icon, const QString& tooltip, int id);

    std::shared_ptr<AppState> appState_;

    QButtonGroup* toolGroup_ = nullptr;
    ColorSwatchButtonV2* colorSwatch_ = nullptr;

    QCheckBox* onionEnabledCb_ = nullptr;
    QSpinBox* onionPrevSpin_ = nullptr;
    QSpinBox* onionNextSpin_ = nullptr;
    QSlider* onionOpacitySlider_ = nullptr;

    QSpinBox* canvasWidthSpin_ = nullptr;
    QSpinBox* canvasHeightSpin_ = nullptr;
};

} // namespace fap
