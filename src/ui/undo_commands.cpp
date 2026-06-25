#include "ui/undo_commands.hpp"
#include "core/layer.hpp"
#include "core/document.hpp"
#include <algorithm>
#include <QDebug>
#include <QPainter>

namespace fap {

CanvasCommand::CanvasCommand(CanvasWidget* canvas, int frame, int layerIdx,
                             const QString& text)
    : QUndoCommand(text)
    , canvas_(canvas)
    , frame_(frame)
    , layerModelIdx_(layerIdx) {
}

GroupLayer& CanvasCommand::root() const {
    return canvas_->currentRootLayer(frame_);
}

Layer* CanvasCommand::activeLayer() const {
    auto& layers = root().layers();
    if (layerModelIdx_ >= 0 && static_cast<size_t>(layerModelIdx_) < layers.size())
        return layers[layerModelIdx_].get();
    return nullptr;
}

void CanvasCommand::repaint() const {
    canvas_->update();
}

void writeRasterRect(RasterLayer* layer, const QRect& rect, const QImage& img) {
    if (!layer || img.isNull() || rect.isEmpty()) {
        qCritical() << "[UndoEngine] writeRasterRect ABORTADO: Capa nula, imagen nula o rect vacío. Rect:" << rect;
        return;
    }

    if (img.width() != rect.width() || img.height() != rect.height()) {
        qCritical() << "[UndoEngine] writeRasterRect ABORTADO: Discordancia de dimensiones. Img:"
                    << img.size() << "Rect:" << rect.size();
        return;
    }

    int ox = layer->originX();
    int oy = layer->originY();
    layer->ensureContains(ox, oy, rect.width(), rect.height());

    QImage converted = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    int dstX = rect.x() - layer->originX();
    int dstY = rect.y() - layer->originY();

    for (int y = 0; y < converted.height(); ++y) {
        const uint32_t* src = reinterpret_cast<const uint32_t*>(converted.constScanLine(y));
        uint32_t* dst = layer->pixelData();

        if (!src || !dst) continue;

        int targetIdx = (dstY + y) * layer->width() + dstX;

        if (targetIdx >= 0 && targetIdx + converted.width() <= layer->pixelCount()) {
            std::copy(src, src + converted.width(), dst + targetIdx);
        } else {
            qCritical() << "[UndoEngine] FATAL: Intento de escritura fuera de los límites del buffer en Y:" << y;
            break;
        }
    }

    layer->bufferEpochTick();
}

static QImage readRasterRect(RasterLayer* raster, const QRect& rect) {
    if (!raster || rect.isEmpty()) return {};
    QImage result(rect.width(), rect.height(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    int ox = raster->originX();
    int oy = raster->originY();
    int rw = raster->width();
    int rh = raster->height();
    for (int y = 0; y < rect.height(); ++y) {
        int srcY = rect.y() + y - oy;
        if (srcY < 0 || srcY >= rh) continue;
        int srcX = rect.x() - ox;
        int dstX = 0;
        int copyW = rect.width();
        if (srcX < 0) { dstX = -srcX; copyW -= dstX; srcX = 0; }
        if (srcX + copyW > rw) copyW = rw - srcX;
        if (copyW <= 0) continue;
        const uint32_t* srcRow = raster->pixelData() + srcY * rw + srcX;
        uint32_t* dstRow = reinterpret_cast<uint32_t*>(result.scanLine(y));
        if (srcRow && dstRow)
            std::copy(srcRow, srcRow + copyW, dstRow + dstX);
    }
    return result;
}

PaintCommand::PaintCommand(CanvasWidget* canvas, int frame, int layerIdx,
                           LayerUid layerUid,
                           QImage beforePixels, QImage afterPixels, QRect dirtyRect,
                           std::vector<RawStroke> beforeStrokes,
                           std::vector<RawStroke> afterStrokes,
                           const QString& text)
    : CanvasCommand(canvas, frame, layerIdx, text)
    , layerUid_(layerUid)
    , beforePixels_(std::move(beforePixels))
    , afterPixels_(std::move(afterPixels))
    , dirtyRect_(dirtyRect)
    , beforeStrokes_(std::move(beforeStrokes))
    , afterStrokes_(std::move(afterStrokes)) {
}

void PaintCommand::undo() {
    Layer* layer = root().layerByUid(layerUid_);
    if (!layer) return;

    canvas_->vectorStrokes()[frame_] = beforeStrokes_;

    if (layer->type() == LayerType::Raster) {
        auto* rasterLayer = static_cast<RasterLayer*>(layer);
        writeRasterRect(rasterLayer, dirtyRect_, beforePixels_);
    }
    repaint();
}

void PaintCommand::redo() {
    Layer* layer = root().layerByUid(layerUid_);
    if (!layer) return;

    canvas_->vectorStrokes()[frame_] = afterStrokes_;

    if (layer->type() == LayerType::Raster) {
        auto* rasterLayer = static_cast<RasterLayer*>(layer);
        writeRasterRect(rasterLayer, dirtyRect_, afterPixels_);
    }
    repaint();
}

bool PaintCommand::mergeWith(const QUndoCommand* other) {
    (void)other;
    return false;
}

MoveCommand::MoveCommand(CanvasWidget* canvas, int frame, int layerIdx,
                         LayerUid layerUid, QRectF srcRect, QPointF delta,
                         QImage contentImage, bool isSelection,
                         const QString& text)
    : CanvasCommand(canvas, frame, layerIdx, text)
    , layerUid_(layerUid)
    , srcRect_(srcRect)
    , delta_(delta)
    , contentImage_(std::move(contentImage))
    , isSelection_(isSelection) {
}

void MoveCommand::undo() {
    Q_UNUSED(isSelection_);
    Layer* layer = root().layerByUid(layerUid_);
    if (!layer) return;

    QImage layerImg;
    if (layer->type() == LayerType::Raster) {
        auto* rasterLayer = static_cast<RasterLayer*>(layer);
        QImage full = const_cast<CanvasWidget*>(canvas_)->readLayerPixels(layer);
        QPainter p(&full);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(srcRect_.topLeft(), contentImage_);
        p.end();
        canvas_->writeLayerPixels(layer, full);
    }
    repaint();
}

void MoveCommand::redo() {
    Layer* layer = root().layerByUid(layerUid_);
    if (!layer) return;

    QRectF newRect = srcRect_.translated(delta_);
    QImage movedContent = contentImage_;

    if (layer->type() == LayerType::Raster) {
        auto* rasterLayer = static_cast<RasterLayer*>(layer);
        QImage full = const_cast<CanvasWidget*>(canvas_)->readLayerPixels(layer);
        {
            QPainter p(&full);
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.fillRect(srcRect_.toRect(), Qt::transparent);
            p.drawImage(newRect.topLeft(), contentImage_);
        }
        canvas_->writeLayerPixels(layer, full);
    }
    repaint();
}

DeleteLayerCommand::DeleteLayerCommand(CanvasWidget* canvas, int frame, int modelIdx,
                                       std::unique_ptr<Layer> layer,
                                       const QString& text)
    : CanvasCommand(canvas, frame, modelIdx, text)
    , storedLayer_(std::move(layer)) {
}

void DeleteLayerCommand::undo() {
    root().layers().insert(root().layers().begin() + layerModelIdx_, std::move(storedLayer_));
    repaint();
}

void DeleteLayerCommand::redo() {
    storedLayer_ = std::move(root().layers()[layerModelIdx_]);
    root().layers().erase(root().layers().begin() + layerModelIdx_);
    repaint();
}

RenameLayerCommand::RenameLayerCommand(CanvasWidget* canvas, int frame, int layerIdx,
                                       LayerUid layerUid,
                                       std::string oldName, std::string newName,
                                       const QString& text)
    : CanvasCommand(canvas, frame, layerIdx, text)
    , layerUid_(layerUid)
    , oldName_(std::move(oldName))
    , newName_(std::move(newName)) {
}

void RenameLayerCommand::undo() {
    Layer* layer = root().layerByUid(layerUid_);
    if (layer) layer->setName(oldName_);
    repaint();
}

void RenameLayerCommand::redo() {
    Layer* layer = root().layerByUid(layerUid_);
    if (layer) layer->setName(newName_);
    repaint();
}

bool RenameLayerCommand::mergeWith(const QUndoCommand* other) {
    auto* cmd = dynamic_cast<const RenameLayerCommand*>(other);
    if (!cmd || cmd->layerUid_ != layerUid_) return false;
    newName_ = cmd->newName_;
    return true;
}

ToggleVisibilityCommand::ToggleVisibilityCommand(CanvasWidget* canvas, int frame,
                                                 int layerIdx, LayerUid layerUid,
                                                 const QString& text)
    : CanvasCommand(canvas, frame, layerIdx, text), layerUid_(layerUid) {
}

void ToggleVisibilityCommand::undo() {
    Layer* layer = root().layerByUid(layerUid_);
    if (layer) layer->setVisible(!layer->visible());
    repaint();
}

void ToggleVisibilityCommand::redo() {
    Layer* layer = root().layerByUid(layerUid_);
    if (layer) layer->setVisible(!layer->visible());
    repaint();
}

SetBlendModeCommand::SetBlendModeCommand(CanvasWidget* canvas, int frame, int layerIdx,
                                         LayerUid layerUid,
                                         BlendMode oldMode, BlendMode newMode,
                                         const QString& text)
    : CanvasCommand(canvas, frame, layerIdx, text)
    , layerUid_(layerUid)
    , oldMode_(oldMode)
    , newMode_(newMode) {
}

void SetBlendModeCommand::undo() {
    Layer* layer = root().layerByUid(layerUid_);
    if (layer) layer->setBlendMode(oldMode_);
    repaint();
}

void SetBlendModeCommand::redo() {
    Layer* layer = root().layerByUid(layerUid_);
    if (layer) layer->setBlendMode(newMode_);
    repaint();
}

bool SetBlendModeCommand::mergeWith(const QUndoCommand* other) {
    auto* cmd = dynamic_cast<const SetBlendModeCommand*>(other);
    if (!cmd || cmd->layerUid_ != layerUid_) return false;
    newMode_ = cmd->newMode_;
    return true;
}

} // namespace fap
