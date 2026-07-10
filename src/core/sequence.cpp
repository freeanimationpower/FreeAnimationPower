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
    frame = std::clamp(frame, 0, total_frames_ - 1);
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
    if (current_frame_ < total_frames_ - 1) {
        setCurrentFrame(current_frame_ + 1);
    } else if (looping_) {
        setCurrentFrame(0);
    }
}

void Sequence::prevFrame() {
    if (current_frame_ > 0) {
        setCurrentFrame(current_frame_ - 1);
    } else if (looping_) {
        setCurrentFrame(total_frames_ - 1);
    }
}

int Sequence::effectiveWorkAreaEnd() const {
    if (workAreaEnd_ > 0) {
        return std::min(workAreaEnd_, total_frames_);
    }
    return total_frames_;
}

int Sequence::advanceFrame() {
    int waStart = std::clamp(workAreaStart_, 0, total_frames_ - 1);
    int waEnd = effectiveWorkAreaEnd();

    if (waEnd <= waStart) {
        waEnd = total_frames_;
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

} // namespace fap
