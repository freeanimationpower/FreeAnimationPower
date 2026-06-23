#include "audio_engine.hpp"
#include <algorithm>
#include <cmath>

namespace fap {

AudioTrack::AudioTrack(const std::string& name)
    : name_(name)
{
}

void AudioTrack::addClip(const AudioClip& clip)
{
    clips_.push_back(clip);
}

void AudioTrack::removeClip(int index)
{
    if (index >= 0 && static_cast<size_t>(index) < clips_.size()) {
        clips_.erase(clips_.begin() + index);
    }
}

void AudioTrack::addKeyframe(const AudioKeyframe& kf)
{
    auto it = std::lower_bound(keyframes_.begin(), keyframes_.end(), kf.frame,
        [](const AudioKeyframe& a, int frame) { return a.frame < frame; });
    if (it != keyframes_.end() && it->frame == kf.frame) {
        *it = kf;
    } else {
        keyframes_.insert(it, kf);
    }
}

void AudioTrack::removeKeyframe(int index)
{
    if (index >= 0 && static_cast<size_t>(index) < keyframes_.size()) {
        keyframes_.erase(keyframes_.begin() + index);
    }
}

float AudioTrack::volumeAtFrame(int frame) const
{
    if (keyframes_.empty()) return volume_;
    if (frame <= keyframes_.front().frame) return keyframes_.front().volume;
    if (frame >= keyframes_.back().frame) return keyframes_.back().volume;

    for (size_t i = 0; i < keyframes_.size() - 1; ++i) {
        const auto& a = keyframes_[i];
        const auto& b = keyframes_[i + 1];
        if (frame >= a.frame && frame <= b.frame) {
            float t = (b.frame != a.frame)
                ? static_cast<float>(frame - a.frame) / (b.frame - a.frame)
                : 0.0f;
            return lerpf(a.volume, b.volume, t);
        }
    }
    return volume_;
}

AudioEngine::AudioEngine(int sampleRate)
    : sample_rate_(sampleRate)
{
}

void AudioEngine::addTrack(std::unique_ptr<AudioTrack> track)
{
    tracks_.push_back(std::move(track));
}

void AudioEngine::removeTrack(int index)
{
    if (index >= 0 && static_cast<size_t>(index) < tracks_.size()) {
        tracks_.erase(tracks_.begin() + index);
    }
}

AudioTrack* AudioEngine::trackAt(int index)
{
    if (index >= 0 && static_cast<size_t>(index) < tracks_.size()) {
        return tracks_[index].get();
    }
    return nullptr;
}

const AudioTrack* AudioEngine::trackAt(int index) const
{
    if (index >= 0 && static_cast<size_t>(index) < tracks_.size()) {
        return tracks_[index].get();
    }
    return nullptr;
}

float AudioEngine::durationSeconds() const
{
    float maxDuration = 0.0f;
    for (const auto& track : tracks_) {
        for (const auto& clip : track->clips()) {
            float end = clip.start_time + clip.duration;
            maxDuration = std::max(maxDuration, end);
        }
    }
    return maxDuration;
}

int AudioEngine::durationFrames() const
{
    float secs = durationSeconds();
    if (secs <= 0.0f) return 1;
    return static_cast<int>(std::ceil(secs * fps_));
}

float AudioEngine::timeForFrame(int frame) const
{
    if (fps_ <= 0) return 0.0f;
    return static_cast<float>(frame) / static_cast<float>(fps_);
}

int AudioEngine::frameForTime(float seconds) const
{
    return static_cast<int>(std::round(seconds * fps_));
}

void AudioEngine::seekToFrame(int frame)
{
    if (on_frame_) {
        on_frame_(frame, timeForFrame(frame));
    }
}

void AudioEngine::play()
{
    playing_ = true;
}

void AudioEngine::pause()
{
    playing_ = false;
}

void AudioEngine::stop()
{
    playing_ = false;
    seekToFrame(0);
}

} // namespace fap
