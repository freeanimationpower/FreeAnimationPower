#pragma once
#include <QWidget>
#include <QTimer>
#include <QColor>
#include <QString>

namespace fap {

class BusyOverlay : public QWidget {
    Q_OBJECT
public:
    explicit BusyOverlay(QWidget* parent = nullptr);
    void setMessage(const QString& message);

public slots:
    void showOverlay();
    void hideOverlay();

protected:
    void paintEvent(QPaintEvent*) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override {}
    void keyReleaseEvent(QKeyEvent*) override {}

private:
    QString message_;
    int currentCounter_ = 0;
    int numLines_ = 12;
    QColor spinnerColor_{255, 72, 0};
    QTimer* timer_ = nullptr;
    bool blocking_ = false;
    bool filterInstalled_ = false;
};

} // namespace fap
