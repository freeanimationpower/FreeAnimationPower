#include "busy_overlay.hpp"
#include <QPainter>
#include <QEvent>
#include <QResizeEvent>

namespace fap {

BusyOverlay::BusyOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::StrongFocus);

    timer_ = new QTimer(this);
    timer_->setInterval(30);
    connect(timer_, &QTimer::timeout, this, [this]() {
        currentCounter_ = (currentCounter_ + 1) % numLines_;
        update();
    });

    hide();
}

void BusyOverlay::setMessage(const QString& msg)
{
    message_ = msg;
    if (isVisible()) update();
}

void BusyOverlay::showOverlay()
{
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
        if (!filterInstalled_) {
            parentWidget()->installEventFilter(this);
            filterInstalled_ = true;
        }
    }
    QWidget::show();
    raise();
    setFocus();
    if (!timer_->isActive()) timer_->start();
    blocking_ = true;
}

void BusyOverlay::hideOverlay()
{
    timer_->stop();
    blocking_ = false;
    if (filterInstalled_ && parentWidget()) {
        parentWidget()->removeEventFilter(this);
        filterInstalled_ = false;
    }
    QWidget::hide();
}

void BusyOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.fillRect(rect(), QColor(0, 0, 0, 200));

    int cx = width() / 2;
    int cy = height() / 2 - 10;
    int radius = 28;
    int lineLen = 18;
    int lineW = 3;

    p.translate(cx, cy);

    for (int i = 0; i < numLines_; ++i) {
        p.save();
        qreal a = 360.0 * i / numLines_;
        p.rotate(a);

        int dist = (i - currentCounter_ + numLines_) % numLines_;
        qreal alpha = 1.0 - (qreal)dist / numLines_ * 0.85;
        alpha = std::max(0.12, std::min(1.0, alpha));

        QColor c = spinnerColor_;
        c.setAlphaF(alpha);
        p.setPen(QPen(c, lineW, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(radius, 0, radius + lineLen, 0);

        p.restore();
    }

    if (!message_.isEmpty()) {
        p.resetTransform();
        p.setPen(QColor("#E8ECF0"));
        QFont f = font();
        f.setPointSize(14);
        p.setFont(f);
        QFontMetrics fm(f);
        int textW = fm.horizontalAdvance(message_);
        p.drawText((width() - textW) / 2, cy + radius + lineLen + 30, message_);
    }
}

bool BusyOverlay::eventFilter(QObject* obj, QEvent* event)
{
    if (!blocking_) return false;
    if (obj == parentWidget()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::Wheel:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            return true;
        case QEvent::Resize:
            setGeometry(parentWidget()->rect());
            break;
        default:
            break;
        }
    }
    return false;
}

void BusyOverlay::resizeEvent(QResizeEvent*)
{
    if (parentWidget()) setGeometry(parentWidget()->rect());
}

} // namespace fap
