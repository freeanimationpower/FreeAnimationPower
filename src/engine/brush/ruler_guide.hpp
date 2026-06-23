#pragma once

#include "core/types.hpp"
#include <vector>
#include <cmath>

namespace fap {

enum class GuideType {
    Linear,
    Radial,
    Mirror,
    Perspective,
    EllipseGuide,
    Concentric
};

struct GuidePoint {
    Vec2 position;
    bool active = true;
};

class Guide {
public:
    Guide(GuideType type);
    virtual ~Guide() = default;

    GuideType type() const { return type_; }
    bool enabled() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }

    const std::vector<GuidePoint>& points() const { return points_; }
    void setPoints(const std::vector<GuidePoint>& pts) { points_ = pts; }

    virtual Vec2 snap(const Vec2& pos, float threshold) const = 0;

protected:
    GuideType type_;
    bool enabled_ = true;
    std::vector<GuidePoint> points_;
};

class LinearGuide : public Guide {
public:
    LinearGuide();
    Vec2 snap(const Vec2& pos, float threshold) const override;
};

class RadialGuide : public Guide {
public:
    RadialGuide();
    void setCenter(const Vec2& center);
    Vec2 snap(const Vec2& pos, float threshold) const override;
};

class MirrorGuide : public Guide {
public:
    MirrorGuide();
    void setAxisPoint(const Vec2& p1, const Vec2& p2);
    Vec2 snap(const Vec2& pos, float threshold) const override;
    Vec2 mirror(const Vec2& pos) const;

private:
    Vec2 p1_{0, 0}, p2_{1, 0};
};

class PerspectiveGuide : public Guide {
public:
    PerspectiveGuide();

    void setVanishingPoint(int index, const Vec2& vp);
    Vec2 vanishingPoint(int index) const;
    int vanishingPointCount() const { return static_cast<int>(vps_.size()); }

    Vec2 snap(const Vec2& pos, float threshold) const override;

private:
    std::vector<Vec2> vps_;
    static constexpr int kMaxVPs = 3;
};

class RulerTool {
public:
    RulerTool();

    void addGuide(std::unique_ptr<Guide> guide);
    void removeGuide(int index);
    Guide* guideAt(int index);
    const Guide* guideAt(int index) const;
    const std::vector<std::unique_ptr<Guide>>& guides() const { return guides_; }
    size_t guideCount() const { return guides_.size(); }

    Vec2 snap(const Vec2& pos, float threshold = 8.0f) const;

    void setSnapEnabled(bool e) { snap_enabled_ = e; }
    bool snapEnabled() const { return snap_enabled_; }

    void setSnapThreshold(float t) { snap_threshold_ = t; }
    float snapThreshold() const { return snap_threshold_; }

private:
    std::vector<std::unique_ptr<Guide>> guides_;
    bool snap_enabled_ = true;
    float snap_threshold_ = 8.0f;
};

} // namespace fap
