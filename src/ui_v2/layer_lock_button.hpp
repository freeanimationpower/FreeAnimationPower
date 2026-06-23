#pragma once

#include <QtWidgets/QPushButton>

namespace fap {

class LayerLockButton : public QPushButton {
    Q_OBJECT
    Q_PROPERTY(bool locked READ locked WRITE setLocked NOTIFY toggled)

public:
    explicit LayerLockButton(bool locked = false, QWidget* parent = nullptr);

    bool locked() const { return locked_; }

public slots:
    void setLocked(bool locked);

signals:
    void toggled(bool locked);

private:
    void updateAppearance();

    bool locked_ = false;

    static constexpr int kSize = 22;
    static const char* kUnlockedIcon;
    static const char* kLockedIcon;
    static const char* kNeutralColor;
    static const char* kLockedColor;
    static const char* kHoverBg;
    static const char* kBaseStyle;
};

} // namespace fap
