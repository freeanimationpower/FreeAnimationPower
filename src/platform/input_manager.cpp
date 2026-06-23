#include "platform/input_manager.hpp"

#include <QMouseEvent>
#include <QPointF>
#include <QTabletEvent>

#include <cmath>

namespace fap {

InputManager::InputManager() {
    resetTransform();
}

void InputManager::resetTransform() {
    canvas_transform_.reset();
}

void InputManager::setCanvasTransform(const QTransform& transform) {
    canvas_transform_ = transform;
}

QPointF InputManager::toCanvas(QPointF screenPos) const {
    bool invertible = false;
    QTransform inv = canvas_transform_.inverted(&invertible);
    if (!invertible) {
        return screenPos;
    }
    return inv.map(screenPos);
}

float InputManager::zoom() const {
    return canvas_transform_.m11();
}

float InputManager::offsetX() const {
    return canvas_transform_.m31();
}

float InputManager::offsetY() const {
    return canvas_transform_.m32();
}

float InputManager::rotation() const {
    return std::atan2(canvas_transform_.m21(), canvas_transform_.m11());
}

InputEvent InputManager::processMouseEvent(QMouseEvent* event) {
    InputEvent ie;
    ie.type = InputEventType::Move;
    ie.pressure = 1.0f;
    ie.tiltX = 0.0f;
    ie.tiltY = 0.0f;
    ie.rotation = 0.0f;
    ie.isEraser = false;

    QPointF canvas = toCanvas(event->position());
    ie.x = static_cast<float>(canvas.x());
    ie.y = static_cast<float>(canvas.y());

    switch (event->type()) {
        case QEvent::MouseButtonPress:
            ie.type = InputEventType::Press;
            ie.pressure = 0.5f;
            break;
        case QEvent::MouseButtonRelease:
            ie.type = InputEventType::Release;
            ie.pressure = 0.0f;
            break;
        case QEvent::MouseMove:
            ie.type = InputEventType::Move;
            if (event->buttons() & Qt::LeftButton) {
                ie.pressure = 0.5f;
            }
            break;
        default:
            break;
    }

    return ie;
}

InputEvent InputManager::processTabletEvent(QTabletEvent* event) {
    InputEvent ie;
    ie.type = InputEventType::Move;

    QPointF canvas = toCanvas(event->position());
    ie.x = static_cast<float>(canvas.x());
    ie.y = static_cast<float>(canvas.y());

    ie.pressure = static_cast<float>(event->pressure());
    ie.tiltX = static_cast<float>(event->xTilt());
    ie.tiltY = static_cast<float>(event->yTilt());
    ie.rotation = static_cast<float>(event->rotation());
    ie.isEraser = (event->pointerType() == QPointingDevice::PointerType::Eraser);

    switch (event->type()) {
        case QEvent::TabletPress:
            ie.type = InputEventType::Press;
            break;
        case QEvent::TabletRelease:
            ie.type = InputEventType::Release;
            ie.pressure = 0.0f;
            break;
        case QEvent::TabletMove:
            ie.type = InputEventType::Move;
            break;
        default:
            break;
    }

    return ie;
}

} // namespace fap
