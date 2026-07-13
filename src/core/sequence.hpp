#pragma once

#include "types.hpp"
#include "layer.hpp"
#include "undo_manager.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fap {

class Sequence : public NonCopyable {
public:
    using FrameCallback = std::function<void(int frame)>;
    using SequenceUid = uint64_t;

    Sequence(std::string name, int canvasWidth, int canvasHeight,
             int fps = 24, int totalFrames = 24);

    SequenceUid uid() const { return uid_; }
    const std::string& name() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    bool visible() const { return visible_; }
    void setVisible(bool v) { visible_ = v; }
    float opacity() const { return opacity_; }
    void setOpacity(float o) { opacity_ = std::clamp(o, 0.0f, 1.0f); }
    bool locked() const { return locked_; }
    void setLocked(bool l) { locked_ = l; }

    int workAreaStart() const { return workAreaStart_; }
    void setWorkAreaStart(int frame) { workAreaStart_ = std::max(0, frame); }
    int workAreaEnd() const { return workAreaEnd_; }
    void setWorkAreaEnd(int frame) { workAreaEnd_ = frame; }
    int effectiveWorkAreaEnd() const;
    int durationFrames() const { return durationFrames_; }
    void setDurationFrames(int count) { durationFrames_ = std::max(1, count); }

    int currentFrame() const { return current_frame_; }
    int totalFrames() const { return total_frames_; }
    int fps() const { return fps_; }
    bool playing() const { return playing_; }
    bool looping() const { return looping_; }

    void setCurrentFrame(int frame);
    void setTotalFrames(int count);
    void setFPS(int fps) { fps_ = fps; }
    void setLooping(bool loop) { looping_ = loop; }

    void play()  { playing_ = true; }
    void pause() { playing_ = false; }
    void stop();

    void nextFrame();
    void prevFrame();
    int advanceFrame();
    void goToStart() { setCurrentFrame(0); }
    void goToEnd() { setCurrentFrame(total_frames_ - 1); }

    void addKeyframe(int layerIndex, int frame);
    void removeKeyframe(int layerIndex, int frame);
    bool hasKeyframe(int layerIndex, int frame) const;

    GroupLayer& rootLayerForFrame(int frame);
    const GroupLayer& rootLayerForFrame(int frame) const;
    const GroupLayer* peekRootLayerForFrame(int frame) const;
    void shiftFrameData(int fromFrame, int delta);
    void removeFrameData(int frameIdx);
    void copyLayerToAllFrames(int sourceFrame, int layerIndex);

    UndoManager& undoManager() { return undo_manager_; }
    const UndoManager& undoManager() const { return undo_manager_; }

    void setOnFrameChanged(FrameCallback cb) { on_frame_changed_ = std::move(cb); }

    std::unique_ptr<Sequence> clone(const std::string& newName = "") const;

private:
    SequenceUid uid_;
    std::string name_;
    bool visible_ = true;
    float opacity_ = 1.0f;
    bool locked_ = false;

    int current_frame_ = 0;
    int total_frames_ = 1;
    int fps_ = 24;
    bool playing_ = false;
    bool looping_ = false;

    int workAreaStart_ = 0;
    int workAreaEnd_ = 0;
    int durationFrames_ = 1;

    std::vector<std::vector<bool>> keyframes_;
    std::map<int, std::unique_ptr<GroupLayer>> frames_;

    UndoManager undo_manager_;
    FrameCallback on_frame_changed_;

    int canvas_width_;
    int canvas_height_;

    void ensureDefaultLayer(GroupLayer& root);
    static uint64_t generateSequenceUid();
};

} // namespace fap
