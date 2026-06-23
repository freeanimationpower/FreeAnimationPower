#pragma once

#include <QObject>
#include <memory>
#include <string>

namespace fap {

class Document;
class Timeline;
class ToolState;
class Layer;
class GroupLayer;
class AudioEngine;
class RulerTool;
class TweenEngine;
class DeformationEngine;
class FrameThumbnailCache;
class FrameViewerData;
class PencilRetouchEngine;
enum class LayerType;
using LayerUid = uint64_t;

class AppState : public QObject {
    Q_OBJECT

public:
    explicit AppState(QObject* parent = nullptr);
    ~AppState() override;

    Document& document() { return *document_; }
    const Document& document() const { return *document_; }

    Timeline& timeline() { return *timeline_; }
    const Timeline& timeline() const { return *timeline_; }

    ToolState& toolState() { return *tool_state_; }
    const ToolState& toolState() const { return *tool_state_; }

    AudioEngine& audioEngine() { return *audio_engine_; }
    const AudioEngine& audioEngine() const { return *audio_engine_; }

    RulerTool& rulerTool() { return *ruler_tool_; }
    const RulerTool& rulerTool() const { return *ruler_tool_; }

    TweenEngine& tweenEngine() { return *tween_engine_; }
    const TweenEngine& tweenEngine() const { return *tween_engine_; }

    DeformationEngine& deformationEngine() { return *deformation_engine_; }
    const DeformationEngine& deformationEngine() const { return *deformation_engine_; }

    FrameThumbnailCache& thumbnailCache() { return *thumbnail_cache_; }
    const FrameThumbnailCache& thumbnailCache() const { return *thumbnail_cache_; }

    FrameViewerData& frameViewer() { return *frame_viewer_; }
    const FrameViewerData& frameViewer() const { return *frame_viewer_; }

    PencilRetouchEngine& pencilRetouch() { return *pencil_retouch_; }
    const PencilRetouchEngine& pencilRetouch() const { return *pencil_retouch_; }

    int currentFrame() const;
    void setCurrentFrame(int frame);

    int activeLayerIndex() const { return active_layer_index_; }
    void setActiveLayerIndex(int index);
    LayerUid activeLayerUid() const { return active_layer_uid_; }

    GroupLayer* activeRootLayer();
    const GroupLayer* activeRootLayer() const;
    Layer* activeLayer();
    const Layer* activeLayer() const;

    bool isModified() const;
    const std::string& filepath() const;

    void resetDocument(int width = 1920, int height = 1080, int fps = 24, int totalFrames = 24);

signals:
    void activeLayerIndexChanged(int layerIndex);
    void activeLayerUidChanged(LayerUid uid);
    void currentFrameChanged(int frame);
    void documentChanged();
    void documentModifiedChanged(bool modified);
    void canvasSizeChanged(int width, int height);

private slots:
    void onTimelineFrameChanged(int frame);

private:
    std::unique_ptr<Document> document_;
    std::unique_ptr<Timeline> timeline_;
    std::unique_ptr<ToolState> tool_state_;
    std::unique_ptr<AudioEngine> audio_engine_;
    std::unique_ptr<RulerTool> ruler_tool_;
    std::unique_ptr<TweenEngine> tween_engine_;
    std::unique_ptr<DeformationEngine> deformation_engine_;
    std::unique_ptr<FrameThumbnailCache> thumbnail_cache_;
    std::unique_ptr<FrameViewerData> frame_viewer_;
    std::unique_ptr<PencilRetouchEngine> pencil_retouch_;

    int active_layer_index_ = 0;
    LayerUid active_layer_uid_ = 0;

    void wireSignals();
    void refreshActiveLayerUid();
};

} // namespace fap
