#include "canvas_widget_v2.hpp"

#include <QtWidgets/QApplication>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QFontDialog>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QPainterPath>
#include <QtGui/QClipboard>
#include <cmath>
#include <algorithm>
#include <functional>
#include <cstring>
#include <deque>

#include "engine/animation/frame_thumbnail.hpp"

#include "core/document.hpp"
#include "core/layer.hpp"
#include "core/undo_manager.hpp"
#include "core/tool_state.hpp"

namespace fap {

namespace {

static QPainter::CompositionMode toQtCompositionMode(BlendMode mode) {
    switch (mode) {
    case BlendMode::Normal:     return QPainter::CompositionMode_SourceOver;
    case BlendMode::Multiply:   return QPainter::CompositionMode_Multiply;
    case BlendMode::Screen:     return QPainter::CompositionMode_Screen;
    case BlendMode::Overlay:    return QPainter::CompositionMode_Overlay;
    case BlendMode::Add:        return QPainter::CompositionMode_Plus;
    case BlendMode::Subtract:   return QPainter::CompositionMode_Exclusion;
    case BlendMode::Darken:     return QPainter::CompositionMode_Darken;
    case BlendMode::Lighten:    return QPainter::CompositionMode_Lighten;
    case BlendMode::ColorBurn:  return QPainter::CompositionMode_ColorBurn;
    case BlendMode::ColorDodge: return QPainter::CompositionMode_ColorDodge;
    case BlendMode::SoftLight:  return QPainter::CompositionMode_SoftLight;
    case BlendMode::HardLight:  return QPainter::CompositionMode_HardLight;
    }
    return QPainter::CompositionMode_SourceOver;
}

class PaintCommandV2 : public UndoCommand {
public:
    PaintCommandV2(LayerUid layerUid, int frame, int layerIndex,
                   QImage before, QImage after, QRect dirtyRect)
        : layerUid_(layerUid)
        , frame_(frame)
        , layerIndex_(layerIndex)
        , beforePixels_(std::move(before))
        , afterPixels_(std::move(after))
        , dirtyRect_(dirtyRect)
    {}

    void undo() override { applySnapshot(beforePixels_); }
    void redo() override { applySnapshot(afterPixels_); }

    void setLayerAccess(std::function<RasterLayer*()> accessor) {
        layerAccessor_ = std::move(accessor);
    }

private:
    void applySnapshot(const QImage& snapshot) {
        if (!layerAccessor_) return;
        auto* layer = layerAccessor_();
        if (!layer) return;
        layer->ensureUnique();
        const int w = snapshot.width();
        const int h = snapshot.height();

        int ox = layer->originX();
        int oy = layer->originY();
        int lx0 = dirtyRect_.x() - ox;
        int ly0 = dirtyRect_.y() - oy;
        int lw = layer->width();
        int lh = layer->height();

        for (int y = 0; y < h; ++y) {
            int ly = ly0 + y;
            if (ly < 0 || ly >= lh) continue;
            const uint32_t* src = reinterpret_cast<const uint32_t*>(snapshot.constScanLine(y));
            if (!src) continue;
            uint32_t* dst = layer->pixelData() + static_cast<size_t>(ly) * static_cast<size_t>(lw);
            int copyStart = std::max(0, lx0);
            int copyEnd   = std::min(lw, lx0 + w);
            int srcOff    = copyStart - lx0;
            int count     = copyEnd - copyStart;
            if (count > 0) {
                std::copy(src + srcOff, src + srcOff + count, dst + copyStart);
            }
        }
    }

    LayerUid layerUid_;
    int frame_;
    int layerIndex_;
    QImage beforePixels_;
    QImage afterPixels_;
    QRect dirtyRect_;
    std::function<RasterLayer*()> layerAccessor_;
};

} // anonymous namespace

CanvasWidgetV2::CanvasWidgetV2(std::shared_ptr<AppState> state, QWidget* parent)
    : QWidget(parent)
    , appState_(std::move(state))
{
    setMinimumSize(400, 300);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto& doc = appState_->document();
    totalFrames_ = doc.totalFrames();

    auto& ts = appState_->toolState();
    brushColor_ = ts.primaryColor();
    brushSize_ = ts.brushSize();
    onionEnabled_ = ts.onionEnabled();
    onionPrevFrames_ = ts.onionPrevFrames();
    onionNextFrames_ = ts.onionNextFrames();
    onionOpacity_ = ts.onionOpacity();
}

void CanvasWidgetV2::setCurrentFrame(int frame)
{
    currentFrame_ = std::clamp(frame, 0, totalFrames_ - 1);
    update();
}

void CanvasWidgetV2::setTotalFrames(int count)
{
    totalFrames_ = std::max(1, count);
    if (currentFrame_ >= totalFrames_) currentFrame_ = totalFrames_ - 1;
    update();
}

void CanvasWidgetV2::setCurrentLayer(int index)
{
    currentLayerIndex_ = std::max(0, index);
    update();
}

void CanvasWidgetV2::setTool(int toolIndex)
{
    appState_->toolState().setActiveToolByIndex(toolIndex);
}

void CanvasWidgetV2::setColor(const QColor& color)
{
    brushColor_ = color;
    appState_->toolState().setPrimaryColor(color);
}

void CanvasWidgetV2::setBrushSize(int size)
{
    if (size < 1) size = 1;
    if (brushSize_ != size) {
        brushSize_ = size;
        update();
    }
}

void CanvasWidgetV2::setBrushShape(const QString& shape)
{
    if (brushShape_ != shape) {
        brushShape_ = shape;
        update();
    }
}

void CanvasWidgetV2::setOnionEnabled(bool enabled)
{
    onionEnabled_ = enabled;
    update();
}

void CanvasWidgetV2::setOnionSettings(int prevFrames, int nextFrames, int opacity)
{
    onionPrevFrames_ = prevFrames;
    onionNextFrames_ = nextFrames;
    onionOpacity_ = opacity;
    update();
}

void CanvasWidgetV2::fit()
{
    auto& doc = appState_->document();
    if (doc.width() <= 0 || doc.height() <= 0) return;
    float sx = static_cast<float>(width()) / static_cast<float>(doc.width());
    float sy = static_cast<float>(height()) / static_cast<float>(doc.height());
    zoom_ = std::min(sx, sy) * 0.9f;
    offsetX_ = 0.0f;
    offsetY_ = 0.0f;
    rotationAngle_ = 0.0f;
    flippedH_ = false;
    update();
}

void CanvasWidgetV2::flipH()
{
    flippedH_ = !flippedH_;
    update();
}

void CanvasWidgetV2::rotateCanvas()
{
    rotationAngle_ += 15.0f;
    if (rotationAngle_ >= 360.0f) rotationAngle_ -= 360.0f;
    update();
}

void CanvasWidgetV2::toggleGrid()
{
    showGrid_ = !showGrid_;
    update();
}

void CanvasWidgetV2::resizeCanvas(int w, int h)
{
    auto& doc = appState_->document();
    doc.setCanvasSize(w, h);
    update();
}

void CanvasWidgetV2::shiftFrameData(int fromFrame, int delta)
{
    appState_->document().shiftFrameData(fromFrame, delta);
    totalFrames_ = appState_->document().totalFrames();
    update();
}

void CanvasWidgetV2::removeFrameData(int frameIdx)
{
    appState_->document().removeFrameData(frameIdx);
    totalFrames_ = appState_->document().totalFrames();
    if (currentFrame_ >= totalFrames_) currentFrame_ = totalFrames_ - 1;
    update();
}

void CanvasWidgetV2::render(QPainter* painter)
{
    if (!painter) return;
    auto& doc = appState_->document();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    auto& root = doc.rootLayerForFrame(currentFrame_);
    for (size_t li = root.layerCount(); li > 0; --li) {
        const Layer* layer = root.layers()[li - 1].get();
        if (!layer || !layer->visible()) continue;
        if (layer->type() != LayerType::Raster) continue;

        const auto& rl = static_cast<const RasterLayer&>(*layer);
        QImage img(reinterpret_cast<const uchar*>(rl.pixelData()),
                   rl.width(), rl.height(),
                   rl.width() * static_cast<int>(sizeof(uint32_t)),
                   QImage::Format_ARGB32_Premultiplied);
        QImage display = img.convertToFormat(QImage::Format_ARGB32);
        painter->setOpacity(layer->opacity());
        painter->setCompositionMode(toQtCompositionMode(layer->blendMode()));
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter->drawImage(QPoint(rl.originX(), rl.originY()), display);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter->setRenderHint(QPainter::Antialiasing, true);
    }

    painter->restore();
}

void CanvasWidgetV2::undo()
{
    auto& um = appState_->document().undoManager();
    if (um.canUndo()) {
        um.undo();
        update();
    }
}

void CanvasWidgetV2::redo()
{
    auto& um = appState_->document().undoManager();
    if (um.canRedo()) {
        um.redo();
        update();
    }
}

void CanvasWidgetV2::copySelection()
{
    QImage toCopy;
    if (floatingActive_ && !floatingSelection_.isNull()) {
        toCopy = floatingSelection_;
    } else if (!selImage_.isNull()) {
        toCopy = selImage_;
    } else if (selRect_.isValid()) {
        auto* layer = activeRasterLayer();
        if (layer) {
            QRect r(static_cast<int>(selRect_.x() - layer->originX()),
                    static_cast<int>(selRect_.y() - layer->originY()),
                    static_cast<int>(selRect_.width()),
                    static_cast<int>(selRect_.height()));
            QImage img(reinterpret_cast<const uchar*>(layer->pixelData()),
                       layer->width(), layer->height(),
                       layer->width() * static_cast<int>(sizeof(uint32_t)),
                       QImage::Format_ARGB32_Premultiplied);
            toCopy = img.copy(r.intersected(QRect(0, 0, layer->width(), layer->height())));
        }
    }
    if (toCopy.isNull()) return;
    QApplication::clipboard()->setImage(toCopy);
    emit statusMessage("Copied to clipboard");
}

void CanvasWidgetV2::cutSelection()
{
    copySelection();

    if (floatingActive_) {
        auto* layer = activeRasterLayer();
        if (layer && !layer->locked()) {
            pushFullLayerUndo(layer, snapFullLayer(layer));
        }
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        selImage_ = QImage();
        selRect_ = QRectF();
        floatingActive_ = false;
        selState_ = SelectionState::None;
        emit canvasUpdated();
        emit statusMessage("Cut to clipboard");
        return;
    }

    if (!selRect_.isValid()) return;

    auto* layer = activeRasterLayer();
    if (layer && !layer->locked()) {
        auto before = snapFullLayer(layer);
        int ox = layer->originX();
        int oy = layer->originY();
        QRect bufRect(static_cast<int>(selRect_.x() - ox),
                      static_cast<int>(selRect_.y() - oy),
                      static_cast<int>(selRect_.width()),
                      static_cast<int>(selRect_.height()));
        bufRect = bufRect.intersected(QRect(0, 0, layer->width(), layer->height()));

        QImage img(reinterpret_cast<uchar*>(layer->pixelData()),
                   layer->width(), layer->height(),
                   layer->width() * static_cast<int>(sizeof(uint32_t)),
                   QImage::Format_ARGB32_Premultiplied);
        img = img.copy();
        {
            QPainter cp(&img);
            cp.setCompositionMode(QPainter::CompositionMode_Clear);
            cp.fillRect(bufRect, Qt::transparent);
        }
        for (int y = 0; y < img.height(); ++y) {
            const uint32_t* src = reinterpret_cast<const uint32_t*>(img.constScanLine(y));
            uint32_t* dst = layer->pixelData() + static_cast<size_t>(y) * static_cast<size_t>(layer->width());
            std::copy(src, src + img.width(), dst);
        }
        layer->bufferEpochTick();
        pushFullLayerUndo(layer, before);
    }

    selRect_ = QRectF();
    selImage_ = QImage();
    selState_ = SelectionState::None;
    emit canvasUpdated();
    emit statusMessage("Cut to clipboard");
}

void CanvasWidgetV2::pasteClipboard()
{
    QImage clip = QApplication::clipboard()->image();
    if (clip.isNull()) return;

    if (floatingActive_ && !floatingSelection_.isNull()) {
        commitFloatingSelection();
    }

    floatingSelection_ = clip.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    originalFloatingSelection_ = floatingSelection_;
    QPointF vc = widgetToCanvas(QPointF(width() / 2.0f, height() / 2.0f));
    int px = qRound(vc.x() - clip.width() / 2.0f);
    int py = qRound(vc.y() - clip.height() / 2.0f);
    selRect_ = QRectF(px, py, clip.width(), clip.height());
    selImage_ = QImage();
    floatingActive_ = true;
    selState_ = SelectionState::None;
    marchingTimer_.start();
    setTool(9);
    emit toolChangedByKey(9);
    emit canvasUpdated();
    emit statusMessage("Pasted");
}

void CanvasWidgetV2::deleteSelection()
{
    if (floatingActive_ && !floatingSelection_.isNull()) {
        auto* layer = activeRasterLayer();
        if (layer && !layer->locked()) {
            pushFullLayerUndo(layer, snapFullLayer(layer));
        }
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        selImage_ = QImage();
        selRect_ = QRectF();
        floatingActive_ = false;
        selState_ = SelectionState::None;
        emit canvasUpdated();
        return;
    }

    if (selRect_.isValid()) {
        auto* layer = activeRasterLayer();
        if (layer && !layer->locked()) {
            auto before = snapFullLayer(layer);
            int ox = layer->originX();
            int oy = layer->originY();
            QRect bufRect(static_cast<int>(selRect_.x() - ox),
                          static_cast<int>(selRect_.y() - oy),
                          static_cast<int>(selRect_.width()),
                          static_cast<int>(selRect_.height()));
            bufRect = bufRect.intersected(QRect(0, 0, layer->width(), layer->height()));

            QImage img(reinterpret_cast<uchar*>(layer->pixelData()),
                       layer->width(), layer->height(),
                       layer->width() * static_cast<int>(sizeof(uint32_t)),
                       QImage::Format_ARGB32_Premultiplied);
            img = img.copy();
            {
                QPainter cp(&img);
                cp.setCompositionMode(QPainter::CompositionMode_Clear);
                cp.fillRect(bufRect, Qt::transparent);
            }
            for (int y = 0; y < img.height(); ++y) {
                const uint32_t* src = reinterpret_cast<const uint32_t*>(img.constScanLine(y));
                uint32_t* dst = layer->pixelData() + static_cast<size_t>(y) * static_cast<size_t>(layer->width());
                std::copy(src, src + img.width(), dst);
            }
            layer->bufferEpochTick();
            pushFullLayerUndo(layer, before);
        }
        selRect_ = QRectF();
        selImage_ = QImage();
        selState_ = SelectionState::None;
        emit canvasUpdated();
    }
}

void CanvasWidgetV2::resetView()
{
    zoom_ = 1.0f;
    offsetX_ = 0.0f;
    offsetY_ = 0.0f;
    rotationAngle_ = 0.0f;
    fit();
    update();
}

void CanvasWidgetV2::paintEvent(QPaintEvent* event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor("#1A1D24"));

    auto& doc = appState_->document();
    const int docW = doc.width();
    const int docH = doc.height();

    p.save();
    p.translate(width() / 2.0f + offsetX_, height() / 2.0f + offsetY_);
    p.rotate(rotationAngle_);
    float sx = flippedH_ ? -zoom_ : zoom_;
    p.scale(sx, zoom_);
    p.translate(-docW / 2.0f, -docH / 2.0f);

    QRect canvasRect(0, 0, docW, docH);

    p.fillRect(canvasRect, QColor("#FFFFFF"));
    p.setPen(QPen(QColor("#2D3139"), 1.0f / zoom_));
    p.drawRect(canvasRect);

    auto& root = doc.rootLayerForFrame(currentFrame_);

    auto drawOnionLayer = [&](int frameOffset, float alpha) {
        if (!onionEnabled_) return;
        int f = currentFrame_ + frameOffset;
        if (f < 0 || f >= totalFrames_) return;
        auto& onionRoot = doc.rootLayerForFrame(f);
        for (size_t li = onionRoot.layerCount(); li > 0; --li) {
            const Layer* layer = onionRoot.layers()[li - 1].get();
            if (!layer || !layer->visible()) continue;
            if (layer->type() != LayerType::Raster) continue;
            const auto& rl = static_cast<const RasterLayer&>(*layer);
            QImage img(reinterpret_cast<const uchar*>(rl.pixelData()),
                       rl.width(), rl.height(),
                       rl.width() * static_cast<int>(sizeof(uint32_t)),
                       QImage::Format_ARGB32_Premultiplied);
            QColor tint;
            if (frameOffset < 0) tint = QColor(255, 100, 80, static_cast<int>(alpha * 255));
            else                 tint = QColor(80, 180, 255, static_cast<int>(alpha * 255));

            QImage tinted(img.size(), QImage::Format_ARGB32_Premultiplied);
            tinted.fill(tint);
            {
                QPainter tp(&tinted);
                tp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                tp.drawImage(0, 0, img);
            }

            p.save();
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.setOpacity(alpha);
            p.setRenderHint(QPainter::Antialiasing, false);
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
            p.drawImage(QPoint(rl.originX(), rl.originY()), tinted.convertToFormat(QImage::Format_ARGB32));
            p.restore();
        }
    };

    for (int i = onionPrevFrames_; i >= 1; --i) {
        float alpha = (onionOpacity_ / 100.0f) * (1.0f - static_cast<float>(i - 1) / onionPrevFrames_);
        drawOnionLayer(-i, alpha);
    }
    for (int i = 1; i <= onionNextFrames_; ++i) {
        float alpha = (onionOpacity_ / 100.0f) * (1.0f - static_cast<float>(i - 1) / onionNextFrames_);
        drawOnionLayer(i, alpha);
    }

    for (size_t li = root.layerCount(); li > 0; --li) {
        const Layer* layer = root.layers()[li - 1].get();
        if (!layer || !layer->visible()) continue;
        if (layer->type() != LayerType::Raster) continue;

        const auto& rl = static_cast<const RasterLayer&>(*layer);
        QImage img(reinterpret_cast<const uchar*>(rl.pixelData()),
                   rl.width(), rl.height(),
                   rl.width() * static_cast<int>(sizeof(uint32_t)),
                   QImage::Format_ARGB32_Premultiplied);
        QImage display = img.convertToFormat(QImage::Format_ARGB32);
        p.setOpacity(layer->opacity());
        p.setCompositionMode(toQtCompositionMode(layer->blendMode()));

        QTransform wt = p.worldTransform();
        qreal rx = static_cast<qreal>(rl.originX());
        qreal ry = static_cast<qreal>(rl.originY());
        qreal devX = wt.m11() * rx + wt.m21() * ry + wt.dx();
        qreal devY = wt.m12() * rx + wt.m22() * ry + wt.dy();
        QTransform snapped(wt.m11(), wt.m12(), wt.m13(),
                           wt.m21(), wt.m22(), wt.m23(),
                           wt.dx() + (std::round(devX) - devX),
                           wt.dy() + (std::round(devY) - devY),
                           wt.m33());
        p.setWorldTransform(snapped, false);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(QPoint(rl.originX(), rl.originY()), display);
        p.setWorldTransform(wt, false);
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.setRenderHint(QPainter::Antialiasing, true);
    }

    if (showGrid_) {
        p.setOpacity(0.15f);
        p.setPen(QPen(QColor("#6B7088"), 0.5f / zoom_));
        int gridSize = 32;
        for (int x = 0; x <= docW; x += gridSize)
            p.drawLine(x, 0, x, docH);
        for (int y = 0; y <= docH; y += gridSize)
            p.drawLine(0, y, docW, y);
    }

    if (drawing_) {
        auto tool = appState_->toolState().activeTool();
        if (tool >= ToolType::Line && tool <= ToolType::Ellipse) {
            p.setPen(QPen(QColor(60, 140, 255, 180), 2.0f / zoom_, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            double x1 = shapeStart_.x(), y1 = shapeStart_.y();
            double x2 = shapeEnd_.x(), y2 = shapeEnd_.y();
            if (tool == ToolType::Line)
                p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
            else if (tool == ToolType::Rectangle)
                p.drawRect(QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized());
            else if (tool == ToolType::Ellipse)
                p.drawEllipse(QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized());
        } else if (tool == ToolType::Brush || tool == ToolType::Eraser) {
            float r = brushSize_ / 2.0f;
            p.setOpacity(0.5f);
            p.setPen(QPen(QColor("#FF6B4A"), 1.5f / zoom_));
            p.setBrush(Qt::NoBrush);

            QPointF center = virtualCursorPos_;
            float flatStretch = 4.0f;
            float calliStretch = 5.0f;
            float highlighterStretch = 8.0f;

            if (brushShape_ == "Square") {
                p.drawRect(QRectF(center.x() - r, center.y() - r, r * 2, r * 2));
            } else if (brushShape_ == "Flat") {
                p.save();
                p.translate(center);
                p.scale(flatStretch, 1.0);
                p.drawEllipse(QPointF(0, 0), r, r);
                p.restore();
            } else if (brushShape_ == "Calligraphy") {
                p.save();
                p.translate(center);
                p.rotate(45.0f);
                p.scale(calliStretch, 1.0);
                p.drawEllipse(QPointF(0, 0), r, r);
                p.restore();
            } else if (brushShape_ == "Pencil") {
                p.drawEllipse(center, r, r);
                p.setPen(QPen(QColor("#FF6B4A"), 0.5f / zoom_, Qt::DotLine));
                p.drawEllipse(center, r * 0.85f, r * 0.85f);
            } else if (brushShape_ == "Highlighter") {
                p.save();
                p.translate(center);
                p.scale(highlighterStretch, highlighterStretch * 0.30f);
                p.drawEllipse(QPointF(0, 0), r, r);
                p.restore();
            } else {
                p.drawEllipse(center, r, r);
            }

            p.setPen(QPen(QColor("#FFFFFF"), 0.5f / zoom_));
            if (brushShape_ == "Square") {
                p.drawRect(QRectF(center.x() - r + 0.5f / zoom_, center.y() - r + 0.5f / zoom_,
                                   r * 2 - 1.0f / zoom_, r * 2 - 1.0f / zoom_));
            } else {
                p.drawEllipse(center, r - 0.5f / zoom_, r - 0.5f / zoom_);
            }
        }
    }

    // Selection rectangle and floating selection
    if (selState_ == SelectionState::Creating || (floatingActive_ && selRect_.isValid())) {
        p.setPen(QPen(QColor("#FF6B4A"), 1.5f / zoom_, Qt::SolidLine));
        p.setBrush(QColor(255, 107, 74, 30));
        p.drawRect(selRect_);

        if (floatingActive_ && !floatingSelection_.isNull()) {
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.drawImage(selRect_.topLeft(), floatingSelection_);

            // Marching ants on the selection border
            if (marchingTimer_.isValid()) {
                qint64 elapsed = marchingTimer_.elapsed() / 128;
                int offset = static_cast<int>(elapsed % 16);
                p.setBrush(Qt::NoBrush);
                QPen antsPen(QColor(255, 255, 255, 200), 1.0f / zoom_, Qt::CustomDashLine);
                QVector<qreal> dashes = {4.0f / zoom_, 4.0f / zoom_};
                antsPen.setDashPattern(dashes);
                antsPen.setDashOffset(-(offset * 4.0f / zoom_));
                p.setPen(antsPen);
                p.drawRect(selRect_);
            }

            // Selection handles
            qreal hs = 6.0f / zoom_;
            p.setPen(QPen(QColor("#FFFFFF"), 1.0f / zoom_));
            p.setBrush(QColor("#FF6B4A"));
            QPointF corners[4] = {
                selRect_.topLeft(), selRect_.topRight(),
                selRect_.bottomRight(), selRect_.bottomLeft()
            };
            for (int i = 0; i < 4; ++i) {
                p.drawRect(QRectF(corners[i].x() - hs, corners[i].y() - hs, hs * 2, hs * 2));
            }
            QPointF midPoints[4] = {
                QPointF((corners[0].x() + corners[1].x()) * 0.5f, corners[0].y()),
                QPointF(corners[1].x(), (corners[1].y() + corners[2].y()) * 0.5f),
                QPointF((corners[2].x() + corners[3].x()) * 0.5f, corners[2].y()),
                QPointF(corners[0].x(), (corners[0].y() + corners[3].y()) * 0.5f),
            };
            for (int i = 0; i < 4; ++i) {
                p.drawRect(QRectF(midPoints[i].x() - hs * 0.7f, midPoints[i].y() - hs * 0.7f,
                                   hs * 1.4f, hs * 1.4f));
            }
        }
    }

    // Move preview
    if (moving_ && !moveImage_.isNull()) {
        p.setOpacity(0.5f);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.drawImage(moveSrcRect_.topLeft() + moveOffset_, moveImage_);
    }

    p.restore();
}

RasterLayer* CanvasWidgetV2::activeRasterLayer()
{
    auto* layer = appState_->activeLayer();
    if (!layer || layer->type() != LayerType::Raster) return nullptr;
    if (layer->locked()) return nullptr;
    return static_cast<RasterLayer*>(layer);
}

QImage CanvasWidgetV2::captureRect(const QRect& r)
{
    if (r.isEmpty()) return QImage();

    QImage snap(r.width(), r.height(), QImage::Format_ARGB32_Premultiplied);
    snap.fill(0);

    auto* layer = activeRasterLayer();
    if (!layer) return snap;

    int ox = layer->originX();
    int oy = layer->originY();
    int lw = layer->width();
    int lh = layer->height();

    int cx = std::max(r.x(), ox);
    int cy = std::max(r.y(), oy);
    int cw = std::min(r.right(), ox + lw) - cx;
    int ch = std::min(r.bottom(), oy + lh) - cy;

    if (cw <= 0 || ch <= 0) return snap;

    int dstX = cx - r.x();
    int dstY = cy - r.y();

    const uint32_t* src = layer->pixelData();
    for (int y = 0; y < ch; ++y) {
        int ly = (cy - oy) + y;
        int dstLine = dstY + y;
        uint32_t* dst = reinterpret_cast<uint32_t*>(snap.scanLine(dstLine)) + dstX;
        const uint32_t* row = src + static_cast<size_t>(ly) * static_cast<size_t>(lw) + (cx - ox);
        std::copy(row, row + cw, dst);
    }
    return snap;
}

void CanvasWidgetV2::growBeforeSnapshot(const QRect& oldRect, const QRect& newRect)
{
    if (beforeSnapshot_.isNull()) return;
    if (oldRect == newRect) return;

    QImage grown(newRect.width(), newRect.height(), QImage::Format_ARGB32_Premultiplied);
    grown.fill(0);

    int ox = oldRect.x() - newRect.x();
    int oy = oldRect.y() - newRect.y();

    for (int y = 0; y < oldRect.height(); ++y) {
        int gy = oy + y;
        if (gy < 0 || gy >= grown.height()) continue;
        const uint32_t* srow = reinterpret_cast<const uint32_t*>(beforeSnapshot_.constScanLine(y));
        uint32_t* drow = reinterpret_cast<uint32_t*>(grown.scanLine(gy));
        std::copy(srow, srow + oldRect.width(), drow + ox);
    }

    if (!cleanLayerCopy_.isNull()) {
        auto* layer = activeRasterLayer();
        int lox = layer ? layer->originX() : 0;
        int loy = layer ? layer->originY() : 0;
        for (int y = 0; y < grown.height(); ++y) {
            uint32_t* drow = reinterpret_cast<uint32_t*>(grown.scanLine(y));
            for (int x = 0; x < grown.width(); ++x) {
                bool inOld = (x >= ox && x < ox + oldRect.width() &&
                              y >= oy && y < oy + oldRect.height());
                if (inOld) continue;
                int lx = newRect.x() + x - lox;
                int ly = newRect.y() + y - loy;
                if (lx >= 0 && lx < cleanLayerCopy_.width() && ly >= 0 && ly < cleanLayerCopy_.height()) {
                    drow[x] = reinterpret_cast<const uint32_t*>(cleanLayerCopy_.constScanLine(ly))[lx];
                }
            }
        }
    }

    beforeSnapshot_ = std::move(grown);
}

void CanvasWidgetV2::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;

    auto tool = appState_->toolState().activeTool();
    if (tool == ToolType::Brush || tool == ToolType::Eraser) {
        auto* active = appState_->activeLayer();
        if (!active || active->type() != LayerType::Raster) return;
        if (active->locked()) {
            emit statusMessage("Layer is locked - unlock it to draw");
            return;
        }
        auto* layer = static_cast<RasterLayer*>(active);

        layer->ensureUnique();

        drawing_ = true;
        strokeDistance_ = 0.0f;
        lastMousePos_ = event->pos();

        QPointF cp = widgetToCanvas(event->pos());
        virtualCursorPos_ = cp;
        prevVirtualCursorPos_ = cp;

        int cx = static_cast<int>(cp.x());
        int cy = static_cast<int>(cp.y());

        cleanLayerCopy_ = QImage(reinterpret_cast<const uchar*>(layer->pixelData()),
                                  layer->width(), layer->height(),
                                  layer->width() * static_cast<int>(sizeof(uint32_t)),
                                  QImage::Format_ARGB32_Premultiplied).copy();

        activeDirtyRect_ = stampRect(cx, cy);
        beforeSnapshot_ = captureRect(activeDirtyRect_);
    } else if (tool == ToolType::ColorPicker) {
        QPointF cp = widgetToCanvas(event->pos());
        auto& doc = appState_->document();
        int cx = static_cast<int>(cp.x());
        int cy = static_cast<int>(cp.y());

        auto& root = doc.rootLayerForFrame(currentFrame_);
        for (size_t li = root.layerCount(); li > 0; --li) {
            const Layer* layer = root.layers()[li - 1].get();
            if (!layer || !layer->visible()) continue;
            if (layer->type() != LayerType::Raster) continue;
            const auto& rl = static_cast<const RasterLayer&>(*layer);
            int lx = cx - rl.originX();
            int ly = cy - rl.originY();
            if (lx >= 0 && lx < rl.width() && ly >= 0 && ly < rl.height()) {
                Color c = rl.pixelAt(lx, ly);
                if (c.a > 0.0f) {
                    QColor qc = QColor::fromRgbF(c.r, c.g, c.b, c.a);
                    brushColor_ = qc;
                    appState_->toolState().setPrimaryColor(qc);
                    appState_->toolState().setSampledColor(qc);
                    emit colorPicked(qc);
                    return;
                }
            }
        }
    } else if (tool == ToolType::Hand) {
        lastMousePos_ = event->pos();
    } else if (tool == ToolType::Fill) {
        QPointF cp = widgetToCanvas(event->pos());
        doFill(cp);
    } else if (tool == ToolType::Text) {
        QPointF cp = widgetToCanvas(event->pos());
        doText(cp);
    } else if (tool == ToolType::Move) {
        QPointF cp = widgetToCanvas(event->pos());
        startMove(cp);
    } else if (tool == ToolType::Select) {
        QPointF cp = widgetToCanvas(event->pos());
        if (floatingActive_ && selRect_.isValid()) {
            qreal hs = 8.0f / zoom_;
            QPointF c[4] = {selRect_.topLeft(), selRect_.topRight(),
                            selRect_.bottomRight(), selRect_.bottomLeft()};
            QPointF h[8] = {
                c[0], QPointF((c[0].x()+c[1].x())*0.5, c[0].y()),
                c[1], QPointF(c[1].x(), (c[1].y()+c[2].y())*0.5),
                c[2], QPointF((c[2].x()+c[3].x())*0.5, c[2].y()),
                c[3], QPointF(c[0].x(), (c[0].y()+c[3].y())*0.5),
            };
            int hit = -1;
            for (int i = 0; i < 8; ++i) {
                if (std::abs(cp.x() - h[i].x()) < hs && std::abs(cp.y() - h[i].y()) < hs) {
                    hit = i; break;
                }
            }
            if (hit >= 0) {
                selState_ = SelectionState::DraggingHandle;
                selHandleIdx_ = hit;
                selDragStart_ = cp;
                return;
            }
            if (selRect_.contains(cp)) {
                selState_ = SelectionState::MovingContent;
                selDragStart_ = cp;
                return;
            }
            commitFloatingSelection();
            return;
        }
        selStart_ = cp;
        selState_ = SelectionState::Creating;
    } else if (tool >= ToolType::Line && tool <= ToolType::Ellipse) {
        QPointF cp = widgetToCanvas(event->pos());
        shapeStart_ = cp;
        shapeEnd_ = cp;
        drawing_ = true;
    }
}

void CanvasWidgetV2::mouseMoveEvent(QMouseEvent* event)
{
    auto tool = appState_->toolState().activeTool();

    if (drawing_ && (tool == ToolType::Brush || tool == ToolType::Eraser)) {
        QPointF mouseCanvas = widgetToCanvas(event->pos());
        float stabilizerLevel = static_cast<float>(appState_->toolState().stabilizerLevel());
        QPointF newVirtual = mouseCanvas;

        if (stabilizerLevel > 0.0f) {
            float dt = 0.016f;
            float cutoff = 1.0f + stabilizerLevel * 0.002f;
            float tau = 1.0f / (2.0f * 3.14159265f * cutoff);
            float alpha = 1.0f / (1.0f + tau / dt);
            newVirtual.setX(virtualCursorPos_.x() + (mouseCanvas.x() - virtualCursorPos_.x()) * alpha);
            newVirtual.setY(virtualCursorPos_.y() + (mouseCanvas.y() - virtualCursorPos_.y()) * alpha);
        }

        prevVirtualCursorPos_ = virtualCursorPos_;
        virtualCursorPos_ = newVirtual;

        float segLen = std::sqrt(
            (virtualCursorPos_.x() - prevVirtualCursorPos_.x()) * (virtualCursorPos_.x() - prevVirtualCursorPos_.x()) +
            (virtualCursorPos_.y() - prevVirtualCursorPos_.y()) * (virtualCursorPos_.y() - prevVirtualCursorPos_.y()));
        if (segLen > 0.1f) {
            strokeDistance_ += segLen;
            drawLineStamps(prevVirtualCursorPos_, virtualCursorPos_);
        }

        lastMousePos_ = event->pos();
        update();
    } else if (event->buttons() & Qt::LeftButton && tool == ToolType::Hand) {
        QPointF delta = event->pos() - lastMousePos_;
        offsetX_ += static_cast<float>(delta.x());
        offsetY_ += static_cast<float>(delta.y());
        lastMousePos_ = event->pos();
        update();
    } else if (tool == ToolType::ColorPicker && event->buttons() & Qt::LeftButton) {
        QPointF cp = widgetToCanvas(event->pos());
        auto& doc = appState_->document();
        int cx = static_cast<int>(cp.x());
        int cy = static_cast<int>(cp.y());

        auto& root = doc.rootLayerForFrame(currentFrame_);
        for (size_t li = root.layerCount(); li > 0; --li) {
            const Layer* layer = root.layers()[li - 1].get();
            if (!layer || !layer->visible()) continue;
            if (layer->type() != LayerType::Raster) continue;
            const auto& rl = static_cast<const RasterLayer&>(*layer);
            int lx = cx - rl.originX();
            int ly = cy - rl.originY();
            if (lx >= 0 && lx < rl.width() && ly >= 0 && ly < rl.height()) {
                Color c = rl.pixelAt(lx, ly);
                if (c.a > 0.0f) {
                    QColor qc = QColor::fromRgbF(c.r, c.g, c.b, c.a);
                    brushColor_ = qc;
                    appState_->toolState().setPrimaryColor(qc);
                    appState_->toolState().setSampledColor(qc);
                    emit colorPicked(qc);
                    break;
                }
            }
        }
    } else if (drawing_ && tool >= ToolType::Line && tool <= ToolType::Ellipse) {
        shapeEnd_ = widgetToCanvas(event->pos());
        update();
    } else if (moving_) {
        QPointF cp = widgetToCanvas(event->pos());
        updateMove(cp);
    } else if (selState_ == SelectionState::Creating) {
        QPointF cp = widgetToCanvas(event->pos());
        selRect_ = QRectF(selStart_, cp).normalized();
        update();
    } else if (selState_ == SelectionState::MovingContent) {
        QPointF cp = widgetToCanvas(event->pos());
        QPointF delta = cp - selDragStart_;
        selRect_.translate(delta);
        selDragStart_ = cp;
        update();
    } else if (selState_ == SelectionState::DraggingHandle) {
        QPointF cp = widgetToCanvas(event->pos());
        static const struct { bool l, t, r, b; } edge[8] = {
            {1,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,1,0},
            {0,0,1,1},{0,0,0,1},{1,0,0,1},{1,0,0,0},
        };
        qreal l = selRect_.left(), t = selRect_.top(), r = selRect_.right(), b = selRect_.bottom();
        auto& f = edge[selHandleIdx_];
        if (f.l) l = cp.x();
        if (f.t) t = cp.y();
        if (f.r) r = cp.x();
        if (f.b) b = cp.y();
        qreal nl = std::min(l, r), nr = std::max(l, r);
        qreal nt = std::min(t, b), nb = std::max(t, b);
        if (nr - nl < 2.0) { nr = nl + 2.0; if (f.l) nl = nr - 2.0; else if (f.r) nr = nl + 2.0; }
        if (nb - nt < 2.0) { nb = nt + 2.0; if (f.t) nt = nb - 2.0; else if (f.b) nb = nt + 2.0; }
        selRect_ = QRectF(QPointF(nl, nt), QPointF(nr, nb));
        update();
    }
}

void CanvasWidgetV2::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;

    if (drawing_) {
        auto tool = appState_->toolState().activeTool();
        if (tool >= ToolType::Line && tool <= ToolType::Ellipse) {
            if (shapeStart_.x() >= 0 && shapeEnd_.x() >= 0) {
                drawShape();
            }
            shapeStart_ = {-1, -1};
            shapeEnd_ = {-1, -1};
            drawing_ = false;
            emit canvasUpdated();
            return;
        }
        if (strokeDistance_ < 0.01f) {
            drawBrushStamp(static_cast<int>(virtualCursorPos_.x()),
                           static_cast<int>(virtualCursorPos_.y()));
        }
        drawing_ = false;
        commitStroke();
    }

    if (moving_) {
        commitMove();
    }

    if (selState_ == SelectionState::Creating) {
        if (selRect_.width() < 3 || selRect_.height() < 3) {
            selRect_ = QRectF();
            floatingSelection_ = QImage();
            originalFloatingSelection_ = QImage();
            selState_ = SelectionState::None;
            update();
            return;
        }
        commitSelection();
        selState_ = SelectionState::None;
    } else if (selState_ == SelectionState::MovingContent) {
        selState_ = SelectionState::None;
        update();
    } else if (selState_ == SelectionState::DraggingHandle) {
        selState_ = SelectionState::None;
        update();
    }
}

void CanvasWidgetV2::wheelEvent(QWheelEvent* event)
{
    float factor = (event->angleDelta().y() > 0) ? 1.15f : 0.87f;
    zoom_ = std::clamp(zoom_ * factor, 0.02f, 50.0f);
    update();
}

void CanvasWidgetV2::keyPressEvent(QKeyEvent* event)
{
    bool handled = true;
    bool ctrl = event->modifiers() & Qt::ControlModifier;

    if (ctrl) {
        switch (event->key()) {
            case Qt::Key_Z: undo(); break;
            case Qt::Key_Y: redo(); break;
            case Qt::Key_C: copySelection(); break;
            case Qt::Key_X: cutSelection(); break;
            case Qt::Key_V: pasteClipboard(); break;
            case Qt::Key_H: flippedH_ = !flippedH_; update(); break;
            case Qt::Key_0: resetView(); break;
            default: handled = false; break;
        }
        if (handled) {
            event->accept();
            emit canvasUpdated();
            return;
        }
    }

    handled = true;
    switch (event->key()) {
        case Qt::Key_B: emit toolChangedByKey(0); break;
        case Qt::Key_E: emit toolChangedByKey(1); break;
        case Qt::Key_I: emit toolChangedByKey(2); break;
        case Qt::Key_G: emit toolChangedByKey(3); break;
        case Qt::Key_T: emit toolChangedByKey(4); break;
        case Qt::Key_L: emit toolChangedByKey(5); break;
        case Qt::Key_U: emit toolChangedByKey(6); break;
        case Qt::Key_Y: emit toolChangedByKey(7); break;
        case Qt::Key_M: emit toolChangedByKey(8); break;
        case Qt::Key_S: emit toolChangedByKey(9); break;
        case Qt::Key_H: emit toolChangedByKey(10); break;
        case Qt::Key_F: fit(); break;
        case Qt::Key_R: rotateCanvas(); break;
        case Qt::Key_Apostrophe: toggleGrid(); break;
        case Qt::Key_Space: emit togglePlayPause(); break;
        case Qt::Key_Delete:
        case Qt::Key_Backspace: deleteSelection(); break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (floatingActive_) { commitFloatingSelection(); }
            else if (selState_ == SelectionState::MovingContent) { commitSelection(); }
            break;
        case Qt::Key_BracketLeft: {
            int newSize = std::max(1, brushSize_ - 5);
            setBrushSize(newSize);
            appState_->toolState().setBrushSize(newSize);
            break;
        }
        case Qt::Key_BracketRight: {
            int newSize = brushSize_ + 5;
            setBrushSize(newSize);
            appState_->toolState().setBrushSize(newSize);
            break;
        }
        case Qt::Key_Left: {
            int frame = std::max(0, currentFrame_ - 1);
            currentFrame_ = frame;
            appState_->setCurrentFrame(frame);
            emit frameChanged(frame);
            break;
        }
        case Qt::Key_Right: {
            int frame = std::min(totalFrames_ - 1, currentFrame_ + 1);
            currentFrame_ = frame;
            appState_->setCurrentFrame(frame);
            emit frameChanged(frame);
            break;
        }
        default: handled = false; break;
    }
    if (handled) {
        event->accept();
        emit canvasUpdated();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void CanvasWidgetV2::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

QPointF CanvasWidgetV2::widgetToCanvas(const QPointF& pos) const
{
    auto& doc = appState_->document();
    float cx = static_cast<float>(width()) / 2.0f + offsetX_;
    float cy = static_cast<float>(height()) / 2.0f + offsetY_;
    float dx = pos.x() - cx;
    float dy = pos.y() - cy;

    if (rotationAngle_ != 0.0f) {
        float rad = static_cast<float>(-rotationAngle_ * M_PI / 180.0);
        float rx = dx * std::cos(rad) - dy * std::sin(rad);
        float ry = dx * std::sin(rad) + dy * std::cos(rad);
        dx = rx;
        dy = ry;
    }

    float sx = flippedH_ ? -zoom_ : zoom_;
    return QPointF(dx / sx + doc.width() / 2.0f,
                   dy / zoom_ + doc.height() / 2.0f);
}

QPointF CanvasWidgetV2::canvasToWidget(const QPointF& pos) const
{
    auto& doc = appState_->document();
    float sx = flippedH_ ? -zoom_ : zoom_;
    float dx = (pos.x() - doc.width() / 2.0f) * sx;
    float dy = (pos.y() - doc.height() / 2.0f) * zoom_;

    if (rotationAngle_ != 0.0f) {
        float rad = static_cast<float>(rotationAngle_ * M_PI / 180.0);
        float rx = dx * std::cos(rad) - dy * std::sin(rad);
        float ry = dx * std::sin(rad) + dy * std::cos(rad);
        dx = rx;
        dy = ry;
    }

    return QPointF(dx + width() / 2.0f + offsetX_,
                   dy + height() / 2.0f + offsetY_);
}

QRect CanvasWidgetV2::stampRect(int cx, int cy) const
{
    int radius = brushSize_ / 2;
    return QRect(cx - radius, cy - radius, brushSize_, brushSize_);
}

void CanvasWidgetV2::drawBrushStamp(int cx, int cy)
{
    auto* layer = activeRasterLayer();
    if (!layer) return;

    int radius = brushSize_ / 2;

    if (brushShape_ == "Calligraphy") {
        float maxDist = 200.0f;
        float t = std::min(strokeDistance_ / maxDist, 1.0f);
        float scale = 0.15f + t * 0.85f;
        radius = std::max(1, static_cast<int>(radius * scale));
    }
    int ox = layer->originX();
    int oy = layer->originY();

    auto tool = appState_->toolState().activeTool();
    bool erasing = (tool == ToolType::Eraser);
    float opacity = appState_->toolState().brushOpacity() / 100.0f;

    QColor color = brushColor_;
    if (erasing) {
        color = QColor(0, 0, 0, 0);
    }

    float flatStretch = 4.0f;
    float calliStretch = 5.0f;
    float calliAngle = 0.785398f;  // 45 degrees in radians
    float highlighterStretch = 8.0f;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            bool inside = false;
            float r2 = static_cast<float>(radius * radius);

            if (brushShape_ == "Round") {
                inside = (dx * dx + dy * dy <= r2);
            } else if (brushShape_ == "Square") {
                inside = true;
            } else if (brushShape_ == "Flat") {
                float ex = static_cast<float>(dx) / flatStretch;
                inside = (ex * ex + dy * dy <= r2);
            } else if (brushShape_ == "Calligraphy") {
                float cosA = std::cos(calliAngle);
                float sinA = std::sin(calliAngle);
                float rx = dx * cosA + dy * sinA;
                float ry = -dx * sinA + dy * cosA;
                float ex = rx / calliStretch;
                inside = (ex * ex + ry * ry <= r2);
            } else if (brushShape_ == "Pencil") {
                float dist = static_cast<float>(dx * dx + dy * dy);
                float edgeFuzz = (static_cast<float>((dx * 7 + dy * 13) & 0xFF) / 512.0f) * r2;
                inside = (dist <= r2 + edgeFuzz);
                if (inside) {
                    uint32_t hash = static_cast<uint32_t>(dx * 374761393 + dy * 668265263 + cx * 1267128163 + cy * 3266489917);
                    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
                    hash = ((hash >> 16) ^ hash);
                    float grain = static_cast<float>(hash & 0xFFFF) / 65536.0f;
                    float threshold = 0.25f;
                    float edgeFactor = (r2 - dist) / r2;
                    if (edgeFactor < 0.2f) threshold = 0.15f;
                    if (grain < threshold) inside = false;
                }
            } else if (brushShape_ == "Highlighter") {
                float ex = static_cast<float>(dx) / highlighterStretch;
                float ey = static_cast<float>(dy) / (highlighterStretch * 0.30f);
                inside = (ex * ex + ey * ey <= r2);
            } else {
                inside = (dx * dx + dy * dy <= r2);
            }

            if (!inside) continue;

            int lx = cx + dx - ox;
            int ly = cy + dy - oy;
            if (lx < 0 || lx >= layer->width() || ly < 0 || ly >= layer->height()) continue;

            float alpha = static_cast<float>(color.alpha()) / 255.0f * opacity;
            bool blendMode = false;
            if (brushShape_ == "Highlighter") {
                alpha *= 0.35f;
                blendMode = true;
            }
            Color c = Color::fromRGBA(
                static_cast<uint8_t>(color.red()),
                static_cast<uint8_t>(color.green()),
                static_cast<uint8_t>(color.blue()),
                static_cast<uint8_t>(alpha * 255.0f));
            if (blendMode) {
                layer->blendPixel(lx, ly, c);
            } else {
                layer->setPixel(lx, ly, c);
            }
        }
    }
}

void CanvasWidgetV2::drawLineStamps(const QPointF& from, const QPointF& to)
{
    float radius = brushSize_ / 2.0f;
    float spacing = std::max(0.5f, radius * 0.08f);

    float dx = static_cast<float>(to.x() - from.x());
    float dy = static_cast<float>(to.y() - from.y());
    float dist = std::sqrt(dx * dx + dy * dy);

    int steps = std::max(1, static_cast<int>(dist / spacing));

    for (int i = 0; i <= steps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        int cx = static_cast<int>(from.x() + dx * t);
        int cy = static_cast<int>(from.y() + dy * t);

        QRect stampR = stampRect(cx, cy);
        QRect oldRect = activeDirtyRect_;
        if (activeDirtyRect_.isNull()) {
            activeDirtyRect_ = stampR;
        } else {
            activeDirtyRect_ = activeDirtyRect_.united(stampR);
        }

        if (activeDirtyRect_ != oldRect && !beforeSnapshot_.isNull()) {
            growBeforeSnapshot(oldRect, activeDirtyRect_);
        }

        drawBrushStamp(cx, cy);
    }
}

void CanvasWidgetV2::commitStroke()
{
    auto* layer = activeRasterLayer();
    if (!layer) return;

    if (activeDirtyRect_.isNull()) return;

    QImage afterSnap = captureRect(activeDirtyRect_);

    if (beforeSnapshot_.isNull() || afterSnap.isNull()) return;

    appState_->document().setModified(true);

    auto cmd = std::make_unique<PaintCommandV2>(
        layer->uid(),
        currentFrame_,
        currentLayerIndex_,
        std::move(beforeSnapshot_),
        std::move(afterSnap),
        activeDirtyRect_);

    cmd->setLayerAccess([this]() -> RasterLayer* {
        return activeRasterLayer();
    });

    appState_->document().undoManager().pushApplied(std::move(cmd));

    activeDirtyRect_ = QRect();
    beforeSnapshot_ = QImage();
    cleanLayerCopy_ = QImage();

    appState_->thumbnailCache().invalidateFrame(currentFrame_);

    emit canvasUpdated();
}

QImage CanvasWidgetV2::captureLayerRect(RasterLayer* layer, const QRect& r) const
{
    if (!layer) return QImage();
    if (r.isEmpty()) return QImage();

    QImage snap(r.width(), r.height(), QImage::Format_ARGB32_Premultiplied);
    snap.fill(0);

    int ox = layer->originX();
    int oy = layer->originY();
    int lw = layer->width();
    int lh = layer->height();

    int cx = std::max(r.x(), ox);
    int cy = std::max(r.y(), oy);
    int cw = std::min(r.right(), ox + lw) - cx;
    int ch = std::min(r.bottom(), oy + lh) - cy;
    if (cw <= 0 || ch <= 0) return snap;

    int dstX = cx - r.x();
    int dstY = cy - r.y();

    const uint32_t* src = layer->pixelData();
    for (int y = 0; y < ch; ++y) {
        int ly = (cy - oy) + y;
        int dstLine = dstY + y;
        uint32_t* dst = reinterpret_cast<uint32_t*>(snap.scanLine(dstLine)) + dstX;
        const uint32_t* row = src + static_cast<size_t>(ly) * static_cast<size_t>(lw) + (cx - ox);
        std::copy(row, row + cw, dst);
    }
    return snap;
}

QImage CanvasWidgetV2::snapFullLayer(RasterLayer* layer)
{
    if (!layer) return QImage();
    QRect fullRect(layer->originX(), layer->originY(), layer->width(), layer->height());
    return captureLayerRect(layer, fullRect);
}

void CanvasWidgetV2::pushFullLayerUndo(RasterLayer* layer, const QImage& beforeSnap)
{
    if (!layer || beforeSnap.isNull()) return;

    QRect fullRect(layer->originX(), layer->originY(), layer->width(), layer->height());
    QImage afterSnap = captureLayerRect(layer, fullRect);

    if (afterSnap.isNull()) return;

    appState_->document().setModified(true);

    auto cmd = std::make_unique<PaintCommandV2>(
        layer->uid(), currentFrame_, currentLayerIndex_,
        QImage(beforeSnap), std::move(afterSnap), fullRect);
    cmd->setLayerAccess([this]() -> RasterLayer* {
        return activeRasterLayer();
    });

    appState_->document().undoManager().pushApplied(std::move(cmd));
    appState_->thumbnailCache().invalidateFrame(currentFrame_);
}

void CanvasWidgetV2::drawShape()
{
    auto* layer = activeRasterLayer();
    if (!layer || layer->locked()) return;

    double x1 = shapeStart_.x(), y1 = shapeStart_.y();
    double x2 = shapeEnd_.x(), y2 = shapeEnd_.y();
    if (x1 == x2 && y1 == y2) return;

    layer->ensureUnique();

    int pad = brushSize_ + 2;
    int minX = static_cast<int>(std::min(x1, x2)) - pad;
    int minY = static_cast<int>(std::min(y1, y2)) - pad;
    int maxX = static_cast<int>(std::max(x1, x2)) + pad;
    int maxY = static_cast<int>(std::max(y1, y2)) + pad;

    layer->ensureContains(minX, minY, maxX - minX, maxY - minY);

    QRect dirtyRect(minX, minY, maxX - minX, maxY - minY);
    QImage before = captureLayerRect(layer, dirtyRect);

    QImage img(reinterpret_cast<const uchar*>(layer->pixelData()),
               layer->width(), layer->height(),
               layer->width() * static_cast<int>(sizeof(uint32_t)),
               QImage::Format_ARGB32_Premultiplied);
    img = img.copy();

    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);

        auto tool = appState_->toolState().activeTool();
        int lineStyle = appState_->toolState().lineStyle();

        auto drawTaperedSegment = [&](double ax, double ay, double bx, double by) -> QPainterPath {
            double dx = bx - ax;
            double dy = by - ay;
            double len = std::sqrt(dx * dx + dy * dy);
            QPainterPath seg;
            if (len < 1.0) return seg;
            double ux = dx / len;
            double uy = dy / len;
            double px = -uy;
            double py = ux;
            double mx = (ax + bx) * 0.5;
            double my = (ay + by) * 0.5;
            double thinW = brushSize_ * 0.15;
            double thickW = brushSize_ * 0.55;

            seg.moveTo(ax + px * thinW, ay + py * thinW);
            seg.lineTo(mx + px * thickW, my + py * thickW);
            seg.lineTo(bx + px * thinW, by + py * thinW);
            seg.lineTo(bx - px * thinW, by - py * thinW);
            seg.lineTo(mx - px * thickW, my - py * thickW);
            seg.lineTo(ax - px * thinW, ay - py * thinW);
            seg.closeSubpath();
            return seg;
        };

        if (lineStyle == 1) {
            p.setPen(Qt::NoPen);
            p.setBrush(brushColor_);

            if (tool == ToolType::Line) {
                QPainterPath path = drawTaperedSegment(x1, y1, x2, y2);
                p.drawPath(path);
                double thinW = brushSize_ * 0.15;
                p.drawEllipse(QPointF(x1, y1), thinW, thinW);
                p.drawEllipse(QPointF(x2, y2), thinW, thinW);
            } else if (tool == ToolType::Rectangle) {
                double minX = std::min(x1, x2);
                double maxX = std::max(x1, x2);
                double minY = std::min(y1, y2);
                double maxY = std::max(y1, y2);
                p.drawPath(drawTaperedSegment(minX, minY, maxX, minY));
                p.drawPath(drawTaperedSegment(maxX, minY, maxX, maxY));
                p.drawPath(drawTaperedSegment(maxX, maxY, minX, maxY));
                p.drawPath(drawTaperedSegment(minX, maxY, minX, minY));
            } else if (tool == ToolType::Ellipse) {
                double minX = std::min(x1, x2);
                double maxX = std::max(x1, x2);
                double minY = std::min(y1, y2);
                double maxY = std::max(y1, y2);
                double cx = (minX + maxX) * 0.5;
                double cy = (minY + maxY) * 0.5;
                double rx = (maxX - minX) * 0.5;
                double ry = (maxY - minY) * 0.5;
                int steps = 48;
                double prevX = cx + rx;
                double prevY = cy;
                for (int i = 1; i <= steps; ++i) {
                    double angle = 2.0 * 3.141592653589793 * i / steps;
                    double curX = cx + rx * std::cos(angle);
                    double curY = cy + ry * std::sin(angle);
                    p.drawPath(drawTaperedSegment(prevX, prevY, curX, curY));
                    prevX = curX;
                    prevY = curY;
                }
            }
        } else {
            Qt::PenStyle penStyle = Qt::SolidLine;
            if (lineStyle == 2) penStyle = Qt::DashLine;
            else if (lineStyle == 3) penStyle = Qt::DotLine;

            p.setPen(QPen(brushColor_, static_cast<qreal>(brushSize_),
                          penStyle, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);

            if (tool == ToolType::Line) {
                p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
            } else if (tool == ToolType::Rectangle) {
                p.drawRect(QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized());
            } else if (tool == ToolType::Ellipse) {
                p.drawEllipse(QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized());
            }
        }
    }

    int ox = layer->originX();
    int oy = layer->originY();
    int lw = layer->width();
    for (int dy = 0; dy < dirtyRect.height(); ++dy) {
        int ly = dirtyRect.y() + dy - oy;
        if (ly < 0 || ly >= layer->height()) continue;
        int lx = dirtyRect.x() - ox;
        if (lx < 0) lx = 0;
        int copyW = std::min(dirtyRect.width(), lw - lx);
        if (copyW <= 0) continue;
        const uint32_t* src = reinterpret_cast<const uint32_t*>(img.constScanLine(ly)) + lx;
        uint32_t* dst = layer->pixelData() + static_cast<size_t>(ly) * static_cast<size_t>(lw) + static_cast<size_t>(lx);
        std::copy(src, src + copyW, dst);
    }

    layer->bufferEpochTick();

    QImage after = captureLayerRect(layer, dirtyRect);

    if (before.isNull() || after.isNull()) return;

    appState_->document().setModified(true);

    auto cmd = std::make_unique<PaintCommandV2>(
        layer->uid(), currentFrame_, currentLayerIndex_,
        std::move(before), std::move(after), dirtyRect);
    cmd->setLayerAccess([this]() -> RasterLayer* {
        return activeRasterLayer();
    });

    appState_->document().undoManager().pushApplied(std::move(cmd));
    appState_->thumbnailCache().invalidateFrame(currentFrame_);
}

void CanvasWidgetV2::doFill(QPointF cpos)
{
    auto* layer = activeRasterLayer();
    if (!layer || layer->locked()) return;

    int ox = layer->originX();
    int oy = layer->originY();
    int lx = static_cast<int>(cpos.x()) - ox;
    int ly = static_cast<int>(cpos.y()) - oy;

    if (lx < 0 || lx >= layer->width() || ly < 0 || ly >= layer->height()) return;

    int fillType = appState_->toolState().fillType();

    Color targetColor = layer->pixelAt(lx, ly);
    QColor targetQC = QColor::fromRgbF(targetColor.r, targetColor.g, targetColor.b, targetColor.a);
    if (fillType == 0 && targetQC == brushColor_) return;

    layer->ensureUnique();

    QImage img(reinterpret_cast<const uchar*>(layer->pixelData()),
               layer->width(), layer->height(),
               layer->width() * static_cast<int>(sizeof(uint32_t)),
               QImage::Format_ARGB32_Premultiplied);
    img = img.copy();

    QImage before = snapFullLayer(layer);

    switch (fillType) {
    case 0: // Solid
        if (targetQC.alpha() < 20) {
            floodFillByAlpha(img, lx, ly, brushColor_, 100);
        } else {
            floodFill(img, lx, ly, brushColor_, targetQC, 32);
        }
        break;
    case 1: // Fabric
        floodFillFabric(img, lx, ly, brushColor_, targetQC);
        break;
    case 2: // Ramp
        floodFillRamp(img, lx, ly, brushColor_);
        break;
    }

    for (int y = 0; y < img.height(); ++y) {
        const uint32_t* src = reinterpret_cast<const uint32_t*>(img.constScanLine(y));
        uint32_t* dst = layer->pixelData() + static_cast<size_t>(y) * static_cast<size_t>(layer->width());
        std::copy(src, src + img.width(), dst);
    }

    layer->bufferEpochTick();

    pushFullLayerUndo(layer, before);
    emit canvasUpdated();
}

void CanvasWidgetV2::floodFillFabric(QImage& img, int sx, int sy,
                                          const QColor& baseColor, const QColor& target)
{
    int w = img.width(), h = img.height();
    int hBase = baseColor.hue();
    if (hBase < 0) hBase = 0;
    int sBase = baseColor.saturation();
    int vBase = baseColor.value();
    int aBase = baseColor.alpha();

    bool byAlpha = target.alpha() < 20;

    std::vector<uint8_t> visited(static_cast<size_t>(w) * h, 0);

    std::vector<std::pair<int, int>> stack;
    stack.reserve(4096);
    stack.push_back({sx, sy});

    while (!stack.empty()) {
        auto [px, py] = stack.back();
        stack.pop_back();
        if (px < 0 || px >= w || py < 0 || py >= h) continue;
        if (visited[static_cast<size_t>(py) * w + px]) continue;

        QColor c = img.pixelColor(px, py);
        if (byAlpha) {
            if (c.alpha() >= 100) continue;
        } else {
            if (std::abs(c.red() - target.red()) > 32 ||
                std::abs(c.green() - target.green()) > 32 ||
                std::abs(c.blue() - target.blue()) > 32 ||
                std::abs(c.alpha() - target.alpha()) > 32)
                continue;
        }

        int lx = px;
        while (lx >= 0) {
            if (visited[static_cast<size_t>(py) * w + lx]) break;
            QColor lc = img.pixelColor(lx, py);
            if (byAlpha && lc.alpha() >= 100) break;
            if (!byAlpha && std::abs(lc.red() - target.red()) > 32) break;

            visited[static_cast<size_t>(py) * w + lx] = 1;

            int noiseH = ((lx * 7 + py * 13 + 17) % 31) - 15;
            int noiseA = ((lx * 11 + py * 5 + 3) % 41) - 20;
            int h = std::clamp(hBase + noiseH / 2, 0, 359);
            int s = std::clamp(sBase - std::abs(noiseH), 0, 255);
            int a = std::clamp(aBase + noiseA, 30, 255);

            QColor wc;
            wc.setHsv(h, s, vBase, a);
            img.setPixelColor(lx, py, wc);

            if (py > 0) stack.push_back({lx, py - 1});
            if (py < h - 1) stack.push_back({lx, py + 1});
            --lx;
        }
        int rx = px + 1;
        while (rx < w) {
            if (visited[static_cast<size_t>(py) * w + rx]) break;
            QColor rc = img.pixelColor(rx, py);
            if (byAlpha && rc.alpha() >= 100) break;
            if (!byAlpha && std::abs(rc.red() - target.red()) > 32) break;

            visited[static_cast<size_t>(py) * w + rx] = 1;

            int noiseH = ((rx * 7 + py * 13 + 17) % 31) - 15;
            int noiseA = ((rx * 11 + py * 5 + 3) % 41) - 20;
            int h = std::clamp(hBase + noiseH / 2, 0, 359);
            int s = std::clamp(sBase - std::abs(noiseH), 0, 255);
            int a = std::clamp(aBase + noiseA, 30, 255);

            QColor wc;
            wc.setHsv(h, s, vBase, a);
            img.setPixelColor(rx, py, wc);

            if (py > 0) stack.push_back({rx, py - 1});
            if (py < h - 1) stack.push_back({rx, py + 1});
            ++rx;
        }
    }
}

void CanvasWidgetV2::floodFillRamp(QImage& img, int sx, int sy, const QColor& baseColor)
{
    int w = img.width(), h = img.height();
    QColor target = img.pixelColor(sx, sy);

    int hBase = baseColor.hue();
    if (hBase < 0) hBase = 0;
    int sBase = baseColor.saturation();
    int vBase = baseColor.value();
    int aBase = baseColor.alpha();

    std::vector<uint8_t> visited(static_cast<size_t>(w) * h, 0);
    std::vector<std::pair<int, int>> region;
    region.reserve(static_cast<size_t>(w) * h / 4);

    std::vector<std::pair<int, int>> stack;
    stack.reserve(4096);
    stack.push_back({sx, sy});

    while (!stack.empty()) {
        auto [px, py] = stack.back();
        stack.pop_back();
        if (px < 0 || px >= w || py < 0 || py >= h) continue;
        if (visited[static_cast<size_t>(py) * w + px]) continue;

        QColor c = img.pixelColor(px, py);
        if (std::abs(c.red() - target.red()) > 32 ||
            std::abs(c.green() - target.green()) > 32 ||
            std::abs(c.blue() - target.blue()) > 32 ||
            std::abs(c.alpha() - target.alpha()) > 32)
            continue;

        visited[static_cast<size_t>(py) * w + px] = 1;
        region.push_back({px, py});

        if (py > 0) stack.push_back({px, py - 1});
        if (py < h - 1) stack.push_back({px, py + 1});
        if (px > 0) stack.push_back({px - 1, py});
        if (px < w - 1) stack.push_back({px + 1, py});
    }

    if (region.empty()) return;

    float maxDist = 0.0f;
    for (auto [rx, ry] : region) {
        float dx = static_cast<float>(rx - sx);
        float dy = static_cast<float>(ry - sy);
        float d = std::sqrt(dx * dx + dy * dy);
        if (d > maxDist) maxDist = d;
    }
    if (maxDist < 1.0f) maxDist = 1.0f;

    float hF = static_cast<float>(hBase);
    float sF = static_cast<float>(sBase);
    float vF = static_cast<float>(vBase);
    float aF = static_cast<float>(aBase);

    for (auto [rx, ry] : region) {
        float dx = static_cast<float>(rx - sx);
        float dy = static_cast<float>(ry - sy);
        float dist = std::sqrt(dx * dx + dy * dy);
        float t = std::min(dist / maxDist, 1.0f);

        float hOut = std::fmod(hF + t * 30.0f, 360.0f);
        float sOut = sF * (1.0f - t * 0.6f);
        float vOut = vF * (1.0f + t * 0.5f);
        float aOut = aF * (1.0f - t * 0.8f);

        QColor rampC;
        rampC.setHsv(static_cast<int>(hOut),
                     static_cast<int>(std::clamp(sOut, 0.0f, 255.0f)),
                     static_cast<int>(std::clamp(vOut, 0.0f, 255.0f)),
                     static_cast<int>(std::clamp(aOut, 0.0f, 255.0f)));
        img.setPixelColor(rx, ry, rampC);
    }
}

void CanvasWidgetV2::floodFill(QImage& img, int sx, int sy,
                                const QColor& fillColor, const QColor& target, int tolerance)
{
    int w = img.width(), h = img.height();
    std::vector<std::pair<int, int>> stack;
    stack.reserve(4096);
    stack.push_back({sx, sy});

    while (!stack.empty()) {
        auto [px, py] = stack.back();
        stack.pop_back();
        if (px < 0 || px >= w || py < 0 || py >= h) continue;

        QColor c = img.pixelColor(px, py);
        if (std::abs(c.red() - target.red()) > tolerance ||
            std::abs(c.green() - target.green()) > tolerance ||
            std::abs(c.blue() - target.blue()) > tolerance ||
            std::abs(c.alpha() - target.alpha()) > tolerance)
            continue;
        if (c == fillColor) continue;

        int lx = px;
        while (lx >= 0) {
            QColor lc = img.pixelColor(lx, py);
            if (std::abs(lc.red() - target.red()) > tolerance) break;
            img.setPixelColor(lx, py, fillColor);
            if (py > 0) stack.push_back({lx, py - 1});
            if (py < h - 1) stack.push_back({lx, py + 1});
            --lx;
        }
        int rx = px + 1;
        while (rx < w) {
            QColor rc = img.pixelColor(rx, py);
            if (std::abs(rc.red() - target.red()) > tolerance) break;
            img.setPixelColor(rx, py, fillColor);
            if (py > 0) stack.push_back({rx, py - 1});
            if (py < h - 1) stack.push_back({rx, py + 1});
            ++rx;
        }
    }
}

void CanvasWidgetV2::floodFillByAlpha(QImage& img, int sx, int sy,
                                       const QColor& fillColor, int boundaryAlpha)
{
    int w = img.width(), h = img.height();
    std::vector<std::pair<int, int>> stack;
    stack.reserve(4096);
    stack.push_back({sx, sy});

    while (!stack.empty()) {
        auto [px, py] = stack.back();
        stack.pop_back();
        if (px < 0 || px >= w || py < 0 || py >= h) continue;

        QColor c = img.pixelColor(px, py);
        if (c.alpha() >= boundaryAlpha) continue;
        if (c == fillColor) continue;

        int lx = px;
        while (lx >= 0) {
            QColor lc = img.pixelColor(lx, py);
            if (lc.alpha() >= boundaryAlpha) break;
            img.setPixelColor(lx, py, fillColor);
            if (py > 0) stack.push_back({lx, py - 1});
            if (py < h - 1) stack.push_back({lx, py + 1});
            --lx;
        }
        int rx = px + 1;
        while (rx < w) {
            QColor rc = img.pixelColor(rx, py);
            if (rc.alpha() >= boundaryAlpha) break;
            img.setPixelColor(rx, py, fillColor);
            if (py > 0) stack.push_back({rx, py - 1});
            if (py < h - 1) stack.push_back({rx, py + 1});
            ++rx;
        }
    }
}

void CanvasWidgetV2::doText(QPointF cpos)
{
    auto* layer = activeRasterLayer();
    if (!layer || layer->locked()) return;

    bool ok = false;
    QString text = QInputDialog::getMultiLineText(this, "Add Text", "Enter text:", "", &ok);
    if (!ok || text.isEmpty()) return;

    bool fontOk = false;
    QFont chosen = QFontDialog::getFont(&fontOk, textFont_, this, "Choose Font");
    if (fontOk) textFont_ = chosen;

    layer->ensureUnique();

    QImage before = snapFullLayer(layer);

    QImage img(reinterpret_cast<const uchar*>(layer->pixelData()),
               layer->width(), layer->height(),
               layer->width() * static_cast<int>(sizeof(uint32_t)),
               QImage::Format_ARGB32_Premultiplied);
    img = img.copy();

    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setFont(textFont_);
        p.setPen(brushColor_);
        p.drawText(cpos, text);
    }

    for (int y = 0; y < img.height(); ++y) {
        const uint32_t* src = reinterpret_cast<const uint32_t*>(img.constScanLine(y));
        uint32_t* dst = layer->pixelData() + static_cast<size_t>(y) * static_cast<size_t>(layer->width());
        std::copy(src, src + img.width(), dst);
    }

    layer->bufferEpochTick();

    pushFullLayerUndo(layer, before);
    emit canvasUpdated();
}

void CanvasWidgetV2::startMove(QPointF cpos)
{
    auto* layer = activeRasterLayer();
    if (!layer || layer->locked()) return;

    moveStart_ = cpos;
    moveOffset_ = QPointF(0, 0);

    if (floatingActive_ && !floatingSelection_.isNull() && selRect_.isValid()) {
        moveImage_ = floatingSelection_;
        moveSrcRect_ = selRect_;
    } else {
        moveImage_ = snapFullLayer(layer);
        int mx = layer->originX();
        int my = layer->originY();
        moveSrcRect_ = QRectF(mx, my, layer->width(), layer->height());
    }
    moving_ = true;
}

void CanvasWidgetV2::updateMove(QPointF cpos)
{
    if (!moving_) return;
    moveOffset_ = QPointF(cpos.x() - moveStart_.x(), cpos.y() - moveStart_.y());
    update();
}

void CanvasWidgetV2::commitMove()
{
    if (!moving_) return;
    moving_ = false;

    auto* layer = activeRasterLayer();
    if (!layer) return;

    constexpr double kMaxMoveDelta = 40000.0;
    double clampedDx = std::clamp(moveOffset_.x(), -kMaxMoveDelta, kMaxMoveDelta);
    double clampedDy = std::clamp(moveOffset_.y(), -kMaxMoveDelta, kMaxMoveDelta);

    if (std::abs(clampedDx) < 0.5 && std::abs(clampedDy) < 0.5) {
        moveImage_ = QImage();
        if (floatingActive_) {
            commitFloatingSelection();
        }
        return;
    }

    if (floatingActive_ && !floatingSelection_.isNull()) {
        selRect_.translate(clampedDx, clampedDy);
        moveImage_ = QImage();
        update();
        return;
    }

    layer->ensureUnique();

    int lw = layer->width();
    int lh = layer->height();

    QImage before = snapFullLayer(layer);

    std::vector<uint32_t> newPixels(static_cast<size_t>(lw) * static_cast<size_t>(lh), 0);

    for (int y = 0; y < lh; ++y) {
        for (int x = 0; x < lw; ++x) {
            int srcX = x - static_cast<int>(clampedDx);
            int srcY = y - static_cast<int>(clampedDy);
            if (srcX >= 0 && srcX < lw && srcY >= 0 && srcY < lh) {
                size_t srcIdx = static_cast<size_t>(srcY) * static_cast<size_t>(lw) + static_cast<size_t>(srcX);
                size_t dstIdx = static_cast<size_t>(y) * static_cast<size_t>(lw) + static_cast<size_t>(x);
                newPixels[dstIdx] = layer->pixelData()[srcIdx];
            }
        }
    }

    std::copy(newPixels.begin(), newPixels.end(), layer->pixelData());
    layer->bufferEpochTick();

    moveOffset_ = QPointF(0, 0);
    moveImage_ = QImage();

    pushFullLayerUndo(layer, before);
    emit canvasUpdated();
}

void CanvasWidgetV2::commitSelection()
{
    if (!selRect_.isValid() || selRect_.width() < 3 || selRect_.height() < 3) {
        selRect_ = QRectF();
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        update();
        return;
    }

    auto* layer = activeRasterLayer();
    if (!layer || layer->locked()) return;

    layer->ensureUnique();

    QImage img(reinterpret_cast<const uchar*>(layer->pixelData()),
               layer->width(), layer->height(),
               layer->width() * static_cast<int>(sizeof(uint32_t)),
               QImage::Format_ARGB32_Premultiplied);
    img = img.copy();

    int ox = layer->originX();
    int oy = layer->originY();
    QRect bufRect(
        static_cast<int>(selRect_.x() - ox),
        static_cast<int>(selRect_.y() - oy),
        static_cast<int>(selRect_.width()),
        static_cast<int>(selRect_.height()));

    bufRect = bufRect.intersected(QRect(0, 0, layer->width(), layer->height()));
    if (bufRect.isEmpty()) return;

    floatingSelection_ = img.copy(bufRect);
    originalFloatingSelection_ = floatingSelection_;
    selImage_ = img.copy(bufRect);

    QImage before = snapFullLayer(layer);

    {
        QPainter cp(&img);
        cp.setCompositionMode(QPainter::CompositionMode_Clear);
        cp.fillRect(bufRect, Qt::transparent);
    }

    for (int y = 0; y < img.height(); ++y) {
        const uint32_t* src = reinterpret_cast<const uint32_t*>(img.constScanLine(y));
        uint32_t* dst = layer->pixelData() + static_cast<size_t>(y) * static_cast<size_t>(layer->width());
        std::copy(src, src + img.width(), dst);
    }

    layer->bufferEpochTick();

    floatingActive_ = true;
    marchingTimer_.start();

    pushFullLayerUndo(layer, before);
    emit canvasUpdated();
}

void CanvasWidgetV2::commitFloatingSelection()
{
    if (!floatingActive_ || floatingSelection_.isNull()) {
        floatingActive_ = false;
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        selRect_ = QRectF();
        update();
        return;
    }

    auto* layer = activeRasterLayer();
    if (!layer || layer->locked()) return;
    if (layer->type() != LayerType::Raster) return;

    layer->ensureUnique();

    int ox = layer->originX();
    int oy = layer->originY();

    int ix = static_cast<int>(selRect_.x() - ox);
    int iy = static_cast<int>(selRect_.y() - oy);
    int iw = floatingSelection_.width();
    int ih = floatingSelection_.height();

    layer->ensureContains(ix, iy, iw, ih);

    ox = layer->originX();
    oy = layer->originY();
    ix = static_cast<int>(selRect_.x() - ox);
    iy = static_cast<int>(selRect_.y() - oy);

    QImage before = snapFullLayer(layer);

    QImage img(reinterpret_cast<const uchar*>(layer->pixelData()),
               layer->width(), layer->height(),
               layer->width() * static_cast<int>(sizeof(uint32_t)),
               QImage::Format_ARGB32_Premultiplied);
    img = img.copy();

    {
        QPainter layerPainter(&img);
        layerPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        layerPainter.setRenderHint(QPainter::SmoothPixmapTransform);
        layerPainter.drawImage(ix, iy, floatingSelection_);
    }

    for (int y = 0; y < img.height(); ++y) {
        const uint32_t* src = reinterpret_cast<const uint32_t*>(img.constScanLine(y));
        uint32_t* dst = layer->pixelData() + static_cast<size_t>(y) * static_cast<size_t>(layer->width());
        std::copy(src, src + img.width(), dst);
    }

    layer->bufferEpochTick();

    floatingActive_ = false;
    floatingSelection_ = QImage();
    originalFloatingSelection_ = QImage();
    selImage_ = QImage();
    selRect_ = QRectF();
    selState_ = SelectionState::None;

    pushFullLayerUndo(layer, before);
    emit canvasUpdated();
}

} // namespace fap
