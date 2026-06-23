#pragma once

#include <QPointF>
#include <QTransform>

class QMouseEvent;
class QTabletEvent;

namespace fap {

enum class InputEventType {
    Press,
    Move,
    Release
};

struct InputEvent {
    float x = 0.0f;
    float y = 0.0f;
    float pressure = 0.0f;
    float tiltX = 0.0f;
    float tiltY = 0.0f;
    float rotation = 0.0f;
    bool isEraser = false;
    InputEventType type = InputEventType::Move;
};

class InputManager {
public:
    InputManager();

    InputEvent processMouseEvent(QMouseEvent* event);
    InputEvent processTabletEvent(QTabletEvent* event);

    void setCanvasTransform(const QTransform& transform);
    void resetTransform();

    QPointF toCanvas(QPointF screenPos) const;

    QTransform canvasTransform() const { return canvas_transform_; }
    float zoom() const;
    float offsetX() const;
    float offsetY() const;
    float rotation() const;

private:
    QTransform canvas_transform_;
};

} // namespace fap
