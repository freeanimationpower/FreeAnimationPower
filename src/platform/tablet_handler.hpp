#pragma once

#include <QPointF>

class QTabletEvent;

namespace fap {

struct TabletState {
    bool active = false;
    bool eraser = false;
    float pressure = 0.0f;
    float tiltX = 0.0f;
    float tiltY = 0.0f;
    float rotation = 0.0f;
    QPointF position = QPointF(0.0, 0.0);
};

class TabletHandler {
public:
    TabletHandler();

    void handleEvent(QTabletEvent* event);

    TabletState state() const;
    bool isTabletActive() const;

    void reset();

    enum class EventType { None, Press, Move, Release };
    EventType lastEventType() const { return last_event_type_; }

private:
    TabletState state_;
    EventType last_event_type_ = EventType::None;
};

} // namespace fap
