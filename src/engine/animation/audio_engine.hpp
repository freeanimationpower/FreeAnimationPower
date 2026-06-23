#pragma once

#include "core/types.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace fap {

struct AudioClip {
    std::string filepath;
    float start_time = 0.0f;
    float duration = 0.0f;
    float start_frame = 0.0f;
    bool muted = false;
    float volume = 1.0f;
    float pan = 0.0f;
};

struct AudioKeyframe {
    int frame = 0;
    float volume = 1.0f;
    float pan = 0.0f;
};

class AudioTrack {
public:
    AudioTrack(const std::string& name = "Audio");

    const std::string& name() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    bool muted() const { return muted_; }
    void setMuted(bool m) { muted_ = m; }
    bool solo() const { return solo_; }
    void setSolo(bool s) { solo_ = s; }
    float volume() const { return volume_; }
    void setVolume(float v) { volume_ = std::clamp(v, 0.0f, 2.0f); }

    void addClip(const AudioClip& clip);
    void removeClip(int index);
    const std::vector<AudioClip>& clips() const { return clips_; }
    size_t clipCount() const { return clips_.size(); }

    void addKeyframe(const AudioKeyframe& kf);
    void removeKeyframe(int index);
    const std::vector<AudioKeyframe>& keyframes() const { return keyframes_; }

    float volumeAtFrame(int frame) const;

private:
    std::string name_;
    bool muted_ = false;
    bool solo_ = false;
    float volume_ = 1.0f;
    std::vector<AudioClip> clips_;
    std::vector<AudioKeyframe> keyframes_;
};

class AudioEngine {
public:
    AudioEngine(int sampleRate = 44100);

    int sampleRate() const { return sample_rate_; }
    int fps() const { return fps_; }
    void setFPS(int fps) { fps_ = fps; }

    void addTrack(std::unique_ptr<AudioTrack> track);
    void removeTrack(int index);
    AudioTrack* trackAt(int index);
    const AudioTrack* trackAt(int index) const;
    const std::vector<std::unique_ptr<AudioTrack>>& tracks() const { return tracks_; }
    size_t trackCount() const { return tracks_.size(); }

    float durationSeconds() const;
    int durationFrames() const;

    float timeForFrame(int frame) const;
    int frameForTime(float seconds) const;

    using AudioCallback = std::function<void(int frame, float timeSeconds)>;
    void setFrameCallback(AudioCallback cb) { on_frame_ = cb; }

    void seekToFrame(int frame);
    void play();
    void pause();
    void stop();
    bool isPlaying() const { return playing_; }

    float masterVolume() const { return master_volume_; }
    void setMasterVolume(float v) { master_volume_ = std::clamp(v, 0.0f, 1.0f); }

private:
    int sample_rate_;
    int fps_ = 24;
    bool playing_ = false;
    float master_volume_ = 1.0f;
    std::vector<std::unique_ptr<AudioTrack>> tracks_;
    AudioCallback on_frame_;
};

} // namespace fap
