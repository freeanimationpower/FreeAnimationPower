#include "canvas_widget_v2.hpp"

#include <QtWidgets/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QPainterPath>
#include <cmath>
#include <algorithm>
#include <functional>
#include <cstring>

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
        painter->setOpacity(layer->opacity());
        painter->setCompositionMode(toQtCompositionMode(layer->blendMode()));
        painter->drawImage(QPoint(rl.originX(), rl.originY()), img);
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
            p.drawImage(QPoint(rl.originX(), rl.originY()), tinted);
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
        p.setOpacity(layer->opacity());
        p.setCompositionMode(toQtCompositionMode(layer->blendMode()));
        p.drawImage(QPoint(rl.originX(), rl.originY()), img);
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
        if (tool == ToolType::Brush || tool == ToolType::Eraser) {
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
    auto* layer = activeRasterLayer();
    if (!layer) return QImage();
    if (r.isEmpty()) return QImage();

    int ox = layer->originX();
    int oy = layer->originY();
    int lw = layer->width();
    int lh = layer->height();

    int cx = std::max(r.x(), ox);
    int cy = std::max(r.y(), oy);
    int cw = std::min(r.right(), ox + lw) - cx;
    int ch = std::min(r.bottom(), oy + lh) - cy;
    if (cw <= 0 || ch <= 0) return QImage();

    QImage snap(cw, ch, QImage::Format_ARGB32_Premultiplied);
    const uint32_t* src = layer->pixelData();
    for (int y = 0; y < ch; ++y) {
        int ly = (cy - oy) + y;
        uint32_t* dst = reinterpret_cast<uint32_t*>(snap.scanLine(y));
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

    for (int y = 0; y < grown.height(); ++y) {
        uint32_t* drow = reinterpret_cast<uint32_t*>(grown.scanLine(y));
        for (int x = 0; x < grown.width(); ++x) {
            bool inOld = (x >= ox && x < ox + oldRect.width() &&
                          y >= oy && y < oy + oldRect.height());
            if (inOld) continue;
            int lx = newRect.x() + x;
            int ly = newRect.y() + y;
            auto* layer = activeRasterLayer();
            if (!layer) continue;
            int lilx = lx - layer->originX();
            int lily = ly - layer->originY();
            if (lilx >= 0 && lilx < layer->width() && lily >= 0 && lily < layer->height()) {
                const uint32_t* src = layer->pixelData() +
                    static_cast<size_t>(lily) * static_cast<size_t>(layer->width()) + lilx;
                drow[x] = *src;
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

        activeDirtyRect_ = stampRect(cx, cy);
        beforeSnapshot_ = captureRect(activeDirtyRect_);

        drawBrushStamp(cx, cy);
    } else if (tool == ToolType::ColorPicker) {
        QPointF cp = widgetToCanvas(event->pos());
        auto* layer = activeRasterLayer();
        if (layer) {
            int lx = static_cast<int>(cp.x()) - layer->originX();
            int ly = static_cast<int>(cp.y()) - layer->originY();
            if (lx >= 0 && lx < layer->width() && ly >= 0 && ly < layer->height()) {
                Color c = layer->pixelAt(lx, ly);
                QColor qc = QColor::fromRgbF(c.r, c.g, c.b, c.a);
                brushColor_ = qc;
                appState_->toolState().setPrimaryColor(qc);
                emit colorPicked(qc);
            }
        }
    } else if (tool == ToolType::Hand) {
        lastMousePos_ = event->pos();
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
        auto* layer = activeRasterLayer();
        if (layer) {
            int lx = static_cast<int>(cp.x()) - layer->originX();
            int ly = static_cast<int>(cp.y()) - layer->originY();
            if (lx >= 0 && lx < layer->width() && ly >= 0 && ly < layer->height()) {
                Color c = layer->pixelAt(lx, ly);
                QColor qc = QColor::fromRgbF(c.r, c.g, c.b, c.a);
                brushColor_ = qc;
                appState_->toolState().setPrimaryColor(qc);
                emit colorPicked(qc);
            }
        }
    }
}

void CanvasWidgetV2::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;

    if (drawing_) {
        drawing_ = false;
        commitStroke();
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

    appState_->document().undoManager().pushCommand(std::move(cmd));

    activeDirtyRect_ = QRect();
    beforeSnapshot_ = QImage();

    appState_->thumbnailCache().invalidateFrame(currentFrame_);

    emit canvasUpdated();
}

} // namespace fap
