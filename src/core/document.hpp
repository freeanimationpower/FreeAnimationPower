#pragma once

#include "types.hpp"
#include "layer.hpp"
#include "sequence.hpp"
#include <memory>
#include <string>
#include <vector>

namespace fap {

struct AudioTrackData {
    std::string filepath;      // original path or extracted temp path
    std::string displayName;   // e.g., "song.mp3"
    std::string zipEntry;      // "audio/track_0.mp3" inside the ZIP
    bool muted = false;
    int volume = 80;           // 0-100
};

struct VideoTrackData {
    std::string filepath;
    std::string displayName;
    std::string zipEntry;
    int width = 0;
    int height = 0;
    int fps = 24;
    int totalFrames = 0;
    bool muted = false;
    int volume = 80;
    float opacity = 1.0f;
};

class Document : public NonCopyable {
public:
    Document();
    explicit Document(const std::string& filepath);

    int width() const { return width_; }
    int height() const { return height_; }
    void setCanvasSize(int w, int h);

    size_t sequenceCount() const { return sequences_.size(); }
    Sequence& sequenceAt(size_t index) { return *sequences_.at(index); }
    const Sequence& sequenceAt(size_t index) const { return *sequences_.at(index); }

    Sequence& activeSequence() { return *sequences_.at(active_sequence_); }
    const Sequence& activeSequence() const { return *sequences_.at(active_sequence_); }
    size_t activeSequenceIndex() const { return active_sequence_; }

    void setActiveSequence(size_t index);
    Sequence& addSequence(const std::string& name = "");
    void removeSequence(size_t index);
    void duplicateSequence(size_t index);
    void renameSequence(size_t index, const std::string& name);
    void moveSequence(size_t from, size_t to);

    int fps() const { return activeSequence().fps(); }
    int totalFrames() const { return activeSequence().totalFrames(); }
    int currentFrame() const { return activeSequence().currentFrame(); }

    void setFPS(int fps) { activeSequence().setFPS(fps); modified_ = true; }
    void setTotalFrames(int count) { activeSequence().setTotalFrames(count); modified_ = true; }
    void setCurrentFrame(int frame) { activeSequence().setCurrentFrame(frame); }

    GroupLayer& rootLayerForFrame(int frame) { return activeSequence().rootLayerForFrame(frame); }
    const GroupLayer& rootLayerForFrame(int frame) const { return activeSequence().rootLayerForFrame(frame); }
    const GroupLayer* peekRootLayerForFrame(int frame) const { return activeSequence().peekRootLayerForFrame(frame); }

    GroupLayer& rootLayer() { return rootLayerForFrame(0); }
    const GroupLayer& rootLayer() const { return rootLayerForFrame(0); }

    UndoManager& undoManager() { return activeSequence().undoManager(); }
    const UndoManager& undoManager() const { return activeSequence().undoManager(); }

    void shiftFrameData(int fromFrame, int delta) { activeSequence().shiftFrameData(fromFrame, delta); }
    void removeFrameData(int frameIdx) { activeSequence().removeFrameData(frameIdx); }

    bool modified() const { return modified_; }
    void setModified(bool m) { modified_ = m; }

    const std::string& filepath() const { return filepath_; }
    void setFilepath(const std::string& fp) { filepath_ = fp; }

    // Audio tracks
    std::vector<AudioTrackData>& audioTracks() { return audioTracks_; }
    const std::vector<AudioTrackData>& audioTracks() const { return audioTracks_; }
    void addAudioTrack(const AudioTrackData& track) { audioTracks_.push_back(track); }
    void clearAudioTracks() { audioTracks_.clear(); }

    // Video tracks
    std::vector<VideoTrackData>& videoTracks() { return videoTracks_; }
    const std::vector<VideoTrackData>& videoTracks() const { return videoTracks_; }
    void addVideoTrack(const VideoTrackData& track) { videoTracks_.push_back(track); }
    void clearVideoTracks() { videoTracks_.clear(); }

private:
    std::string filepath_;
    int width_ = 1920;
    int height_ = 1080;
    bool modified_ = false;

    std::vector<std::unique_ptr<Sequence>> sequences_;
    size_t active_sequence_ = 0;
    std::vector<AudioTrackData> audioTracks_;
    std::vector<VideoTrackData> videoTracks_;
};

} // namespace fap
