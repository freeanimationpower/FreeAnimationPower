#pragma once

#include "types.hpp"
#include "layer.hpp"
#include "undo_manager.hpp"
#include <memory>
#include <string>
#include <map>

namespace fap {

static constexpr size_t kDefaultLayerCapacity = 128;

class Document : public NonCopyable {
public:
    Document();
    explicit Document(const std::string& filepath);

    int width() const { return width_; }
    int height() const { return height_; }
    int fps() const { return fps_; }
    int totalFrames() const { return total_frames_; }

    void setCanvasSize(int w, int h);
    void setFPS(int fps);
    void setTotalFrames(int count);

    GroupLayer& rootLayerForFrame(int frame);
    const GroupLayer& rootLayerForFrame(int frame) const;
    const GroupLayer* peekRootLayerForFrame(int frame) const;

    GroupLayer& rootLayer() { return rootLayerForFrame(0); }
    const GroupLayer& rootLayer() const { return rootLayerForFrame(0); }

    bool modified() const { return modified_; }
    void setModified(bool m) { modified_ = m; }

    const std::string& filepath() const { return filepath_; }

    UndoManager& undoManager() { return undo_manager_; }
    const UndoManager& undoManager() const { return undo_manager_; }

    void shiftFrameData(int fromFrame, int delta);
    void removeFrameData(int frameIdx);

private:
    std::string filepath_;
    int width_ = 1920;
    int height_ = 1080;
    int fps_ = 24;
    int total_frames_ = 24;
    bool modified_ = false;
    std::map<int, std::unique_ptr<GroupLayer>> frames_;
    UndoManager undo_manager_;
    void ensureDefaultLayer(GroupLayer& root);
};

} // namespace fap
