#include "core/timeline.hpp"
#include <algorithm>

namespace fap {

Timeline::Timeline() : keyframes_(1, std::vector<bool>(total_frames_, false)) {}

void Timeline::setCurrentFrame(int frame) {
    frame = std::max(0, std::min(frame, total_frames_ - 1));
    if (frame != current_frame_) {
        current_frame_ = frame;
        if (on_frame_changed_) {
            on_frame_changed_(current_frame_);
        }
    }
}

void Timeline::setTotalFrames(int count) {
    total_frames_ = std::max(1, count);
    current_frame_ = std::min(current_frame_, total_frames_ - 1);
    for (auto& layer : keyframes_) {
        layer.resize(total_frames_, false);
    }
}

void Timeline::setFPS(int fps) {
    fps_ = std::max(1, fps);
}

void Timeline::addKeyframe(int layerIndex, int frame) {
    while (keyframes_.size() <= static_cast<size_t>(layerIndex)) {
        keyframes_.emplace_back(total_frames_, false);
    }
    if (frame >= 0 && frame < total_frames_) {
        keyframes_[layerIndex][frame] = true;
    }
}

void Timeline::removeKeyframe(int layerIndex, int frame) {
    if (layerIndex >= 0 && static_cast<size_t>(layerIndex) < keyframes_.size() &&
        frame >= 0 && static_cast<size_t>(frame) < keyframes_[layerIndex].size()) {
        keyframes_[layerIndex][frame] = false;
    }
}

bool Timeline::hasKeyframe(int layerIndex, int frame) const {
    if (layerIndex >= 0 && static_cast<size_t>(layerIndex) < keyframes_.size() &&
        frame >= 0 && static_cast<size_t>(frame) < keyframes_[layerIndex].size()) {
        return keyframes_[layerIndex][frame];
    }
    return false;
}

void Timeline::play() { playing_ = true; }
void Timeline::pause() { playing_ = false; }
void Timeline::stop() { playing_ = false; current_frame_ = 0; }

void Timeline::nextFrame() {
    if (current_frame_ < total_frames_ - 1) {
        setCurrentFrame(current_frame_ + 1);
    } else if (looping_) {
        setCurrentFrame(0);
    }
}

void Timeline::prevFrame() {
    if (current_frame_ > 0) {
        setCurrentFrame(current_frame_ - 1);
    } else if (looping_) {
        setCurrentFrame(total_frames_ - 1);
    }
}

void Timeline::goToStart() { setCurrentFrame(0); }
void Timeline::goToEnd() { setCurrentFrame(total_frames_ - 1); }

} // namespace fap
