#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QPushButton>
#include <QtGui/QColor>
#include <memory>

class QButtonGroup;
class QSpinBox;

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

signals:
    void toolChanged(int toolIndex);
    void colorChanged(const QColor& color);
    void settingsChanged();

private:
    QPushButton* createToolButton(const QString& icon, const QString& tooltip, int id);

    std::shared_ptr<AppState> appState_;

    QButtonGroup* toolGroup_ = nullptr;
    ColorSwatchButtonV2* colorSwatch_ = nullptr;
};

} // namespace fap
