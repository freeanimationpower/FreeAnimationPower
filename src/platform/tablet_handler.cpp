#include "platform/tablet_handler.hpp"

#include <QPointF>
#include <QTabletEvent>

namespace fap {

TabletHandler::TabletHandler() {
    reset();
}

void TabletHandler::handleEvent(QTabletEvent* event) {
    if (!event) return;

    state_.active   = true;
    state_.pressure = static_cast<float>(event->pressure());
    state_.tiltX    = static_cast<float>(event->xTilt());
    state_.tiltY    = static_cast<float>(event->yTilt());
    state_.rotation = static_cast<float>(event->rotation());
    state_.position = event->position();
    state_.eraser   = (event->pointerType() == QPointingDevice::PointerType::Eraser);

    switch (event->type()) {
        case QEvent::TabletPress:
            last_event_type_ = EventType::Press;
            break;
        case QEvent::TabletRelease:
            last_event_type_ = EventType::Release;
            state_.pressure  = 0.0f;
            state_.active    = false;
            break;
        case QEvent::TabletMove:
            last_event_type_ = EventType::Move;
            break;
        case QEvent::TabletEnterProximity:
            state_.active = true;
            break;
        case QEvent::TabletLeaveProximity:
            state_.active = false;
            break;
        default:
            break;
    }
}

TabletState TabletHandler::state() const {
    return state_;
}

bool TabletHandler::isTabletActive() const {
    return state_.active;
}

void TabletHandler::reset() {
    state_.active   = false;
    state_.eraser   = false;
    state_.pressure = 0.0f;
    state_.tiltX    = 0.0f;
    state_.tiltY    = 0.0f;
    state_.rotation = 0.0f;
    state_.position = QPointF(0.0, 0.0);
    last_event_type_ = EventType::None;
}

} // namespace fap
