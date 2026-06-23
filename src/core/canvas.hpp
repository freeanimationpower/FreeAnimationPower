#pragma once

#include "types.hpp"

namespace fap {

class Canvas {
public:
    Canvas(int width = 1920, int height = 1080);

    int width() const;
    int height() const;
    Size size() const;
    void setSize(int w, int h);

    float zoom() const;
    void setZoom(float factor);
    void zoomAt(float factor, float cx, float cy);

    float panX() const;
    float panY() const;
    void setPan(float x, float y);
    void panBy(float dx, float dy);

    float rotation() const;
    void setRotation(float degrees);
    void rotateBy(float degrees);

    bool flipH() const;
    bool flipV() const;
    void setFlipH(bool flip);
    void setFlipV(bool flip);

    Vec2 worldToPixel(float wx, float wy) const;
    Vec2 pixelToWorld(float px, float py) const;

    float worldToPixelX(float wx) const;
    float worldToPixelY(float wy) const;
    float pixelToWorldX(float px) const;
    float pixelToWorldY(float py) const;

    void resetView();

private:
    int width_;
    int height_;
    float zoom_ = 1.0f;
    float pan_x_ = 0.0f;
    float pan_y_ = 0.0f;
    float rotation_ = 0.0f;
    bool flip_h_ = false;
    bool flip_v_ = false;
};

} // namespace fap
