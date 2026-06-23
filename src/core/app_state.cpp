#include "core/app_state.hpp"
#include "core/document.hpp"
#include "core/timeline.hpp"
#include "core/tool_state.hpp"
#include "core/layer.hpp"
#include "core/types.hpp"
#include "engine/animation/audio_engine.hpp"
#include "engine/animation/tween_engine.hpp"
#include "engine/animation/frame_thumbnail.hpp"
#include "engine/deformation/deformation_engine.hpp"
#include "engine/brush/ruler_guide.hpp"
#include "engine/brush/pencil_retouch.hpp"

namespace fap {

AppState::AppState(QObject* parent)
    : QObject(parent)
    , document_(std::make_unique<Document>())
    , timeline_(std::make_unique<Timeline>())
    , tool_state_(std::make_unique<ToolState>())
    , audio_engine_(std::make_unique<AudioEngine>())
    , ruler_tool_(std::make_unique<RulerTool>())
    , tween_engine_(std::make_unique<TweenEngine>())
    , deformation_engine_(std::make_unique<DeformationEngine>())
    , thumbnail_cache_(std::make_unique<FrameThumbnailCache>())
    , frame_viewer_(std::make_unique<FrameViewerData>())
    , pencil_retouch_(std::make_unique<PencilRetouchEngine>()) {
    timeline_->setTotalFrames(document_->totalFrames());
    timeline_->setFPS(document_->fps());
    wireSignals();
    thumbnail_cache_->setDocument(document_.get());
    frame_viewer_->setDocument(document_.get());
}

AppState::~AppState() = default;

void AppState::wireSignals() {
    timeline_->setOnFrameChanged([this](int frame) {
        emit currentFrameChanged(frame);
    });

    QObject::connect(tool_state_.get(), &ToolState::toolSettingsChanged,
                     this, [this]() { emit documentChanged(); });
}

void AppState::onTimelineFrameChanged(int frame) {
    emit currentFrameChanged(frame);
}

int AppState::currentFrame() const {
    return timeline_->currentFrame();
}

void AppState::setCurrentFrame(int frame) {
    timeline_->setCurrentFrame(frame);
    refreshActiveLayerUid();
}

void AppState::setActiveLayerIndex(int index) {
    if (index < 0) index = 0;

    active_layer_index_ = index;
    refreshActiveLayerUid();

    emit activeLayerIndexChanged(active_layer_index_);
}

void AppState::refreshActiveLayerUid() {
    const GroupLayer* root = activeRootLayer();
    if (root && active_layer_index_ >= 0 &&
        static_cast<size_t>(active_layer_index_) < root->layerCount()) {
        const Layer* layer = root->layerAt(active_layer_index_);
        if (layer && layer->uid() != active_layer_uid_) {
            active_layer_uid_ = layer->uid();
            emit activeLayerUidChanged(active_layer_uid_);
        }
    }
}

GroupLayer* AppState::activeRootLayer() {
    return &document_->rootLayerForFrame(timeline_->currentFrame());
}

const GroupLayer* AppState::activeRootLayer() const {
    const GroupLayer* peek = document_->peekRootLayerForFrame(timeline_->currentFrame());
    if (peek) return peek;
    return &document_->rootLayerForFrame(timeline_->currentFrame());
}

Layer* AppState::activeLayer() {
    GroupLayer* root = activeRootLayer();
    if (!root) return nullptr;
    return root->layerAt(active_layer_index_);
}

const Layer* AppState::activeLayer() const {
    const GroupLayer* root = activeRootLayer();
    if (!root) return nullptr;
    return root->layerAt(active_layer_index_);
}

bool AppState::isModified() const {
    return document_->modified();
}

const std::string& AppState::filepath() const {
    return document_->filepath();
}

void AppState::resetDocument(int width, int height, int fps, int totalFrames) {
    document_ = std::make_unique<Document>();
    document_->setCanvasSize(width, height);
    document_->setFPS(fps);
    document_->setTotalFrames(totalFrames);

    timeline_ = std::make_unique<Timeline>();
    timeline_->setTotalFrames(totalFrames);
    timeline_->setFPS(fps);
    timeline_->setCurrentFrame(0);

    audio_engine_ = std::make_unique<AudioEngine>();
    ruler_tool_ = std::make_unique<RulerTool>();
    tween_engine_ = std::make_unique<TweenEngine>();
    deformation_engine_ = std::make_unique<DeformationEngine>();
    thumbnail_cache_ = std::make_unique<FrameThumbnailCache>();
    frame_viewer_ = std::make_unique<FrameViewerData>();
    pencil_retouch_ = std::make_unique<PencilRetouchEngine>();

    thumbnail_cache_->setDocument(document_.get());
    frame_viewer_->setDocument(document_.get());
    wireSignals();

    active_layer_index_ = 0;

    emit documentChanged();
    emit canvasSizeChanged(width, height);
    emit currentFrameChanged(0);
    emit activeLayerIndexChanged(0);
}

} // namespace fap
