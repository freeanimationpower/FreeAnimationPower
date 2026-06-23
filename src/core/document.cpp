#include "core/document.hpp"

namespace fap {

Document::Document() {
    rootLayerForFrame(0);
}

Document::Document(const std::string& filepath)
    : filepath_(filepath) {
    rootLayerForFrame(0);
}

void Document::setCanvasSize(int w, int h) {
    width_ = w;
    height_ = h;
    modified_ = true;
}

void Document::setFPS(int fps) {
    fps_ = fps;
    modified_ = true;
}

void Document::setTotalFrames(int count) {
    total_frames_ = count;
    modified_ = true;
}

GroupLayer& Document::rootLayerForFrame(int frame) {
    auto it = frames_.find(frame);
    if (it == frames_.end()) {
        auto root = std::make_unique<GroupLayer>("Root");
        ensureDefaultLayer(*root);
        auto [inserted, _] = frames_.emplace(frame, std::move(root));
        return *inserted->second;
    }
    return *it->second;
}

const GroupLayer& Document::rootLayerForFrame(int frame) const {
    auto it = frames_.find(frame);
    if (it != frames_.end()) return *it->second;
    static GroupLayer fallback("Empty");
    return fallback;
}

const GroupLayer* Document::peekRootLayerForFrame(int frame) const {
    auto it = frames_.find(frame);
    return (it != frames_.end()) ? it->second.get() : nullptr;
}

void Document::ensureDefaultLayer(GroupLayer& root) {
    if (root.layerCount() == 0) {
        root.addLayer(std::make_unique<RasterLayer>("Layer 1", width_, height_));
    }
}

void Document::shiftFrameData(int fromFrame, int delta) {
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
    modified_ = true;
}

void Document::removeFrameData(int frameIdx) {
    frames_.erase(frameIdx);
    modified_ = true;
}

} // namespace fap
