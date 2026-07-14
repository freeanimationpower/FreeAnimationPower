#include "core/app_state.hpp"
#include "core/document.hpp"
#include "core/sequence.hpp"
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
    , tool_state_(std::make_unique<ToolState>())
    , audio_engine_(std::make_unique<AudioEngine>())
    , ruler_tool_(std::make_unique<RulerTool>())
    , tween_engine_(std::make_unique<TweenEngine>())
    , deformation_engine_(std::make_unique<DeformationEngine>())
    , thumbnail_cache_(std::make_unique<FrameThumbnailCache>())
    , frame_viewer_(std::make_unique<FrameViewerData>())
    , pencil_retouch_(std::make_unique<PencilRetouchEngine>()) {
    tool_state_->resetToDefaults();
    wireSignals();
    wireSequenceSignals();
    thumbnail_cache_->setDocument(document_.get());
    frame_viewer_->setDocument(document_.get());
}

AppState::~AppState() = default;

void AppState::wireSignals() {
    QObject::connect(tool_state_.get(), &ToolState::toolSettingsChanged,
                     this, [this]() { emit documentChanged(); });
}

void AppState::wireSequenceSignals() {
    for (size_t i = 0; i < document_->sequenceCount(); ++i) {
        document_->sequenceAt(i).setOnFrameChanged(nullptr);
    }
    activeSequence().setOnFrameChanged([this](int frame) {
        emit currentFrameChanged(frame);
    });
}

Sequence& AppState::activeSequence() {
    return document_->activeSequence();
}

const Sequence& AppState::activeSequence() const {
    return document_->activeSequence();
}

int AppState::currentFrame() const {
    return document_->activeSequence().currentFrame();
}

void AppState::setCurrentFrame(int frame) {
    document_->activeSequence().setCurrentFrame(frame);
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
    return &document_->activeSequence().rootLayerForFrame(
        document_->activeSequence().currentFrame());
}

const GroupLayer* AppState::activeRootLayer() const {
    const GroupLayer* peek = document_->activeSequence().peekRootLayerForFrame(
        document_->activeSequence().currentFrame());
    if (peek) return peek;
    return &document_->activeSequence().rootLayerForFrame(
        document_->activeSequence().currentFrame());
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

int AppState::activeSequenceIndex() const {
    return static_cast<int>(document_->activeSequenceIndex());
}

int AppState::sequenceCount() const {
    return static_cast<int>(document_->sequenceCount());
}

void AppState::setActiveSequence(int index) {
    document_->setActiveSequence(static_cast<size_t>(index));
    wireSequenceSignals();

    active_layer_index_ = 0;
    active_layer_uid_ = 0;
    refreshActiveLayerUid();

    emit activeSequenceChanged(index);
    emit currentFrameChanged(document_->activeSequence().currentFrame());
    emit activeLayerIndexChanged(0);
    emit documentChanged();
}

void AppState::addSequence(const std::string& name) {
    document_->addSequence(name);
}

void AppState::duplicateSequence(int index) {
    document_->duplicateSequence(static_cast<size_t>(index));
}

void AppState::removeSequence(int index) {
    document_->removeSequence(static_cast<size_t>(index));
}

void AppState::renameSequence(int index, const std::string& name) {
    document_->renameSequence(static_cast<size_t>(index), name);
}

void AppState::setSequenceOpacity(int index, float opacity) {
    document_->sequenceAt(static_cast<size_t>(index)).setOpacity(opacity);
    emit documentChanged();
}

void AppState::setSequenceLocked(int index, bool locked) {
    document_->sequenceAt(static_cast<size_t>(index)).setLocked(locked);
    emit documentChanged();
}

void AppState::setWorkAreaStart(int frame) {
    activeSequence().setWorkAreaStart(frame);
    emit documentChanged();
}

void AppState::setWorkAreaEnd(int frame) {
    activeSequence().setWorkAreaEnd(frame);
    emit documentChanged();
}

void AppState::setDurationFrames(int count) {
    activeSequence().setDurationFrames(count);
    emit documentChanged();
}

void AppState::setFps(int fps) {
    if (activeSequence().fps() == fps) return;
    activeSequence().setFPS(fps);
    emit documentChanged();
}

void AppState::setLooping(bool looping) {
    activeSequence().setLooping(looping);
}

bool AppState::isLooping() const {
    return activeSequence().looping();
}

void AppState::moveSequence(int from, int to) {
    document_->moveSequence(static_cast<size_t>(from), static_cast<size_t>(to));
    emit documentChanged();
    emit activeSequenceChanged(static_cast<int>(document_->activeSequenceIndex()));
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
    document_->activeSequence().setFPS(fps);
    document_->activeSequence().setTotalFrames(totalFrames);
    document_->activeSequence().setCurrentFrame(0);

    audio_engine_ = std::make_unique<AudioEngine>();
    ruler_tool_ = std::make_unique<RulerTool>();
    tween_engine_ = std::make_unique<TweenEngine>();
    deformation_engine_ = std::make_unique<DeformationEngine>();
    thumbnail_cache_ = std::make_unique<FrameThumbnailCache>();
    frame_viewer_ = std::make_unique<FrameViewerData>();
    pencil_retouch_ = std::make_unique<PencilRetouchEngine>();

    tool_state_->resetToDefaults();

    thumbnail_cache_->setDocument(document_.get());
    frame_viewer_->setDocument(document_.get());
    wireSignals();
    wireSequenceSignals();

    active_layer_index_ = 0;

    emit documentChanged();
    emit canvasSizeChanged(width, height);
    emit currentFrameChanged(0);
    emit activeLayerIndexChanged(0);
}

} // namespace fap
