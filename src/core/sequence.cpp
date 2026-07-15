#include "sequence.hpp"
#include "diagnostic/tracer_macros.hpp"
#include <atomic>
#include <algorithm>
#include <map>
#include <utility>

namespace fap {

static std::atomic<Sequence::SequenceUid> g_nextSequenceUid{1};

Sequence::SequenceUid Sequence::generateSequenceUid() {
    return g_nextSequenceUid.fetch_add(1, std::memory_order_relaxed);
}

Sequence::Sequence(std::string name, int canvasWidth, int canvasHeight,
                   int fps, int totalFrames)
    : uid_(generateSequenceUid())
    , name_(std::move(name))
    , total_frames_(std::max(1, totalFrames))
    , fps_(std::max(1, fps))
    , durationFrames_(std::max(1, totalFrames))
    , canvas_width_(canvasWidth)
    , canvas_height_(canvasHeight)
{
    rootLayerForFrame(0);
}

void Sequence::setCurrentFrame(int frame) {
    frame = std::clamp(frame, 0, durationFrames_ - 1);
    if (current_frame_ != frame) {
        current_frame_ = frame;
        if (on_frame_changed_) {
            on_frame_changed_(current_frame_);
        }
    }
}

void Sequence::setTotalFrames(int count) {
    total_frames_ = std::max(1, count);
    if (current_frame_ >= total_frames_) {
        setCurrentFrame(total_frames_ - 1);
    }
}

void Sequence::stop() {
    playing_ = false;
    setCurrentFrame(0);
}

void Sequence::nextFrame() {
    if (current_frame_ < durationFrames_ - 1) {
        setCurrentFrame(current_frame_ + 1);
    } else if (looping_) {
        setCurrentFrame(0);
    }
}

void Sequence::prevFrame() {
    if (current_frame_ > 0) {
        setCurrentFrame(current_frame_ - 1);
    } else if (looping_) {
        setCurrentFrame(durationFrames_ - 1);
    }
}

int Sequence::effectiveWorkAreaEnd() const {
    if (workAreaEnd_ > 0) {
        return std::min(workAreaEnd_, durationFrames_);
    }
    return durationFrames_;
}

int Sequence::advanceFrame() {
    int waStart = std::clamp(workAreaStart_, 0, durationFrames_ - 1);
    int waEnd = effectiveWorkAreaEnd();

    if (waEnd <= waStart) {
        waEnd = durationFrames_;
    }

    if (current_frame_ >= waEnd - 1) {
        setCurrentFrame(waStart);
    } else {
        setCurrentFrame(current_frame_ + 1);
    }

    return current_frame_;
}

void Sequence::addKeyframe(int layerIndex, int frame) {
    while (keyframes_.size() <= static_cast<size_t>(layerIndex)) {
        keyframes_.emplace_back(total_frames_, false);
    }
    if (frame >= 0 && frame < total_frames_) {
        keyframes_[layerIndex][frame] = true;
    }
}

void Sequence::removeKeyframe(int layerIndex, int frame) {
    if (layerIndex >= 0 && static_cast<size_t>(layerIndex) < keyframes_.size() &&
        frame >= 0 && static_cast<size_t>(frame) < keyframes_[layerIndex].size()) {
        keyframes_[layerIndex][frame] = false;
    }
}

bool Sequence::hasKeyframe(int layerIndex, int frame) const {
    if (layerIndex >= 0 && static_cast<size_t>(layerIndex) < keyframes_.size() &&
        frame >= 0 && static_cast<size_t>(frame) < keyframes_[layerIndex].size()) {
        return keyframes_[layerIndex][frame];
    }
    return false;
}

void Sequence::ensureDefaultLayer(GroupLayer& root) {
    if (root.layerCount() == 0) {
        root.addLayer(std::make_unique<RasterLayer>("Layer 1", canvas_width_, canvas_height_));
    }
}

GroupLayer& Sequence::rootLayerForFrame(int frame) {
    auto it = frames_.find(frame);
    if (it == frames_.end()) {
        auto root = std::make_unique<GroupLayer>("Root");
        ensureDefaultLayer(*root);
        auto [inserted, _] = frames_.emplace(frame, std::move(root));
        return *inserted->second;
    }
    return *it->second;
}

const GroupLayer& Sequence::rootLayerForFrame(int frame) const {
    auto it = frames_.find(frame);
    if (it != frames_.end()) return *it->second;
    static GroupLayer emptyRoot("EmptyRoot");
    return emptyRoot;
}

const GroupLayer* Sequence::peekRootLayerForFrame(int frame) const {
    auto it = frames_.find(frame);
    return (it != frames_.end()) ? it->second.get() : nullptr;
}

void Sequence::shiftFrameData(int fromFrame, int delta) {
    std::map<int, std::unique_ptr<GroupLayer>> shifted;
    for (auto& [frame, layerPtr] : frames_) {
        if (frame >= fromFrame) {
            int newIdx = frame + delta;
            if (newIdx >= 0) shifted.emplace(newIdx, std::move(layerPtr));
        } else {
            shifted.emplace(frame, std::move(layerPtr));
        }
    }
    frames_ = std::move(shifted);
}

void Sequence::removeFrameData(int frameIdx) {
    frames_.erase(frameIdx);
}

std::unique_ptr<Sequence> Sequence::clone(const std::string& newName) const {
    auto seq = std::make_unique<Sequence>(
        newName.empty() ? name_ + " copy" : newName,
        canvas_width_, canvas_height_, fps_, total_frames_);

    seq->current_frame_ = current_frame_;
    seq->playing_ = false;
    seq->looping_ = looping_;
    seq->visible_ = visible_;
    seq->opacity_ = opacity_;
    seq->locked_ = locked_;
    seq->workAreaStart_ = workAreaStart_;
    seq->workAreaEnd_ = workAreaEnd_;
    seq->durationFrames_ = durationFrames_;
    seq->markers_ = markers_;

    for (const auto& [frameIdx, rootLayer] : frames_) {
        auto newRoot = std::make_unique<GroupLayer>(rootLayer->name());
        newRoot->setVisible(rootLayer->visible());
        newRoot->setOpacity(rootLayer->opacity());
        newRoot->setBlendMode(rootLayer->blendMode());
        newRoot->setLocked(rootLayer->locked());

        for (size_t li = 0; li < rootLayer->layerCount(); ++li) {
            const Layer* src = rootLayer->layerAt(static_cast<int>(li));
            if (!src) continue;

            if (src->type() == LayerType::Raster) {
                const auto* srcRl = static_cast<const RasterLayer*>(src);
                auto copyRl = std::make_unique<RasterLayer>(
                    srcRl->name(), srcRl->width(), srcRl->height());
                copyRl->shareDataFrom(*srcRl);
                copyRl->setHasContent(srcRl->hasContent());
                copyRl->setVisible(srcRl->visible());
                copyRl->setOpacity(srcRl->opacity());
                copyRl->setBlendMode(srcRl->blendMode());
                copyRl->setLocked(srcRl->locked());
                newRoot->addLayer(std::move(copyRl));
            } else {
                newRoot->addLayer(src->clone());
            }
        }

        seq->frames_[frameIdx] = std::move(newRoot);
    }

    if (seq->frames_.empty()) {
        auto root = std::make_unique<GroupLayer>("Root");
        root->addLayer(std::make_unique<RasterLayer>("Layer 1", canvas_width_, canvas_height_));
        seq->frames_[0] = std::move(root);
    }

    seq->keyframes_ = keyframes_;
    return seq;
}

void Sequence::copyLayerToAllFrames(int sourceFrame, int layerIndex) {
    GroupLayer& srcRoot = rootLayerForFrame(sourceFrame);
    if (layerIndex < 0 || layerIndex >= static_cast<int>(srcRoot.layerCount()))
        return;

    const Layer* srcLayer = srcRoot.layerAt(layerIndex);
    if (!srcLayer) return;

    for (int f = 0; f < total_frames_; ++f) {
        if (f == sourceFrame) continue;

        GroupLayer& dstRoot = rootLayerForFrame(f);

        // Ensure target frame has enough layers
        while (static_cast<int>(dstRoot.layerCount()) <= layerIndex) {
            dstRoot.addLayer(srcLayer->clone());
        }

        Layer* dstLayer = dstRoot.layerAt(layerIndex);
        if (!dstLayer || dstLayer->type() != srcLayer->type()) continue;

        dstLayer->setName(srcLayer->name());
        dstLayer->setVisible(srcLayer->visible());
        dstLayer->setOpacity(srcLayer->opacity());
        dstLayer->setBlendMode(srcLayer->blendMode());
        dstLayer->setLocked(srcLayer->locked());

        if (srcLayer->type() == LayerType::Raster) {
            const auto* srcRl = static_cast<const RasterLayer*>(srcLayer);
            auto* dstRl = static_cast<RasterLayer*>(dstLayer);
            dstRl->shareDataFrom(*srcRl);
            dstRl->ensureUnique();
            dstRl->setHasContent(srcRl->hasContent());
        }
    }
}

void Sequence::addMarker(const Marker& marker) {
    Marker m = marker;
    if (m.uid == 0) m.uid = Marker::nextUid();
    if (m.comment.empty()) m.comment = std::to_string(markers_.size() + 1);

    for (auto& existing : markers_) {
        if (existing.frame == m.frame) {
            existing = m;
            return;
        }
    }
    markers_.push_back(m);

    std::sort(markers_.begin(), markers_.end(),
              [](const Marker& a, const Marker& b) { return a.frame < b.frame; });
}

void Sequence::removeMarker(int index) {
    if (index >= 0 && index < static_cast<int>(markers_.size())) {
        markers_.erase(markers_.begin() + index);
    }
}

void Sequence::removeMarkerByUid(int64_t uid) {
    markers_.erase(
        std::remove_if(markers_.begin(), markers_.end(),
                       [uid](const Marker& m) { return m.uid == uid; }),
        markers_.end());
}

void Sequence::updateMarker(int index, const Marker& marker) {
    if (index < 0 || index >= static_cast<int>(markers_.size())) return;

    Marker m = marker;
    if (m.comment.empty()) m.comment = std::to_string(index + 1);

    for (size_t i = 0; i < markers_.size(); ++i) {
        if (static_cast<int>(i) != index && markers_[i].frame == m.frame) {
            markers_.erase(markers_.begin() + index);
            std::sort(markers_.begin(), markers_.end(),
                      [](const Marker& a, const Marker& b) { return a.frame < b.frame; });
            return;
        }
    }

    markers_[index] = m;
    std::sort(markers_.begin(), markers_.end(),
              [](const Marker& a, const Marker& b) { return a.frame < b.frame; });
}

Marker* Sequence::markerAtFrame(int frame) {
    for (auto& m : markers_) {
        if (m.frame == frame) return &m;
    }
    return nullptr;
}

const Marker* Sequence::markerAtFrame(int frame) const {
    for (auto& m : markers_) {
        if (m.frame == frame) return &m;
    }
    return nullptr;
}

int Sequence::markerIndexAtFrame(int frame) const {
    for (size_t i = 0; i < markers_.size(); ++i) {
        if (markers_[i].frame == frame) return static_cast<int>(i);
    }
    return -1;
}

std::vector<const Marker*> Sequence::sortedMarkers() const {
    std::vector<const Marker*> result;
    result.reserve(markers_.size());
    for (auto& m : markers_) result.push_back(&m);
    return result;
}

} // namespace fap
