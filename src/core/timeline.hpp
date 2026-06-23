#pragma once

#include "types.hpp"
#include <functional>
#include <vector>

namespace fap {

class Timeline {
public:
    Timeline();

    int currentFrame() const { return current_frame_; }
    int totalFrames() const { return total_frames_; }
    int fps() const { return fps_; }
    bool playing() const { return playing_; }
    bool looping() const { return looping_; }

    void setCurrentFrame(int frame);
    void setTotalFrames(int count);
    void setFPS(int fps);
    void setLooping(bool loop) { looping_ = loop; }

    void addKeyframe(int layerIndex, int frame);
    void removeKeyframe(int layerIndex, int frame);
    bool hasKeyframe(int layerIndex, int frame) const;

    void play();
    void pause();
    void stop();
    void nextFrame();
    void prevFrame();
    void goToStart();
    void goToEnd();

    using FrameCallback = std::function<void(int frame)>;
    void setOnFrameChanged(FrameCallback cb) { on_frame_changed_ = cb; }

private:
    int current_frame_ = 0;
    int total_frames_ = 1;
    int fps_ = 24;
    bool playing_ = false;
    bool looping_ = false;
    std::vector<std::vector<bool>> keyframes_; // [layer][frame]
    FrameCallback on_frame_changed_;
};

} // namespace fap
