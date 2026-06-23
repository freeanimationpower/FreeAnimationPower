#pragma once

#include <algorithm>

namespace fap {

enum class PlaybackMode {
    Forward,
    Reverse,
    PingPong
};

struct PlaybackState {
    int startFrame = 0;
    int endFrame = 0;
    PlaybackMode mode = PlaybackMode::Forward;
    bool active = false;
};

class PlaybackController {
public:
    void start(int startFrame, int endFrame, PlaybackMode mode)
    {
        state_.startFrame = startFrame;
        state_.endFrame = endFrame;
        state_.mode = mode;
        state_.active = true;
        pingpong_forward_ = (mode == PlaybackMode::Reverse) ? false : true;
    }

    void stop()
    {
        state_.active = false;
    }

    int getNextFrame(int currentFrame) const
    {
        if (!state_.active) {
            return currentFrame;
        }

        const int s = state_.startFrame;
        const int e = state_.endFrame;

        if (s == e) {
            return s;
        }

        const int lo = std::min(s, e);
        const int hi = std::max(s, e);

        switch (state_.mode) {
        case PlaybackMode::Forward: {
            if (currentFrame < hi) {
                return currentFrame + 1;
            }
            if (currentFrame >= hi) {
                return lo;
            }
            return currentFrame;
        }
        case PlaybackMode::Reverse: {
            if (currentFrame > lo) {
                return currentFrame - 1;
            }
            if (currentFrame <= lo) {
                return hi;
            }
            return currentFrame;
        }
        case PlaybackMode::PingPong: {
            if (pingpong_forward_) {
                if (currentFrame >= hi) {
                    pingpong_forward_ = false;
                    return hi - 1;
                }
                return currentFrame + 1;
            } else {
                if (currentFrame <= lo) {
                    pingpong_forward_ = true;
                    return lo + 1;
                }
                return currentFrame - 1;
            }
        }
        }

        return currentFrame;
    }

    bool isActive() const
    {
        return state_.active;
    }

private:
    PlaybackState state_;
    mutable bool pingpong_forward_ = true;
};

} // namespace fap
