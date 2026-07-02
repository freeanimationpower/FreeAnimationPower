#pragma once

#include "core/sequence.hpp"
#include <cstdint>
#include <functional>
#include <algorithm>

namespace fap {

class TimelineEngine {
public:
    explicit TimelineEngine(Sequence& sequence)
        : sequence_(sequence)
    {}

    void setFPS(int fps) { sequence_.setFPS(fps); }
    int fps() const { return sequence_.fps(); }

    void setTotalFrames(int count) { sequence_.setTotalFrames(count); }
    int totalFrames() const { return sequence_.totalFrames(); }

    void play() { if (sequence_.totalFrames() > 1) sequence_.play(); }
    void pause() { sequence_.pause(); }
    void stop() { sequence_.stop(); }
    bool playing() const { return sequence_.playing(); }

    void setLooping(bool loop) { sequence_.setLooping(loop); }
    bool looping() const { return sequence_.looping(); }

    int currentFrame() const { return sequence_.currentFrame(); }

    void stepForward() { sequence_.nextFrame(); }
    void stepBackward() { sequence_.prevFrame(); }
    void goToFrame(int frame) { sequence_.setCurrentFrame(frame); }
    void goToStart() { sequence_.goToStart(); }
    void goToEnd() { sequence_.goToEnd(); }

    float currentTimeSeconds() const {
        return static_cast<float>(sequence_.currentFrame())
             / static_cast<float>(sequence_.fps());
    }

    void update(float deltaTime) {
        if (!sequence_.playing() || sequence_.totalFrames() <= 1) {
            return;
        }

        const float frameDuration = 1.0f / static_cast<float>(sequence_.fps());
        accumulated_ += deltaTime;

        while (accumulated_ >= frameDuration) {
            accumulated_ -= frameDuration;
            if (sequence_.currentFrame() < sequence_.totalFrames() - 1) {
                sequence_.nextFrame();
            } else if (sequence_.looping()) {
                sequence_.goToStart();
            } else {
                sequence_.pause();
                accumulated_ = 0.0f;
                break;
            }
        }
    }

private:
    Sequence& sequence_;
    float accumulated_ = 0.0f;
};

} // namespace fap
