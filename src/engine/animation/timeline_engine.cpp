#pragma once

#include <cstdint>
#include <functional>
#include <algorithm>

namespace fap {

class TimelineEngine {
public:
    TimelineEngine(int fps = 24)
        : fps_(std::max(1, fps))
    {
    }

    void setFPS(int fps)
    {
        fps_ = std::max(1, fps);
    }

    int fps() const
    {
        return fps_;
    }

    void setTotalFrames(int count)
    {
        total_frames_ = std::max(1, count);
        if (current_frame_ >= total_frames_) {
            current_frame_ = total_frames_ - 1;
        }
    }

    int totalFrames() const
    {
        return total_frames_;
    }

    void play()
    {
        if (total_frames_ <= 1) {
            return;
        }
        playing_ = true;
    }

    void pause()
    {
        playing_ = false;
    }

    void stop()
    {
        playing_ = false;
        current_frame_ = 0;
        accumulated_ = 0.0f;
        if (on_frame_changed_) {
            on_frame_changed_(current_frame_);
        }
    }

    bool playing() const
    {
        return playing_;
    }

    bool looping() const
    {
        return looping_;
    }

    void setLooping(bool loop)
    {
        looping_ = loop;
    }

    int currentFrame() const
    {
        return current_frame_;
    }

    void stepForward()
    {
        if (playing_) {
            return;
        }
        if (current_frame_ < total_frames_ - 1) {
            ++current_frame_;
        } else if (looping_) {
            current_frame_ = 0;
        }
        if (on_frame_changed_) {
            on_frame_changed_(current_frame_);
        }
    }

    void stepBackward()
    {
        if (playing_) {
            return;
        }
        if (current_frame_ > 0) {
            --current_frame_;
        } else if (looping_) {
            current_frame_ = total_frames_ - 1;
        }
        if (on_frame_changed_) {
            on_frame_changed_(current_frame_);
        }
    }

    void goToFrame(int frame)
    {
        if (frame < 0) {
            current_frame_ = 0;
        } else if (frame >= total_frames_) {
            current_frame_ = total_frames_ - 1;
        } else {
            current_frame_ = frame;
        }
        if (on_frame_changed_) {
            on_frame_changed_(current_frame_);
        }
    }

    void goToStart()
    {
        goToFrame(0);
    }

    void goToEnd()
    {
        goToFrame(total_frames_ - 1);
    }

    float currentTimeSeconds() const
    {
        return static_cast<float>(current_frame_) / static_cast<float>(fps_);
    }

    void update(float deltaTime)
    {
        if (!playing_ || total_frames_ <= 1) {
            return;
        }

        const float frameDuration = 1.0f / static_cast<float>(fps_);
        accumulated_ += deltaTime;

        bool changed = false;

        while (accumulated_ >= frameDuration) {
            accumulated_ -= frameDuration;
            if (current_frame_ < total_frames_ - 1) {
                ++current_frame_;
                changed = true;
            } else if (looping_) {
                current_frame_ = 0;
                changed = true;
            } else {
                current_frame_ = total_frames_ - 1;
                playing_ = false;
                accumulated_ = 0.0f;
                changed = true;
                break;
            }
        }

        if (changed && on_frame_changed_) {
            on_frame_changed_(current_frame_);
        }
    }

    void setOnFrameChanged(std::function<void(int)> callback)
    {
        on_frame_changed_ = std::move(callback);
    }

private:
    int fps_ = 24;
    int total_frames_ = 1;
    int current_frame_ = 0;
    bool playing_ = false;
    bool looping_ = false;
    float accumulated_ = 0.0f;
    std::function<void(int)> on_frame_changed_;
};

} // namespace fap
