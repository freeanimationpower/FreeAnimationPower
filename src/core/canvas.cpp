#include "core/canvas.hpp"
#include <cmath>
#include <algorithm>

namespace fap {

Canvas::Canvas(int width, int height)
    : width_(width), height_(height) {}

int Canvas::width() const { return width_; }
int Canvas::height() const { return height_; }

Size Canvas::size() const { return { width_, height_ }; }

void Canvas::setSize(int w, int h) {
    width_ = w;
    height_ = h;
}

float Canvas::zoom() const { return zoom_; }

void Canvas::setZoom(float factor) {
    zoom_ = clampf(factor, 0.01f, 64.0f);
}

void Canvas::zoomAt(float factor, float cx, float cy) {
    float old_zoom = zoom_;
    setZoom(zoom_ * factor);
    float ratio = zoom_ / old_zoom;
    pan_x_ = cx - ratio * (cx - pan_x_);
    pan_y_ = cy - ratio * (cy - pan_y_);
}

float Canvas::panX() const { return pan_x_; }
float Canvas::panY() const { return pan_y_; }

void Canvas::setPan(float x, float y) {
    pan_x_ = x;
    pan_y_ = y;
}

void Canvas::panBy(float dx, float dy) {
    pan_x_ += dx;
    pan_y_ += dy;
}

float Canvas::rotation() const { return rotation_; }

void Canvas::setRotation(float degrees) {
    rotation_ = std::fmod(degrees, 360.0f);
    if (rotation_ < 0.0f) rotation_ += 360.0f;
}

void Canvas::rotateBy(float degrees) {
    setRotation(rotation_ + degrees);
}

bool Canvas::flipH() const { return flip_h_; }
bool Canvas::flipV() const { return flip_v_; }

void Canvas::setFlipH(bool flip) { flip_h_ = flip; }
void Canvas::setFlipV(bool flip) { flip_v_ = flip; }

Vec2 Canvas::worldToPixel(float wx, float wy) const {
    float x = wx;
    float y = wy;

    if (flip_h_) x = width_ - x;
    if (flip_v_) y = height_ - y;

    if (rotation_ != 0.0f) {
        float rad = rotation_ * 3.14159265f / 180.0f;
        float cos_r = std::cos(rad);
        float sin_r = std::sin(rad);
        float cx = width_ * 0.5f;
        float cy = height_ * 0.5f;
        float dx = x - cx;
        float dy = y - cy;
        x = cx + dx * cos_r - dy * sin_r;
        y = cy + dx * sin_r + dy * cos_r;
    }

    x = (x - pan_x_) * zoom_;
    y = (y - pan_y_) * zoom_;

    return { x, y };
}

Vec2 Canvas::pixelToWorld(float px, float py) const {
    float x = px / zoom_ + pan_x_;
    float y = py / zoom_ + pan_y_;

    if (rotation_ != 0.0f) {
        float rad = -rotation_ * 3.14159265f / 180.0f;
        float cos_r = std::cos(rad);
        float sin_r = std::sin(rad);
        float cx = width_ * 0.5f;
        float cy = height_ * 0.5f;
        float dx = x - cx;
        float dy = y - cy;
        x = cx + dx * cos_r - dy * sin_r;
        y = cy + dx * sin_r + dy * cos_r;
    }

    if (flip_h_) x = width_ - x;
    if (flip_v_) y = height_ - y;

    return { x, y };
}

float Canvas::worldToPixelX(float wx) const {
    return worldToPixel(wx, 0.0f).x;
}

float Canvas::worldToPixelY(float wy) const {
    return worldToPixel(0.0f, wy).y;
}

float Canvas::pixelToWorldX(float px) const {
    return pixelToWorld(px, 0.0f).x;
}

float Canvas::pixelToWorldY(float py) const {
    return pixelToWorld(0.0f, py).y;
}

void Canvas::resetView() {
    zoom_ = 1.0f;
    pan_x_ = 0.0f;
    pan_y_ = 0.0f;
    rotation_ = 0.0f;
    flip_h_ = false;
    flip_v_ = false;
}

} // namespace fap
