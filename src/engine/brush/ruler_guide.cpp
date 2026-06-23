#include "ruler_guide.hpp"
#include <algorithm>
#include <limits>

namespace fap {

Guide::Guide(GuideType type) : type_(type) {}

LinearGuide::LinearGuide() : Guide(GuideType::Linear) {}

Vec2 LinearGuide::snap(const Vec2& pos, float threshold) const
{
    if (points_.size() < 2) return pos;

    Vec2 best = pos;
    float bestDist = threshold;

    Vec2 lineDir = points_[1].position - points_[0].position;
    float lineLen = lineDir.length();
    if (lineLen < 1e-6f) return pos;
    lineDir = lineDir.normalized();

    float t = (pos - points_[0].position).dot(lineDir);
    t = std::clamp(t, 0.0f, lineLen);
    Vec2 proj = points_[0].position + lineDir * t;
    float dist = Vec2::distance(pos, proj);

    if (dist < bestDist) {
        best = proj;
    }

    if (points_.size() >= 3) {
        Vec2 dir2 = points_[2].position - points_[0].position;
        float len2 = dir2.length();
        if (len2 > 1e-6f) {
            dir2 = dir2.normalized();
            float t2 = (pos - points_[0].position).dot(dir2);
            t2 = std::clamp(t2, 0.0f, len2);
            Vec2 proj2 = points_[0].position + dir2 * t2;
            float dist2 = Vec2::distance(pos, proj2);
            if (dist2 < bestDist) {
                best = proj2;
            }
        }
    }

    Vec2 normal = { -lineDir.y, lineDir.x };
    float symT = (pos - points_[0].position).dot(normal);
    Vec2 reflected = pos - normal * (2.0f * symT);
    float reflectDist = Vec2::distance(pos, reflected);
    if (reflectDist < bestDist) {
        best = reflected;
    }

    return best;
}

RadialGuide::RadialGuide() : Guide(GuideType::Radial) {}

void RadialGuide::setCenter(const Vec2& center)
{
    if (points_.empty()) {
        points_.push_back({ center });
    } else {
        points_[0].position = center;
    }
}

Vec2 RadialGuide::snap(const Vec2& pos, float threshold) const
{
    if (points_.empty()) return pos;

    Vec2 center = points_[0].position;
    Vec2 dir = pos - center;
    float dist = dir.length();

    for (size_t i = 1; i < points_.size(); ++i) {
        Vec2 guideDir = points_[i].position - center;
        float gLen = guideDir.length();
        if (gLen < 1e-6f) continue;
        guideDir = guideDir.normalized();

        float angle = std::acos(std::clamp(dir.normalized().dot(guideDir), -1.0f, 1.0f));
        if (angle * dist < threshold) {
            return center + guideDir * dist;
        }
    }

    return pos;
}

MirrorGuide::MirrorGuide() : Guide(GuideType::Mirror) {}

void MirrorGuide::setAxisPoint(const Vec2& a, const Vec2& b)
{
    p1_ = a;
    p2_ = b;
}

Vec2 MirrorGuide::snap(const Vec2& pos, float threshold) const
{
    Vec2 mirrored = mirror(pos);
    float dist = Vec2::distance(pos, mirrored);
    if (dist < threshold * 2) {
        return mirrored;
    }
    return pos;
}

Vec2 MirrorGuide::mirror(const Vec2& pos) const
{
    Vec2 axisDir = p2_ - p1_;
    float len = axisDir.length();
    if (len < 1e-6f) return pos;
    axisDir = axisDir.normalized();

    Vec2 normal = { -axisDir.y, axisDir.x };
    float d = (pos - p1_).dot(normal);
    return pos - normal * (2.0f * d);
}

PerspectiveGuide::PerspectiveGuide()
    : Guide(GuideType::Perspective)
{
    vps_.resize(kMaxVPs, { 0.0f, 0.0f });
}

void PerspectiveGuide::setVanishingPoint(int index, const Vec2& vp)
{
    if (index >= 0 && index < kMaxVPs) {
        vps_[index] = vp;
    }
}

Vec2 PerspectiveGuide::vanishingPoint(int index) const
{
    if (index >= 0 && static_cast<size_t>(index) < vps_.size()) {
        return vps_[index];
    }
    return { 0.0f, 0.0f };
}

Vec2 PerspectiveGuide::snap(const Vec2& pos, float threshold) const
{
    Vec2 best = pos;
    float bestDist = threshold;

    for (const auto& vp : vps_) {
        if (vp.x == 0.0f && vp.y == 0.0f) continue;

        Vec2 dir = vp - pos;
        float dist = dir.length();
        if (dist < threshold) {
            return vp;
        }

        Vec2 toVP = (vp - pos).normalized();
        for (const auto& vp2 : vps_) {
            if (&vp2 == &vp || (vp2.x == 0.0f && vp2.y == 0.0f)) continue;
            Vec2 toVP2 = (vp2 - pos).normalized();
            float dot = std::abs(toVP.dot(toVP2));
            if (dot > 0.999f) {
                return pos;
            }
        }
    }

    return best;
}

RulerTool::RulerTool() = default;

void RulerTool::addGuide(std::unique_ptr<Guide> guide)
{
    guides_.push_back(std::move(guide));
}

void RulerTool::removeGuide(int index)
{
    if (index >= 0 && static_cast<size_t>(index) < guides_.size()) {
        guides_.erase(guides_.begin() + index);
    }
}

Guide* RulerTool::guideAt(int index)
{
    if (index >= 0 && static_cast<size_t>(index) < guides_.size()) {
        return guides_[index].get();
    }
    return nullptr;
}

const Guide* RulerTool::guideAt(int index) const
{
    if (index >= 0 && static_cast<size_t>(index) < guides_.size()) {
        return guides_[index].get();
    }
    return nullptr;
}

Vec2 RulerTool::snap(const Vec2& pos, float threshold) const
{
    if (!snap_enabled_) return pos;

    Vec2 best = pos;
    float bestDist = snap_threshold_;

    for (const auto& guide : guides_) {
        if (!guide->enabled()) continue;
        Vec2 snapped = guide->snap(pos, threshold);
        float dist = Vec2::distance(pos, snapped);
        if (dist < bestDist) {
            bestDist = dist;
            best = snapped;
        }
    }

    return best;
}

} // namespace fap
