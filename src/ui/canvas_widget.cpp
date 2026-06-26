#include "canvas_widget.hpp"
#include "core/document.hpp"
#include "core/layer.hpp"
#include "core/undo_manager.hpp"
#include "ui/layer_panel.hpp"
#include "ui/undo_commands.hpp"
#include "engine/brush/brush_engine.hpp"
#include <QtWidgets/QApplication>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QFontDialog>
#include <QtWidgets/QLineEdit>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QTabletEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QShowEvent>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QPen>
#include <QtGui/QClipboard>
#include <cmath>
#include <algorithm>

namespace fap {

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

CanvasWidget::CanvasWidget(Document* doc, BrushEngine* brush, QWidget* parent)
    : QWidget(parent)
    , doc_(doc)
    , brush_(brush)
    , floatingActive_(false)
    , floatingSelection_()
    , originalFloatingSelection_()
    , selRect_()
    , selImage_()
    , selState_(SelectionState::None)
    , selHandleIdx_(-1)
{
    setMinimumSize(400, 300);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TabletTracking, true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(false);
    setStyleSheet("background-color: #1e1e1e;");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::CrossCursor);
    font_ = QFont("Arial", 24);
}

Layer* CanvasWidget::resolveCurrentLayer() const {
    if (!doc_) return nullptr;
    auto& layers = currentRootLayer().layers();
    if (currentLayerUid_ != 0) {
        Layer* found = currentRootLayer().layerByUid(currentLayerUid_);
        if (found) return found;
    }
    if (currentLayerIdx_ >= 0 && static_cast<size_t>(currentLayerIdx_) < layers.size())
        return layers[currentLayerIdx_].get();
    return nullptr;
}

GroupLayer& CanvasWidget::currentRootLayer() const {
    return doc_->rootLayerForFrame(currentFrame_);
}

GroupLayer& CanvasWidget::currentRootLayer(int frame) const {
    return doc_->rootLayerForFrame(frame);
}

void CanvasWidget::resizeEvent(QResizeEvent*) { fit(); }
void CanvasWidget::showEvent(QShowEvent*) { fit(); }

void CanvasWidget::fit() {
    if (width() == 0 || height() == 0 || !doc_) return;
    double pad = 1.0;
    double zw = static_cast<double>(width() - pad * 2) / doc_->width();
    double zh = static_cast<double>(height() - pad * 2) / doc_->height();
    zoom_ = std::min(zw, zh);
    offX_ = std::round((width() - doc_->width() * zoom_) / 2.0);
    offY_ = std::round((height() - doc_->height() * zoom_) / 2.0);
    update();
}

void CanvasWidget::setTool(int tool) {
    if (floatingActive_ && !originalFloatingSelection_.isNull() && selRect_.isValid() && tool != 9 && tool != 8) {
        commitFloatingSelection();
    } else if (floatingActive_) {
        floatingActive_ = false;
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        selImage_ = QImage();
        selRect_ = QRectF();
    }

    tool_ = tool;
    selState_ = SelectionState::None;
    selHandleIdx_ = -1;

    if (tool != 9 && tool != 8) {
        originalFloatingSelection_ = QImage();
        floatingSelection_ = QImage();
        selImage_ = QImage();
        selRect_ = QRectF();
        floatingActive_ = false;
    }

    transformActive_ = false;
    activeHandle_ = -1;

    if (tool == 11) {
        scaleX_ = 1.0; scaleY_ = 1.0; translate_ = QPointF(0, 0);
        transformActive_ = true;
        ensureTransformBounds();
        setCursor(Qt::CrossCursor);
    } else if (tool == 10) setCursor(Qt::OpenHandCursor);
    else if (tool == 2) setCursor(Qt::CrossCursor);
    else if (tool == 8) setCursor(Qt::ArrowCursor);
    else if (tool == 4) setCursor(Qt::IBeamCursor);
    else if (tool == 9) setCursor(Qt::CrossCursor);
    else setCursor(Qt::CrossCursor);

    dumpState("Fin setTool");
    update();
}

void CanvasWidget::setColor(const QColor& c) {
    color_ = c;
    if (brush_) { BrushPreset p = brush_->preset(); p.color = Color{c.redF(), c.greenF(), c.blueF(), c.alphaF()}; brush_->setPreset(p); }
}

QPointF CanvasWidget::screenToCanvas(QPointF sp) const {
    return (sp - QPointF(offX_, offY_)) / zoom_;
}

QPointF CanvasWidget::canvasToScreen(QPointF cp) const {
    return cp * zoom_ + QPointF(offX_, offY_);
}

void CanvasWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(100, 100, 100));
    if (!doc_) return;

    double cw = doc_->width(), ch = doc_->height();
    int sw = static_cast<int>(std::ceil(cw * zoom_));
    int sh = static_cast<int>(std::ceil(ch * zoom_));
    double ox = std::round(offX_);
    double oy = std::round(offY_);
    QRectF screenRect(ox, oy, sw, sh);

    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(screenRect, Qt::white);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.save();
    p.translate(ox, oy);
    p.scale(zoom_, zoom_);

    if (flipH_) {
        p.translate(cw, 0);
        p.scale(-1.0, 1.0);
    }
    if (rotation_ != 0) {
        p.translate(cw * 0.5, ch * 0.5);
        p.rotate(static_cast<double>(rotation_));
        p.translate(-cw * 0.5, -ch * 0.5);
    }

    QRectF canvasRect(0, 0, cw, ch);
    if (showGrid_) paintGrid(p, canvasRect);
    paintOnionSkin(p);

    const GroupLayer& root = currentRootLayer();
    auto& layers = root.layers();
    for (auto it = layers.begin(); it != layers.end(); ++it) {
        Layer* layer = it->get();
        if (!layer->visible()) continue;

        p.setOpacity(layer->opacity());
        p.setCompositionMode(toQtCompositionMode(layer->blendMode()));

        if (layer->type() == LayerType::Raster) {
            RasterLayer* rasterLayer = static_cast<RasterLayer*>(layer);
            QImage layerImage = wrapRasterLayer(rasterLayer);
            if (!layerImage.isNull()) {
                qreal rx = std::round(static_cast<qreal>(rasterLayer->originX()));
                qreal ry = std::round(static_cast<qreal>(rasterLayer->originY()));
                QRectF targetRect(rx, ry, layerImage.width(), layerImage.height());
                QRectF sourceRect(0, 0, layerImage.width(), layerImage.height());
                bool needsSmoothing = (zoom_ != 1.0);
                if (needsSmoothing) {
                    QTransform wt = p.worldTransform();
                    qreal devX = wt.m11() * rx + wt.m21() * ry + wt.dx();
                    qreal devY = wt.m12() * rx + wt.m22() * ry + wt.dy();
                    qreal snapDx = wt.dx() + (std::round(devX) - devX);
                    qreal snapDy = wt.dy() + (std::round(devY) - devY);
                    QTransform snapped(wt.m11(), wt.m12(), wt.m13(),
                                       wt.m21(), wt.m22(), wt.m23(),
                                       snapDx, snapDy, wt.m33());
                    p.setWorldTransform(snapped, false);
                    p.setRenderHint(QPainter::Antialiasing, false);
                    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                    p.drawImage(targetRect, layerImage, sourceRect);
                    p.setWorldTransform(wt, false);
                    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
                    p.setRenderHint(QPainter::Antialiasing, true);
                } else {
                    p.drawImage(targetRect, layerImage, sourceRect);
                }
            }
            }
        } else if (layer->type() == LayerType::Vector) {
            renderVectorStrokes(p, currentFrame_, layer->uid());
        }
    }
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setOpacity(1.0);

    if (drawing_ && !strokePoints_.empty()) paintLiveStroke(p);
    if (drawing_ && shapeStart_.x() >= 0 && shapeEnd_.x() >= 0) paintShapePreview(p);

    if (selState_ == SelectionState::Creating && selRect_.isValid()) {
        p.setPen(QPen(QColor(0, 200, 255), 3.0 / zoom_, Qt::SolidLine));
        p.setBrush(QColor(0, 180, 255, 50));
        p.drawRect(selRect_);
    }

    if (tool_ == 9 && floatingActive_ && !originalFloatingSelection_.isNull() && selRect_.isValid()) {
        p.save();
        p.setClipping(false);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        QImage scaled = originalFloatingSelection_.scaled(
            selRect_.size().toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        p.drawImage(selRect_.topLeft(), scaled);
        p.restore();

        qreal dashLen = 6.0 / zoom_;
        qreal offset = static_cast<qreal>(marchingTimer_.elapsed()) * 0.03;
        QPen whitePen(Qt::white, 1.5 / zoom_, Qt::DashLine);
        whitePen.setDashPattern({dashLen, dashLen});
        whitePen.setDashOffset(offset);
        QPen blackPen(Qt::black, 1.5 / zoom_, Qt::DashLine);
        blackPen.setDashPattern({dashLen, dashLen});
        blackPen.setDashOffset(offset + dashLen);

        p.setBrush(Qt::NoBrush);
        p.setPen(whitePen);
        p.drawRect(selRect_);
        p.setPen(blackPen);
        p.drawRect(selRect_);

        qreal hs = 5.0 / zoom_;
        QPointF c[4] = {selRect_.topLeft(), selRect_.topRight(),
                        selRect_.bottomRight(), selRect_.bottomLeft()};
        QPointF h[8] = {
            c[0], QPointF((c[0].x()+c[1].x())*0.5, c[0].y()),
            c[1], QPointF(c[1].x(), (c[1].y()+c[2].y())*0.5),
            c[2], QPointF((c[2].x()+c[3].x())*0.5, c[2].y()),
            c[3], QPointF(c[0].x(), (c[0].y()+c[3].y())*0.5),
        };
        p.setPen(QPen(Qt::white, 1.0 / zoom_));
        p.setBrush(QColor(50, 120, 220));
        for (int i = 0; i < 8; ++i)
            p.drawRect(QRectF(h[i].x() - hs, h[i].y() - hs, hs * 2, hs * 2));
    } else if (tool_ == 9 && (floatingActive_ || (selState_ == SelectionState::None && selRect_.isValid() && !selImage_.isNull()))) {
        p.setPen(QPen(QColor(0, 200, 255), 2.5 / zoom_, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(selRect_);
    }

    if (moving_ && moveImage_.isNull() == false) {
        p.setOpacity(0.7);
        p.drawImage(moveSrcRect_.topLeft() + moveOffset_, moveImage_);
        p.setOpacity(1.0);
    }

    if (tool_ == 11 && transformActive_) {
        ensureTransformBounds();
        QImage src;
        if (floatingActive_ && !floatingSelection_.isNull())
            src = floatingSelection_;
        else
            src = readLayerPixels(resolveCurrentLayer());

        QPointF c = transformBounds_.center();
        QTransform t;
        t.translate(c.x() + translate_.x(), c.y() + translate_.y());
        t.scale(scaleX_, scaleY_);
        t.translate(-c.x(), -c.y());

        p.save();
        p.setOpacity(0.5);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.setTransform(t, true);
        p.drawImage(QPointF(0, 0), src);
        p.restore();

        p.setBrush(Qt::NoBrush);

        QPointF corners[4] = {
            transformBounds_.topLeft(), transformBounds_.topRight(),
            transformBounds_.bottomRight(), transformBounds_.bottomLeft()
        };
        auto tx = [&](QPointF pt) {
            return QPointF((pt.x() - c.x()) * scaleX_ + c.x() + translate_.x(),
                           (pt.y() - c.y()) * scaleY_ + c.y() + translate_.y());
        };
        QPointF hc[8] = {
            tx(corners[0]),
            tx(QPointF((corners[0].x()+corners[1].x())*0.5, corners[0].y())),
            tx(corners[1]),
            tx(QPointF(corners[1].x(), (corners[1].y()+corners[2].y())*0.5)),
            tx(corners[2]),
            tx(QPointF((corners[2].x()+corners[3].x())*0.5, corners[2].y())),
            tx(corners[3]),
            tx(QPointF(corners[0].x(), (corners[0].y()+corners[3].y())*0.5)),
        };

        p.setPen(QPen(QColor(255, 255, 255), 1.5 / zoom_, Qt::SolidLine));
        for (int i = 0; i < 4; ++i)
            p.drawLine(hc[i * 2], hc[(i * 2 + 2) % 8]);
        p.drawLine(hc[0], hc[6]);
        p.drawLine(hc[2], hc[4]);

        qreal hsz = 5.0 / zoom_;
        p.setPen(QPen(Qt::white, 1.0 / zoom_));
        p.setBrush(QColor(60, 60, 60));
        for (int i = 0; i < 8; ++i)
            p.drawRect(QRectF(hc[i].x() - hsz, hc[i].y() - hsz, hsz * 2, hsz * 2));
    }

    p.restore();

    QColor shadowColor(0, 0, 0, 50);
    p.setPen(Qt::NoPen);
    p.setBrush(shadowColor);
    p.drawRect(screenRect.adjusted(1, 1, 1, 1));
    p.setPen(QPen(QColor(120, 120, 120), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRect(screenRect);

    if (!drawing_ && !panning_ && (tool_ == 0 || tool_ == 1)) {
        QPointF cp = screenToCanvas(cursorPos_);
        double s = (brush_ ? brush_->preset().size : 10.0);
        QPointF sc = canvasToScreen(cp);
        p.setPen(QPen(QColor(255, 255, 255, 160), 1.5));
        p.setBrush(QColor(180, 180, 180, 30));
        p.drawEllipse(sc, s * zoom_ / 2, s * zoom_ / 2);
    }
    paintInfoOverlay(p);
    p.end();

    if (tool_ == 9 && floatingActive_)
        update();
}

void CanvasWidget::paintCheckerboard(QPainter& p, const QRectF& rect) {
    int cs = 16;
    QColor light(195, 195, 195), dark(165, 165, 165);
    int x0 = static_cast<int>(rect.left()) / cs * cs;
    int y0 = static_cast<int>(rect.top()) / cs * cs;
    int x1 = static_cast<int>(std::ceil(rect.right()));
    int y1 = static_cast<int>(std::ceil(rect.bottom()));
    int maxX = x1, maxY = y1;
    for (int y = y0; y < y1; y += cs) {
        for (int x = x0; x < x1; x += cs) {
            int bw = std::min(cs, maxX - x);
            int bh = std::min(cs, maxY - y);
            if (bw > 0 && bh > 0)
                p.fillRect(QRectF(x, y, bw, bh), ((x / cs + y / cs) % 2 == 0) ? light : dark);
        }
    }
}

void CanvasWidget::paintGrid(QPainter& p, const QRectF& rect) {
    int gs = gridSize_;
    p.setPen(QPen(QColor(80, 180, 80, 140), 1.0));
    for (int x = gs * (static_cast<int>(rect.left()) / gs); x < static_cast<int>(rect.right()); x += gs)
        p.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    for (int y = gs * (static_cast<int>(rect.top()) / gs); y < static_cast<int>(rect.bottom()); y += gs)
        p.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
}

void CanvasWidget::paintOnionSkin(QPainter& p) {
    if (!onionEnabled_) return;
    if (onionPrev_ <= 0 && onionNext_ <= 0) return;
    if (!doc_) return;
    int cur = currentFrame_;
    int totalFrames = doc_->totalFrames();
    for (int i = 1; i <= onionPrev_; ++i) {
        int fi = cur - i; if (fi < 0) break;
        if (!doc_->peekRootLayerForFrame(fi)) continue;
        int alpha = static_cast<int>(255 * onionOpacity_ * (1.0 - i / (onionPrev_ + 1.5)));
        renderTintedFrame(p, fi, QColor(255, 60, 60, alpha));
    }
    for (int i = 1; i <= onionNext_; ++i) {
        int fi = cur + i; if (fi >= totalFrames) break;
        if (!doc_->peekRootLayerForFrame(fi)) continue;
        int alpha = static_cast<int>(255 * onionOpacity_ * (1.0 - i / (onionNext_ + 1.5)));
        renderTintedFrame(p, fi, QColor(60, 100, 255, alpha));
    }
}

void CanvasWidget::renderTintedFrame(QPainter& p, int frameIdx, QColor tint) {
    if (!doc_) return;
    const GroupLayer* root = doc_->peekRootLayerForFrame(frameIdx);
    if (!root) return;
    QImage combined(doc_->width(), doc_->height(), QImage::Format_ARGB32_Premultiplied);
    combined.fill(Qt::transparent);
    QPainter cp(&combined);
    auto& layers = root->layers();
    for (auto it = layers.begin(); it != layers.end(); ++it) {
        Layer* layer = it->get();
        if (!layer->visible()) continue;
        if (layer->type() != LayerType::Raster) continue;
        RasterLayer* rasterLayer = static_cast<RasterLayer*>(layer);
        QImage img = wrapRasterLayer(rasterLayer);
        if (!img.isNull()) { cp.setOpacity(layer->opacity()); cp.drawImage(rasterLayer->originX(), rasterLayer->originY(), img); }
    }
    cp.end();
    QImage tinted(combined.size(), QImage::Format_ARGB32_Premultiplied);
    tinted.fill(tint);
    QPainter tp(&tinted);
    tp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    tp.drawImage(0, 0, combined);
    tp.end();
    p.drawImage(0, 0, tinted);
}

void CanvasWidget::paintLiveStroke(QPainter& p) {
    if (strokePoints_.empty()) return;
    float sz = brush_ ? brush_->preset().size : 10.0f;

    if (tool_ == 1) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 180));
        for (const auto& pt : strokePoints_)
            p.drawEllipse(QPointF(pt.position.x, pt.position.y), sz * 0.5, sz * 0.5);
        return;
    }

    if (strokePoints_.size() < 2) {
        float r = sz * 0.5f;
        float pad = r + 2.0f;
        int s = static_cast<int>(std::ceil(pad * 2));
        QImage stamp(s, s, QImage::Format_ARGB32_Premultiplied);
        stamp.fill(Qt::transparent);
        {
            QPainter sp(&stamp);
            sp.setRenderHint(QPainter::Antialiasing);
            float opacity = brush_ ? brush_->preset().opacity : 1.0f;
            QColor c(color_);
            c.setAlphaF(c.alphaF() * opacity);
            sp.setPen(Qt::NoPen);
            sp.setBrush(c);
            sp.drawEllipse(QPointF(pad, pad), r, r);
        }
        p.save();
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(QRectF(strokePoints_[0].position.x - pad,
                           strokePoints_[0].position.y - pad,
                           s, s), stamp);
        p.restore();
        return;
    }

    float minX = strokePoints_[0].position.x, maxX = minX;
    float minY = strokePoints_[0].position.y, maxY = minY;
    for (const auto& pt : strokePoints_) {
        minX = std::min(minX, pt.position.x);
        maxX = std::max(maxX, pt.position.x);
        minY = std::min(minY, pt.position.y);
        maxY = std::max(maxY, pt.position.y);
    }

    float padding = sz * 1.5f;
    int ix = static_cast<int>(std::floor(minX - padding));
    int iy = static_cast<int>(std::floor(minY - padding));
    int iw = static_cast<int>(std::ceil(maxX - minX + padding * 2));
    int ih = static_cast<int>(std::ceil(maxY - minY + padding * 2));
    if (iw < 2 || ih < 2) return;

    QImage offscreen(iw, ih, QImage::Format_ARGB32_Premultiplied);
    offscreen.fill(Qt::transparent);

    {
        QPainter op(&offscreen);
        op.setRenderHint(QPainter::Antialiasing);
        op.translate(-ix, -iy);

        float opacity = brush_ ? brush_->preset().opacity : 1.0f;
        QColor c(color_);
        c.setAlphaF(c.alphaF() * opacity);

        QPainterPath path;
        path.moveTo(strokePoints_[0].position.x, strokePoints_[0].position.y);
        for (size_t i = 1; i < strokePoints_.size(); ++i)
            path.lineTo(strokePoints_[i].position.x, strokePoints_[i].position.y);
        QPen pen(c, sz, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        op.setPen(pen);
        op.drawPath(path);
    }

    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(ix, iy, offscreen);
    p.restore();
}

void CanvasWidget::paintShapePreview(QPainter& p) {
    p.setPen(QPen(QColor(60, 140, 255, 180), 2.0 / zoom_, Qt::DashLine));
    double x1=shapeStart_.x(), y1=shapeStart_.y(), x2=shapeEnd_.x(), y2=shapeEnd_.y();
    if (tool_==5) p.drawLine(QPointF(x1,y1), QPointF(x2,y2));
    else if (tool_==6) p.drawRect(QRectF(QPointF(x1,y1), QPointF(x2,y2)).normalized());
    else if (tool_==7) p.drawEllipse(QRectF(QPointF(x1,y1), QPointF(x2,y2)).normalized());
}

void CanvasWidget::paintInfoOverlay(QPainter& p) {
    p.save(); p.setFont(QFont("Consolas", 9));
    QPointF cp = screenToCanvas(cursorPos_);
    QString text = QString("XY: %1, %2  |  Zoom: %3%  |  Frame: %4/%5")
        .arg(static_cast<int>(cp.x())).arg(static_cast<int>(cp.y()))
        .arg(static_cast<int>(zoom_ * 100))
        .arg(currentFrame_ + 1).arg(totalFrames_);
    if (rotation_ != 0) text += QString("  |  Rot: %1deg").arg(rotation_);
    if (flipH_) text += "  |  FLIPPED";
    QFontMetrics fm = p.fontMetrics();
    int tw = fm.horizontalAdvance(text) + 16, th = fm.height() + 8;
    p.fillRect(QRectF(width() - tw - 8, height() - th - 8, tw, th), QColor(0, 0, 0, 140));
    p.setPen(QColor(200, 200, 200));
    p.drawText(QRectF(width() - tw, height() - th, tw, th), text);
    p.restore();
}

QImage CanvasWidget::wrapRasterLayer(RasterLayer* rasterLayer) {
    if (!rasterLayer) return QImage();
    auto buffer = rasterLayer->pixelBuffer();
    if (!buffer || buffer->pixels.empty()) return QImage();
    int w = rasterLayer->width();
    int h = rasterLayer->height();
    if (w <= 0 || h <= 0) return QImage();
    uint64_t epoch = rasterLayer->bufferEpoch();
    LayerUid uid = rasterLayer->uid();
    auto it = rasterEpochs_.find(uid);
    if (it != rasterEpochs_.end() && it->second != epoch) {
        it->second = epoch;
    } else if (it == rasterEpochs_.end()) {
        rasterEpochs_[uid] = epoch;
    }
    const uchar* pixels = reinterpret_cast<const uchar*>(buffer->pixels.data());
    int bpl = w * static_cast<int>(sizeof(uint32_t));
    auto* holder = new std::shared_ptr<PixelBuffer>(std::move(buffer));
    return QImage(pixels, w, h, bpl, QImage::Format_ARGB32_Premultiplied,
                  [](void* info) { delete static_cast<std::shared_ptr<PixelBuffer>*>(info); },
                  holder);
}

QImage CanvasWidget::readLayerPixels(Layer* layer) {
    if (!layer || !doc_) {
        QImage empty(doc_ ? doc_->width() : 1920, doc_ ? doc_->height() : 1080, QImage::Format_ARGB32_Premultiplied);
        empty.fill(Qt::transparent);
        return empty;
    }
    if (layer->type() == LayerType::Raster) {
        auto* raster = static_cast<RasterLayer*>(layer);
        if (raster->pixelData() && raster->pixelCount() > 0) {
            return QImage(reinterpret_cast<const uchar*>(raster->pixelData()),
                         raster->width(), raster->height(),
                         raster->width() * static_cast<int>(sizeof(uint32_t)),
                         QImage::Format_ARGB32_Premultiplied).copy();
        }
    }
    QImage empty(doc_->width(), doc_->height(), QImage::Format_ARGB32_Premultiplied);
    empty.fill(Qt::transparent);
    return empty;
}

QImage CanvasWidget::readRasterRect(RasterLayer* raster, const QRect& rect) const {
    if (!raster || rect.isEmpty()) return {};
    QImage result(rect.width(), rect.height(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    int ox = raster->originX(), oy = raster->originY();
    int rw = raster->width(), rh = raster->height();
    for (int y = 0; y < rect.height(); ++y) {
        int srcY = rect.y() + y - oy;
        if (srcY < 0 || srcY >= rh) continue;
        int srcStart = rect.x() - ox;
        int dstStart = 0;
        if (srcStart < 0) { dstStart = -srcStart; srcStart = 0; }
        int copyW = std::min(rect.width() - dstStart, rw - srcStart);
        if (copyW <= 0) continue;
        const uint32_t* srcRow = raster->pixelData() + srcY * rw + srcStart;
        uint32_t* dstRow = reinterpret_cast<uint32_t*>(result.scanLine(y));
        if (srcRow && dstRow)
            std::copy(srcRow, srcRow + copyW, dstRow + dstStart);
    }
    return result;
}

void CanvasWidget::writeRasterRect(RasterLayer* raster, const QRect& rect, const QImage& pixels) {
    if (!raster || rect.isEmpty() || pixels.isNull()) return;
    QImage src = pixels.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    int ox = raster->originX(), oy = raster->originY();
    int rw = raster->width(), rh = raster->height();
    for (int y = 0; y < rect.height(); ++y) {
        int dstY = rect.y() + y - oy;
        if (dstY < 0 || dstY >= rh) continue;
        int dstStart = rect.x() - ox;
        int srcStart = 0;
        if (dstStart < 0) { srcStart = -dstStart; dstStart = 0; }
        int copyW = std::min(rect.width() - srcStart, rw - dstStart);
        if (copyW <= 0) continue;
        const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(src.constScanLine(y));
        uint32_t* dstRow = raster->pixelData() + dstY * rw + dstStart;
        if (srcRow && dstRow) {
            std::copy(srcRow + srcStart, srcRow + srcStart + copyW, dstRow);
            for (int x = 0; x < copyW; ++x) {
                if ((dstRow[x] >> 24) == 0) dstRow[x] = 0;
            }
        }
    }
}

void CanvasWidget::writeLayerPixels(Layer* layer, const QImage& img) {
    if (!layer || img.isNull()) return;
    if (layer->type() == LayerType::Raster) {
        auto* raster = static_cast<RasterLayer*>(layer);
        if (img.width() <= 0 || img.height() <= 0) return;
        int ox = raster->originX();
        int oy = raster->originY();
        raster->ensureContains(ox, oy, img.width(), img.height());
        QImage converted = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        int dstX = ox - raster->originX();
        int dstY = oy - raster->originY();
        int w = std::min(converted.width(), raster->width() - dstX);
        int h = std::min(converted.height(), raster->height() - dstY);
        for (int y = 0; y < h; ++y) {
            const uint32_t* src = reinterpret_cast<const uint32_t*>(converted.constScanLine(y));
            if (!src) continue;
            uint32_t* dst = raster->pixelData();
            if (!dst) continue;
            std::copy(src, src + w, dst + (dstY + y) * raster->width() + dstX);
        }
        for (int y = 0; y < h; ++y) {
            uint32_t* dst = raster->pixelData();
            if (!dst) continue;
            uint32_t* row = dst + (dstY + y) * raster->width() + dstX;
            for (int x = 0; x < w; ++x) {
                if ((row[x] >> 24) == 0) row[x] = 0;
            }
        }
        raster->bufferEpochTick();
    }
}

void CanvasWidget::blendStrokeToLayer(RasterLayer* raster, const QImage& stroke) {
    if (!raster || stroke.isNull()) return;
    QImage src = stroke.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    int ox = raster->originX();
    int oy = raster->originY();
    raster->ensureContains(ox, oy, src.width(), src.height());
    int dstX = ox - raster->originX();
    int dstY = oy - raster->originY();
    int w = std::min(src.width(), raster->width() - dstX);
    int h = std::min(src.height(), raster->height() - dstY);
    for (int y = 0; y < h; ++y) {
        const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(src.constScanLine(y));
        if (!srcRow) continue;
        uint32_t* dstRow = raster->pixelData();
        if (!dstRow) continue;
        dstRow += (dstY + y) * raster->width() + dstX;
        for (int x = 0; x < w; ++x) {
            uint32_t s = srcRow[x];
            uint32_t sa = (s >> 24) & 0xFF;
            if (sa < 4) continue;
            if (sa == 255) { dstRow[x] = s; continue; }
            uint32_t d = dstRow[x];
            uint32_t invA = 255 - sa;
            uint32_t sr = (s >> 16) & 0xFF;
            uint32_t sg = (s >> 8) & 0xFF;
            uint32_t sb = s & 0xFF;
            uint32_t dr = (d >> 16) & 0xFF;
            uint32_t dg = (d >> 8) & 0xFF;
            uint32_t db = d & 0xFF;
            uint32_t da = (d >> 24) & 0xFF;
            uint32_t outA = sa + (da * invA) / 255;
            uint32_t outR = sr + (dr * invA) / 255;
            uint32_t outG = sg + (dg * invA) / 255;
            uint32_t outB = sb + (db * invA) / 255;
            dstRow[x] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }
    raster->bufferEpochTick();
}

void CanvasWidget::blendStrokeToLayerAt(RasterLayer* raster, const QImage& stroke, int canvasX, int canvasY) {
    if (!raster || stroke.isNull()) return;
    QImage src = stroke.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    int ox = raster->originX();
    int oy = raster->originY();
    raster->ensureContains(canvasX, canvasY, src.width(), src.height());
    int dstX = canvasX - raster->originX();
    int dstY = canvasY - raster->originY();
    int w = std::min(src.width(), raster->width() - dstX);
    int h = std::min(src.height(), raster->height() - dstY);
    for (int y = 0; y < h; ++y) {
        const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(src.constScanLine(y));
        if (!srcRow) continue;
        uint32_t* dstRow = raster->pixelData();
        if (!dstRow) continue;
        dstRow += (dstY + y) * raster->width() + dstX;
        for (int x = 0; x < w; ++x) {
            uint32_t s = srcRow[x];
            uint32_t sa = (s >> 24) & 0xFF;
            if (sa < 4) continue;
            if (sa == 255) { dstRow[x] = s; continue; }
            uint32_t d = dstRow[x];
            uint32_t invA = 255 - sa;
            uint32_t sr = (s >> 16) & 0xFF;
            uint32_t sg = (s >> 8) & 0xFF;
            uint32_t sb = s & 0xFF;
            uint32_t dr = (d >> 16) & 0xFF;
            uint32_t dg = (d >> 8) & 0xFF;
            uint32_t db = d & 0xFF;
            uint32_t da = (d >> 24) & 0xFF;
            uint32_t outA = sa + (da * invA) / 255;
            uint32_t outR = sr + (dr * invA) / 255;
            uint32_t outG = sg + (dg * invA) / 255;
            uint32_t outB = sb + (db * invA) / 255;
            dstRow[x] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }
    raster->bufferEpochTick();
}

void CanvasWidget::shiftFrameData(int fromFrame, int delta) {
    std::map<int, std::vector<RawStroke>> newStrokes;
    for (auto it = vectorStrokes_.begin(); it != vectorStrokes_.end(); ) {
        if (it->first >= fromFrame) {
            int newIdx = it->first + delta;
            if (newIdx >= 0) newStrokes[newIdx] = std::move(it->second);
            it = vectorStrokes_.erase(it);
        } else {
            ++it;
        }
    }
    vectorStrokes_.merge(newStrokes);

    undoStack_.clear();
}

void CanvasWidget::pushFullLayerUndo(Layer* layer, const QImage& beforeSnap,
                                     const QString& desc) {
    if (!layer || beforeSnap.isNull()) return;
    auto& strokes = vectorStrokes_[currentFrame_];
    auto beforeStrokes = strokes;
    QImage afterSnap = snapCurrentLayer();
    auto afterStrokes = strokes;
    QRect fullRect(0, 0, doc_->width(), doc_->height());
    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        fullRect = QRect(r->originX(), r->originY(), r->width(), r->height());
    }
    int modelIdx = currentRootLayer().indexOfLayer(layer);
    undoStack_.push(new PaintCommand(this, currentFrame_, modelIdx,
        layer->uid(), beforeSnap, std::move(afterSnap), fullRect,
        std::move(beforeStrokes), std::move(afterStrokes), desc));
}

void CanvasWidget::removeFrameData(int frameIdx) {
    vectorStrokes_.erase(frameIdx);
}

QImage CanvasWidget::compositeFrame(int frameNum) const {
    if (!doc_) return QImage();
    QImage result(doc_->width(), doc_->height(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter p(&result);
    const GroupLayer& root = const_cast<CanvasWidget*>(this)->currentRootLayer(frameNum);
    auto& layers = root.layers();
    for (auto it = layers.begin(); it != layers.end(); ++it) {
        Layer* layer = it->get();
        if (!layer->visible()) continue;
        p.setOpacity(layer->opacity());
        p.setCompositionMode(toQtCompositionMode(layer->blendMode()));
        if (layer->type() == LayerType::Raster) {
            RasterLayer* rasterLayer = static_cast<RasterLayer*>(layer);
            QImage img = const_cast<CanvasWidget*>(this)->wrapRasterLayer(rasterLayer);
            if (!img.isNull()) { p.drawImage(rasterLayer->originX(), rasterLayer->originY(), img); }
        } else if (layer->type() == LayerType::Vector) {
            const_cast<CanvasWidget*>(this)->renderVectorStrokes(p, frameNum, layer->uid());
        }
    }
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setOpacity(1.0);
    p.end(); return result;
}

void CanvasWidget::tabletEvent(QTabletEvent* ev) {
    ev->accept();
    tabletActive_ = true;
    tabletPressure_ = static_cast<float>(ev->pressure());
    tabletEraser_ = (ev->pointerType() == QPointingDevice::PointerType::Eraser);
    switch (ev->type()) {
    case QEvent::TabletPress: inputPress(ev->position()); break;
    case QEvent::TabletMove: if (drawing_ || panning_ || moving_ || tool_ == 2 || selState_ != SelectionState::None) inputMove(ev->position()); cursorPos_ = ev->position(); break;
    case QEvent::TabletRelease: if (drawing_ || moving_ || tool_ == 2 || selState_ != SelectionState::None) inputRelease(); tabletActive_ = false; break;
    default: break;
    }
    update();
}

void CanvasWidget::mousePressEvent(QMouseEvent* ev) {
    if (tabletActive_ && !drawing_) { tabletActive_ = false; }
    if (tabletActive_) return;
    cursorPos_ = ev->position();
    if (ev->button() == Qt::MiddleButton || (tool_ == 10 && ev->button() == Qt::LeftButton)) {
        panning_ = true; panStart_ = ev->position(); panOffStart_ = QPointF(offX_, offY_);
        setCursor(Qt::ClosedHandCursor); return;
    }
    if (ev->button() == Qt::LeftButton) inputPress(ev->position());
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* ev) {
    if (tabletActive_ && !drawing_) { tabletActive_ = false; }
    if (tabletActive_) return;
    cursorPos_ = ev->position();
    if (panning_) { QPointF delta = ev->position() - panStart_; offX_ = std::round(panOffStart_.x()+delta.x()); offY_ = std::round(panOffStart_.y()+delta.y()); update(); return; }
    if (drawing_ || moving_ || tool_ == 2 || selState_ != SelectionState::None) inputMove(ev->position());
    update();
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* ev) {
    if (tabletActive_) return;
    cursorPos_ = ev->position();
    if (panning_) { panning_ = false; setCursor(tool_ == 10 ? Qt::OpenHandCursor : Qt::CrossCursor); return; }
    if (drawing_ || moving_ || tool_ == 2 || selState_ != SelectionState::None) inputRelease();
}

void CanvasWidget::wheelEvent(QWheelEvent* ev) {
    double factor = 1.12;
    QPointF mpos = ev->position();
    QPointF oldCpos = screenToCanvas(mpos);
    zoom_ = std::clamp(ev->angleDelta().y() > 0 ? zoom_ * factor : zoom_ / factor, 0.02, 64.0);
    QPointF newCpos = screenToCanvas(mpos);
    offX_ = std::round(offX_ + (newCpos.x() - oldCpos.x()) * zoom_);
    offY_ = std::round(offY_ + (newCpos.y() - oldCpos.y()) * zoom_);
    update();
}

void CanvasWidget::keyPressEvent(QKeyEvent* ev) {
    int k = ev->key(); bool ctrl = ev->modifiers() & Qt::ControlModifier;
    if (ctrl && k == Qt::Key_Z) { undo(); return; }
    if (ctrl && k == Qt::Key_Y) { redo(); return; }
    if (ctrl && k == Qt::Key_0) { zoom_=1.0; offX_=0; offY_=0; rotation_=0; fit(); return; }
    if (ctrl && k == Qt::Key_C) { copySelection(); return; }
    if (ctrl && k == Qt::Key_X) { cutSelection(); return; }
    if (ctrl && k == Qt::Key_V) { pasteClipboard(); return; }
    if (k == Qt::Key_Return || k == Qt::Key_Enter) { if (tool_ == 11 && transformActive_) { commitTransform(); } else if (tool_ == 9 && floatingActive_) { commitFloatingSelection(); } return; }
    if (k == Qt::Key_W) { setTool(11); emit toolChangedByKey(11); return; }
    if (k == Qt::Key_B) { setTool(0); emit toolChangedByKey(0); return; }
    if (k == Qt::Key_E) { setTool(1); emit toolChangedByKey(1); return; }
    if (k == Qt::Key_I) { setTool(2); emit toolChangedByKey(2); return; }
    if (k == Qt::Key_G) { setTool(3); emit toolChangedByKey(3); return; }
    if (k == Qt::Key_T) { setTool(4); emit toolChangedByKey(4); return; }
    if (k == Qt::Key_L) { setTool(5); emit toolChangedByKey(5); return; }
    if (k == Qt::Key_U) { setTool(6); emit toolChangedByKey(6); return; }
    if (k == Qt::Key_Y) { setTool(7); emit toolChangedByKey(7); return; }
    if (k == Qt::Key_M) { setTool(8); emit toolChangedByKey(8); return; }
    if (k == Qt::Key_S && !ctrl) { setTool(9); emit toolChangedByKey(9); return; }
    if (k == Qt::Key_H && !ctrl) { setTool(10); emit toolChangedByKey(10); return; }
    if (k == Qt::Key_H && ctrl) { flipH_ = !flipH_; update(); return; }
    if (k == Qt::Key_R && !ctrl) { rotation_ = (rotation_ + 15) % 360; update(); return; }
    if (k == Qt::Key_Apostrophe) { showGrid_ = !showGrid_; if (showGrid_ && gridSize_ == 0) gridSize_ = 64; update(); return; }
    if (k == Qt::Key_Left) { if (currentFrame_ > 0) currentFrame_--; emit canvasUpdated(); return; }
    if (k == Qt::Key_Right) { if (currentFrame_ < totalFrames_ - 1) currentFrame_++; emit canvasUpdated(); return; }
    if (k == Qt::Key_F) { fit(); return; }
    if (k == Qt::Key_Space) { emit togglePlayPause(); return; }
    if (k == Qt::Key_Delete || k == Qt::Key_Backspace) { deleteSelection(); return; }
}

void CanvasWidget::inputPress(QPointF screenPos) {
    cursorPos_ = screenPos;
    if (tool_ == 10) return;
    if (tool_ == 2) { doPickColor(screenToCanvas(screenPos)); return; }

    if ((tool_ == 0 || tool_ == 1) && floatingActive_ && !originalFloatingSelection_.isNull()) {
        commitFloatingSelection();
    }

    QPointF cpos = screenToCanvas(screenPos);

    Layer* activeLayer = resolveCurrentLayer();

    if (!activeLayer) {
        emit statusMessage("No active layer selected.");
        return;
    }

    if (activeLayer->locked()) {
        emit statusMessage(QString("Cannot draw: \"%1\" is locked. Select another layer.")
            .arg(QString::fromStdString(activeLayer->name())));
        return;
    }

    if (tool_ == 3) { doFill(cpos); return; }
    if (tool_ == 4) { doText(cpos); return; }
    if (tool_ == 8) { startMove(cpos); return; }
    if (tool_ == 9) {
        if (floatingActive_ && selRect_.isValid()) {
            qreal hs = 8.0 / zoom_;
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
                if (std::abs(cpos.x() - h[i].x()) < hs && std::abs(cpos.y() - h[i].y()) < hs) {
                    hit = i; break;
                }
            }
            if (hit >= 0) {
                selState_ = SelectionState::DraggingHandle;
                selHandleIdx_ = hit;
                selDragStart_ = cpos;
                return;
            }
            if (selRect_.contains(cpos)) {
                selState_ = SelectionState::MovingContent;
                selDragStart_ = cpos;
                return;
            }
            commitFloatingSelection();
            return;
        }
        selStart_ = cpos; selState_ = SelectionState::Creating; return;
    }
    if (tool_ == 11 && transformActive_) {
        ensureTransformBounds();
        QPointF c = transformBounds_.center();
        auto tx = [&](QPointF p) -> QPointF {
            return QPointF((p.x() - c.x()) * scaleX_ + c.x() + translate_.x(),
                           (p.y() - c.y()) * scaleY_ + c.y() + translate_.y());
        };
        QPointF corners[4] = {
            transformBounds_.topLeft(),
            transformBounds_.topRight(),
            transformBounds_.bottomRight(),
            transformBounds_.bottomLeft()
        };
        QPointF handles[8] = {
            tx(corners[0]),
            tx(QPointF((corners[0].x() + corners[1].x()) * 0.5, corners[0].y())),
            tx(corners[1]),
            tx(QPointF(corners[1].x(), (corners[1].y() + corners[2].y()) * 0.5)),
            tx(corners[2]),
            tx(QPointF((corners[2].x() + corners[3].x()) * 0.5, corners[2].y())),
            tx(corners[3]),
            tx(QPointF(corners[0].x(), (corners[0].y() + corners[3].y()) * 0.5)),
        };

        qreal hitR = 12.0 / zoom_;
        int hit = -1;
        for (int i = 0; i < 8; ++i) {
            if (QPointF::dotProduct(cpos - handles[i], cpos - handles[i]) < hitR * hitR) {
                hit = i; break;
            }
        }

        activeHandle_ = hit;
        transformDragOrigin_ = cpos;
        dragScaleRefX_ = scaleX_; dragScaleRefY_ = scaleY_;
        dragTranslateRef_ = translate_;
        handleScaleX_ = false; handleScaleY_ = false;

        if (hit >= 0) {
            switch (hit) {
            case 0: handleFixedPoint_ = transformBounds_.bottomRight(); handleScaleX_ = handleScaleY_ = true; break;
            case 1: handleFixedPoint_ = QPointF(transformBounds_.center().x(), transformBounds_.bottom()); handleScaleY_ = true; break;
            case 2: handleFixedPoint_ = transformBounds_.bottomLeft(); handleScaleX_ = handleScaleY_ = true; break;
            case 3: handleFixedPoint_ = QPointF(transformBounds_.left(), transformBounds_.center().y()); handleScaleX_ = true; break;
            case 4: handleFixedPoint_ = transformBounds_.topLeft(); handleScaleX_ = handleScaleY_ = true; break;
            case 5: handleFixedPoint_ = QPointF(transformBounds_.center().x(), transformBounds_.top()); handleScaleY_ = true; break;
            case 6: handleFixedPoint_ = transformBounds_.topRight(); handleScaleX_ = handleScaleY_ = true; break;
            case 7: handleFixedPoint_ = QPointF(transformBounds_.right(), transformBounds_.center().y()); handleScaleX_ = true; break;
            }
        } else {
            QPointF tc = tx(transformBounds_.center());
            QRectF tBounds(tx(transformBounds_.topLeft()), tx(transformBounds_.bottomRight()));
            if (tBounds.normalized().contains(cpos))
                activeHandle_ = 8;
        }
        return;
    }
    if (tool_ >= 5 && tool_ <= 7) { shapeStart_ = cpos; shapeEnd_ = cpos; drawing_ = true; return; }

    drawing_ = true;
    strokePoints_.clear();
    float pressure = tabletActive_ ? tabletPressure_ : 0.5f;
    if (brush_ && tool_ != 1) {
        brush_->beginStroke(Color{static_cast<float>(color_.redF()), static_cast<float>(color_.greenF()),
                                   static_cast<float>(color_.blueF()), static_cast<float>(color_.alphaF())},
                             brush_->preset().size);
    }
    auto _beforeSnap = snapCurrentLayer();
    StrokePoint pt; pt.position={static_cast<float>(cpos.x()), static_cast<float>(cpos.y())}; pt.pressure=pressure;
    strokePoints_.push_back(pt);
    if (brush_ && tool_ != 1) brush_->addPoint(pt);
}

void CanvasWidget::inputMove(QPointF screenPos) {
    cursorPos_ = screenPos;
    QPointF cpos = screenToCanvas(screenPos);

    if (tool_ == 2) { doPickColor(cpos); return; }
    if (selState_ == SelectionState::Creating) {
        selRect_ = QRectF(selStart_, cpos).normalized(); update(); return;
    }
    if (selState_ == SelectionState::MovingContent) {
        QPointF delta = cpos - selDragStart_;
        selRect_.translate(delta);
        selDragStart_ = cpos;
        update();
        return;
    }
    if (selState_ == SelectionState::DraggingHandle) {
        static const struct { bool l, t, r, b; } e[8] = {
            {1,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,1,0},
            {0,0,1,1},{0,0,0,1},{1,0,0,1},{1,0,0,0},
        };
        qreal l = selRect_.left(), t = selRect_.top(), r_ = selRect_.right(), b = selRect_.bottom();
        auto& f = e[selHandleIdx_];
        if (f.l) l = cpos.x();
        if (f.t) t = cpos.y();
        if (f.r) r_ = cpos.x();
        if (f.b) b = cpos.y();
        qreal nl = std::min(l, r_), nr = std::max(l, r_);
        qreal nt = std::min(t, b), nb = std::max(t, b);
        if (nr - nl < 2.0) { nr = nl + 2.0; if (f.l) nl = nr - 2.0; else if (f.r) nr = nl + 2.0; }
        if (nb - nt < 2.0) { nb = nt + 2.0; if (f.t) nt = nb - 2.0; else if (f.b) nb = nt + 2.0; }
        selRect_ = QRectF(QPointF(nl, nt), QPointF(nr, nb));
        update();
        return;
    }
    if (tool_ == 11 && activeHandle_ >= 0) {
        if (activeHandle_ == 8) {
            translate_ = dragTranslateRef_ + (cpos - transformDragOrigin_);
        } else {
            QPointF f = handleFixedPoint_;
            if (handleScaleX_) {
                qreal refW = std::abs(transformBounds_.center().x() - f.x()) * 2.0;
                if (refW > 1.0) scaleX_ = std::max(0.01, dragScaleRefX_ * std::abs(cpos.x() - f.x()) / (refW * 0.5));
                if (dragScaleRefX_ < 0 != (cpos.x() < f.x())) scaleX_ = -scaleX_;
            }
            if (handleScaleY_) {
                qreal refH = std::abs(transformBounds_.center().y() - f.y()) * 2.0;
                if (refH > 1.0) scaleY_ = std::max(0.01, dragScaleRefY_ * std::abs(cpos.y() - f.y()) / (refH * 0.5));
                if (dragScaleRefY_ < 0 != (cpos.y() < f.y())) scaleY_ = -scaleY_;
            }
        }
        update();
        return;
    }
    if (moving_) { updateMove(cpos); return; }
    if (!drawing_) return;

    if (tool_ >= 5 && tool_ <= 7) { shapeEnd_ = cpos; update(); return; }

    int stabilizer = brush_ ? static_cast<int>(brush_->preset().dynamics.smoothing * 10.0f) : 0;
    if (stabilizer > 0) {
        stabilizerBuffer_.push_back(cpos);
        if (static_cast<int>(stabilizerBuffer_.size()) > stabilizer) {
            stabilizerBuffer_.erase(stabilizerBuffer_.begin());
        }
        double sumX = 0, sumY = 0;
        for (const auto& p : stabilizerBuffer_) { sumX += p.x(); sumY += p.y(); }
        cpos = QPointF(sumX / stabilizerBuffer_.size(), sumY / stabilizerBuffer_.size());
    }

    float pressure = tabletActive_ ? tabletPressure_ : 0.5f;
    StrokePoint last = strokePoints_.back();
    double dist = std::hypot(cpos.x() - last.position.x, cpos.y() - last.position.y);
    if (dist < 0.5) return;
    StrokePoint pt; pt.position={static_cast<float>(cpos.x()), static_cast<float>(cpos.y())}; pt.pressure=pressure;
    strokePoints_.push_back(pt);
    if (brush_ && tool_ != 1) brush_->addPoint(pt);
    update();
}

void CanvasWidget::inputRelease() {
    if (tool_ == 2) { emit colorCommittedToRecent(color_); return; }
    if (selState_ == SelectionState::Creating) {
        if (selRect_.width() < 3 || selRect_.height() < 3) {
            selRect_ = QRectF();
            floatingSelection_ = QImage();
            originalFloatingSelection_ = QImage();
            selState_ = SelectionState::None;
            update();
            return;
        }
        commitSelection(); selState_ = SelectionState::None; return;
    }
    if (selState_ == SelectionState::MovingContent) { selState_ = SelectionState::None; update(); return; }
    if (selState_ == SelectionState::DraggingHandle) { selState_ = SelectionState::None; update(); return; }
    if (tool_ == 11 && activeHandle_ >= 0) { activeHandle_ = -1; update(); return; }
    if (moving_) { commitMove(); return; }
    if (!drawing_) return;

    if (tool_ >= 5 && tool_ <= 7 && shapeStart_.x() >= 0 && shapeEnd_.x() >= 0) {
        drawShape(); shapeStart_={-1,-1}; shapeEnd_={-1,-1}; drawing_=false; emit canvasUpdated(); return;
    }

    drawing_ = false;
    if (tool_ == 1) {
        doErase();
        strokePoints_.clear();
        tabletActive_ = false;
        emit canvasUpdated();
        return;
    }
    if (!strokePoints_.empty()) {
        if (brush_) brush_->endStroke();
        commitStroke();
    }
    strokePoints_.clear();
    tabletActive_ = false;
    emit canvasUpdated();
}

void CanvasWidget::doErase() {
    if (!doc_ || strokePoints_.empty()) return;
    Layer* layer = resolveCurrentLayer();
    if (!layer || layer->locked()) return;
    LayerUid layerUid = layer->uid();
    float sz = brush_ ? brush_->preset().size : 20.0f;

    QImage beforeSnap = snapCurrentLayer();
    auto vecBefore = vectorStrokes_[currentFrame_];

    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        QImage img = readLayerPixels(layer);
        {
            QPainter ep(&img);
            ep.setRenderHint(QPainter::Antialiasing);
            ep.setCompositionMode(QPainter::CompositionMode_DestinationOut);
            QPen pen(Qt::black, sz, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            ep.setPen(pen);
            for (size_t i = 1; i < strokePoints_.size(); ++i) {
                ep.drawLine(QPointF(strokePoints_[i-1].position.x, strokePoints_[i-1].position.y),
                            QPointF(strokePoints_[i].position.x, strokePoints_[i].position.y));
            }
            if (strokePoints_.size() == 1)
                ep.drawPoint(QPointF(strokePoints_[0].position.x, strokePoints_[0].position.y));
            ep.end();
        }
        writeLayerPixels(layer, img);
    }

    auto& strokes = vectorStrokes_[currentFrame_];
    strokes.erase(
        std::remove_if(strokes.begin(), strokes.end(),
            [layerUid](const RawStroke& vs) { return vs.layerUid == layerUid; }),
        strokes.end());
    if (strokes.empty()) vectorStrokes_.erase(currentFrame_);

    QImage afterSnap = snapCurrentLayer();
    auto vecAfter = vectorStrokes_[currentFrame_];
    QRect fullRect(0, 0, doc_->width(), doc_->height());
    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        fullRect = QRect(r->originX(), r->originY(), r->width(), r->height());
    }
    undoStack_.push(new PaintCommand(this, currentFrame_, currentLayerIdx_,
        layer->uid(), std::move(beforeSnap), std::move(afterSnap), fullRect,
        std::move(vecBefore), std::move(vecAfter), QStringLiteral("Erase")));
}

void CanvasWidget::commitStroke() {
    if (!doc_ || strokePoints_.size() < 2) return;
    Layer* layer = resolveCurrentLayer();
    if (!layer || layer->locked()) return;

    float sz = brush_ ? brush_->preset().size : 10.0f;
    float padding = sz * 1.5f + 1.0f;

    float minX = strokePoints_[0].position.x, maxX = minX;
    float minY = strokePoints_[0].position.y, maxY = minY;
    for (const auto& pt : strokePoints_) {
        minX = std::min(minX, pt.position.x);
        maxX = std::max(maxX, pt.position.x);
        minY = std::min(minY, pt.position.y);
        maxY = std::max(maxY, pt.position.y);
    }
    QRect dirtyRect(
        static_cast<int>(std::floor(minX - padding)),
        static_cast<int>(std::floor(minY - padding)),
        static_cast<int>(std::ceil(maxX - minX + padding * 2)),
        static_cast<int>(std::ceil(maxY - minY + padding * 2)));

    QImage beforePixels;
    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        beforePixels = readRasterRect(r, dirtyRect);
        for (const auto& pt : strokePoints_)
            r->ensureContains(static_cast<int>(pt.position.x - sz),
                              static_cast<int>(pt.position.y - sz),
                              static_cast<int>(sz * 2),
                              static_cast<int>(sz * 2));
    }

    float opacity = brush_ ? brush_->preset().opacity : 1.0f;
    bool pSize = brush_ ? brush_->preset().dynamics.pressure_size : true;
    bool pOpacity = brush_ ? brush_->preset().dynamics.pressure_opacity : false;
    auto beforeStrokes = vectorStrokes_[currentFrame_];
    RawStroke vs;
    vs.layerUid = layer->uid();
    vs.points = strokePoints_;
    vs.color = color_;
    vs.baseWidth = sz;
    vs.opacity = opacity;
    vs.pressureSize = pSize;
    vs.pressureOpacity = pOpacity;
    vs.eraser = false;
    vectorStrokes_[currentFrame_].push_back(vs);

    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        constexpr int kStampPad = 2;
        QRect stampRect = dirtyRect.adjusted(-kStampPad, -kStampPad, kStampPad, kStampPad);
        QImage img(stampRect.width(), stampRect.height(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img); p.setRenderHint(QPainter::Antialiasing);
        p.translate(-stampRect.x(), -stampRect.y());
        renderVectorStroke(p, vs);
        p.end();
        blendStrokeToLayerAt(r, img, stampRect.x(), stampRect.y());
    }

    QImage afterPixels;
    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        afterPixels = readRasterRect(r, dirtyRect);
    }
    auto afterStrokes = vectorStrokes_[currentFrame_];

    int modelIdx = currentRootLayer().indexOfLayer(layer);
    undoStack_.push(new PaintCommand(
        this, currentFrame_, modelIdx, layer->uid(),
        std::move(beforePixels), std::move(afterPixels), dirtyRect,
        std::move(beforeStrokes), std::move(afterStrokes)));

    emit statusMessage(QString("Stroke on: %1")
        .arg(QString::fromStdString(layer->name())));

}

void CanvasWidget::drawShape() {
    if (!doc_) return;
    Layer* layer = resolveCurrentLayer();
    if (!layer || layer->locked()) return;
    double x1=shapeStart_.x(), y1=shapeStart_.y(), x2=shapeEnd_.x(), y2=shapeEnd_.y();
    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        r->ensureContains(static_cast<int>(std::min(x1, x2) - 10),
                          static_cast<int>(std::min(y1, y2) - 10),
                          static_cast<int>(std::abs(x2 - x1) + 20),
                          static_cast<int>(std::abs(y2 - y1) + 20));
    }
    auto _beforeSnap = snapCurrentLayer();
    QImage img = readLayerPixels(layer);
    QPainter p(&img); p.setRenderHint(QPainter::Antialiasing);
    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        p.translate(-r->originX(), -r->originY());
    }
    p.setPen(QPen(color_, brush_ ? brush_->preset().size : 5.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (tool_==5) p.drawLine(QPointF(x1,y1), QPointF(x2,y2));
    else if (tool_==6) p.drawRect(QRectF(QPointF(x1,y1), QPointF(x2,y2)).normalized());
    else if (tool_==7) p.drawEllipse(QRectF(QPointF(x1,y1), QPointF(x2,y2)).normalized());
    p.end();
    writeLayerPixels(layer, img);
    pushFullLayerUndo(layer, _beforeSnap, QStringLiteral("Action"));
}

void CanvasWidget::doFill(QPointF cpos) {
    if (!doc_) return;
    Layer* layer = resolveCurrentLayer();
    if (!layer || layer->locked()) return;

    QImage img = readLayerPixels(layer);
    int ox = 0, oy = 0;
    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        ox = r->originX(); oy = r->originY();
    }
    int x = static_cast<int>(cpos.x()) - ox;
    int y = static_cast<int>(cpos.y()) - oy;
    if (x < 0 || x >= img.width() || y < 0 || y >= img.height()) return;

    QColor target = img.pixelColor(x, y);
    if (target == color_) return;

    auto _beforeSnap = snapCurrentLayer();
    if (target.alpha() < 20) {
        floodFillByAlpha(img, x, y, color_, 100);
    } else {
        floodFill(img, x, y, color_, target, 32);
    }
    writeLayerPixels(layer, img);
    pushFullLayerUndo(layer, _beforeSnap, QStringLiteral("Action"));
    emit canvasUpdated();
}

void CanvasWidget::doPickColor(QPointF cpos) {
    if (!doc_) return;

    QImage composite = compositeFrame(currentFrame_);
    if (composite.isNull()) return;

    QImage onWhite(composite.size(), QImage::Format_ARGB32_Premultiplied);
    onWhite.fill(Qt::white);
    {
        QPainter pp(&onWhite);
        pp.setCompositionMode(QPainter::CompositionMode_SourceOver);
        pp.drawImage(0, 0, composite);
    }

    int x = static_cast<int>(cpos.x()), y = static_cast<int>(cpos.y());
    if (x < 0 || x >= onWhite.width() || y < 0 || y >= onWhite.height()) return;

    QColor c = onWhite.pixelColor(x, y);
    color_ = c;
    if (brush_) {
        BrushPreset p = brush_->preset();
        p.color = Color{c.redF(), c.greenF(), c.blueF(), c.alphaF()};
        brush_->setPreset(p);
    }
    emit colorPicked(c);
}

void CanvasWidget::doText(QPointF cpos) {
    if (!doc_) return;
    Layer* layer = resolveCurrentLayer();
    if (!layer || layer->locked()) return;

    bool ok;
    QString text = QInputDialog::getMultiLineText(this, "Add Text", "Enter text:", "", &ok);
    if (!ok || text.isEmpty()) return;

    bool fontOk;
    QFont chosen = QFontDialog::getFont(&fontOk, font_, this, "Choose Font");
    if (fontOk) font_ = chosen;

    auto _beforeSnap = snapCurrentLayer();
    QImage img = readLayerPixels(layer);
    QPainter p(&img); p.setRenderHint(QPainter::Antialiasing);
    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        p.translate(-r->originX(), -r->originY());
    }
    p.setFont(font_);
    p.setPen(color_);
    p.drawText(cpos, text);
    p.end();
    writeLayerPixels(layer, img);
    pushFullLayerUndo(layer, _beforeSnap, QStringLiteral("Action"));
    emit canvasUpdated();
}

void CanvasWidget::startMove(QPointF cpos) {
    if (!doc_) return;
    Layer* layer = resolveCurrentLayer();
    if (!layer) return;
    moveLayer_ = layer;
    moveStart_ = cpos;
    if (!selImage_.isNull() && selRect_.isValid()) {
        moveImage_ = selImage_;
        moveSrcRect_ = selRect_;
        moveModeSelection_ = true;
    } else {
        moveImage_ = readLayerPixels(layer);
        int mx = 0, my = 0;
        if (layer->type() == LayerType::Raster) {
            auto* r = static_cast<RasterLayer*>(layer);
            mx = r->originX(); my = r->originY();
        }
        moveSrcRect_ = QRectF(mx, my, moveImage_.width(), moveImage_.height());
        moveModeSelection_ = false;
    }
    moving_ = true;
    auto _beforeSnap = snapCurrentLayer();
}

void CanvasWidget::updateMove(QPointF cpos) {
    if (!moving_ || !moveLayer_) return;
    moveOffset_ = QPointF(cpos.x() - moveStart_.x(), cpos.y() - moveStart_.y());
    update();
}

void CanvasWidget::commitMove() {
    if (!moving_ || !moveLayer_) return;
    moving_ = false;

    constexpr double kMaxMoveDelta = 40'000.0;
    double clampedDx = std::clamp(moveOffset_.x(), -kMaxMoveDelta, kMaxMoveDelta);
    double clampedDy = std::clamp(moveOffset_.y(), -kMaxMoveDelta, kMaxMoveDelta);

    LayerUid uid = moveLayer_->uid();
    int modelIdx = currentRootLayer().indexOfLayer(moveLayer_);

    auto* cmd = new MoveCommand(
        this, currentFrame_, modelIdx, uid,
        moveSrcRect_,
        QPointF(clampedDx, clampedDy),
        moveImage_,
        moveModeSelection_);

    undoStack_.push(cmd);

    if (moveModeSelection_) {
        selRect_ = moveSrcRect_.translated(clampedDx, clampedDy);
        floatingActive_ = false;
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        selImage_ = QImage();
    }

    moveImage_ = QImage();
    moveLayer_ = nullptr;
    emit canvasUpdated();
}

void CanvasWidget::commitSelection() {
    if (!selRect_.isValid() || selRect_.width() < 3 || selRect_.height() < 3) {
        selRect_ = QRectF();
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        update();
        return;
    }

    QImage composite = compositeFrame(currentFrame_);
    selImage_ = composite.copy(selRect_.toAlignedRect());

    Layer* layer = resolveCurrentLayer();
    if (!layer || layer->locked()) {
        qWarning() << "[commitSelection] Layer null or locked, aborting";
        update();
        return;
    }

    qreal ox = 0, oy = 0;
    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        ox = r->originX(); oy = r->originY();
    }
    QImage layerPixels = readLayerPixels(layer);
    if (layerPixels.isNull()) {
        qWarning() << "[commitSelection] readLayerPixels returned null, aborting cut";
        update();
        return;
    }

    QRectF bufRect = selRect_.translated(-ox, -oy);
    floatingSelection_ = layerPixels.copy(bufRect.toAlignedRect());
    originalFloatingSelection_ = floatingSelection_;

    auto beforeSnap = snapCurrentLayer();
    {
        QPainter cp(&layerPixels);
        cp.setCompositionMode(QPainter::CompositionMode_Clear);
        cp.translate(-ox, -oy);
        cp.fillRect(selRect_, Qt::transparent);
        cp.end();
    }
    writeLayerPixels(layer, layerPixels);
    pushFullLayerUndo(layer, beforeSnap, QStringLiteral("Cut Selection"));
    floatingActive_ = true;
    marchingTimer_.start();

    update();
    emit canvasUpdated();
}

void CanvasWidget::dumpState(const QString& contextAction) const {
    qInfo() << "[CanvasState]" << contextAction
             << "| Tool:" << tool_
             << "| SelState:" << static_cast<int>(selState_)
             << "| floatingActive_:" << floatingActive_
             << "| selRect_:" << selRect_
             << "| floatSel.isNull:" << floatingSelection_.isNull()
             << "| origFloatSel.isNull:" << originalFloatingSelection_.isNull();
}

void CanvasWidget::commitFloatingSelection()
{
    dumpState("Inicio commitFloatingSelection");

    if (!floatingActive_ || floatingSelection_.isNull()) {
        floatingActive_ = false;
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        selRect_ = QRectF();
        update();
        dumpState("Fin commitFloatingSelection (abortado por estado nulo)");
        return;
    }

    Layer* layer = resolveCurrentLayer();
    if (!layer || layer->locked()) {
        qWarning() << "[commitFloatingSelection] Layer null or locked, aborting";
        floatingActive_ = false;
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        selRect_ = QRectF();
        update();
        return;
    }

    if (layer->type() != LayerType::Raster) {
        floatingActive_ = false;
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        selRect_ = QRectF();
        update();
        return;
    }

    auto* rLayer = static_cast<RasterLayer*>(layer);
    LayerUid targetUid = layer->uid();

    QImage beforePixels = readLayerPixels(layer);

    auto beforeStrokes = vectorStrokes_[currentFrame_];
    beforeStrokes.erase(
        std::remove_if(beforeStrokes.begin(), beforeStrokes.end(),
            [targetUid](const RawStroke& vs) { return vs.layerUid == targetUid; }),
        beforeStrokes.end());

    qreal ox = rLayer->originX();
    qreal oy = rLayer->originY();

    QImage img = readLayerPixels(layer);
    if (img.isNull()) {
        qWarning() << "[commitFloatingSelection] readLayerPixels returned null, aborting stamp";
        floatingActive_ = false;
        floatingSelection_ = QImage();
        originalFloatingSelection_ = QImage();
        selRect_ = QRectF();
        update();
        return;
    }

    {
        QPainter layerPainter(&img);
        layerPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        layerPainter.setRenderHint(QPainter::SmoothPixmapTransform);
        layerPainter.translate(-ox, -oy);
        layerPainter.drawImage(selRect_, floatingSelection_);
        layerPainter.end();
    }

    writeLayerPixels(layer, img);
    rLayer->bufferEpochTick();

    auto& strokes = vectorStrokes_[currentFrame_];
    strokes.erase(
        std::remove_if(strokes.begin(), strokes.end(),
            [targetUid](const RawStroke& vs) { return vs.layerUid == targetUid; }),
        strokes.end());
    if (strokes.empty()) {
        vectorStrokes_.erase(currentFrame_);
    }

    QImage afterPixels = readLayerPixels(layer);
    auto afterStrokes = vectorStrokes_[currentFrame_];

    QRect fullRect(rLayer->originX(), rLayer->originY(), rLayer->width(), rLayer->height());
    int modelIdx = currentRootLayer().indexOfLayer(layer);
    undoStack_.push(new PaintCommand(
        this, currentFrame_, modelIdx, layer->uid(),
        std::move(beforePixels), std::move(afterPixels), fullRect,
        std::move(beforeStrokes), std::move(afterStrokes),
        QStringLiteral("Commit Selection")));

    floatingActive_ = false;
    floatingSelection_ = QImage();
    originalFloatingSelection_ = QImage();
    selImage_ = QImage();
    selRect_ = QRectF();
    selState_ = SelectionState::None;

    update();
    emit canvasUpdated();
    dumpState("Fin commitFloatingSelection");
}

void CanvasWidget::ensureTransformBounds()
{
    if (floatingActive_ && selRect_.isValid()) {
        transformBounds_ = selRect_;
        return;
    }
    Layer* layer = resolveCurrentLayer();
    if (layer && layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        transformBounds_ = QRectF(r->originX(), r->originY(), r->width(), r->height());
    } else if (doc_) {
        transformBounds_ = QRectF(0, 0, doc_->width(), doc_->height());
    }
}

void CanvasWidget::commitTransform()
{
    if (!transformActive_) return;
    if (scaleX_ < 0.01f || scaleY_ < 0.01f) return;

    Layer* layer = resolveCurrentLayer();
    if (!layer || layer->locked()) return;

    QImage src = readLayerPixels(layer);
    QImage dst(src.size(), QImage::Format_ARGB32_Premultiplied);
    dst.fill(Qt::transparent);

    QPointF c = transformBounds_.center();
    QTransform t;
    t.translate(c.x() + translate_.x(), c.y() + translate_.y());
    t.scale(scaleX_, scaleY_);
    t.translate(-c.x(), -c.y());

    {
        QPainter pp(&dst);
        pp.setRenderHint(QPainter::SmoothPixmapTransform);
        pp.setTransform(t);
        pp.drawImage(0, 0, src);
    }

    auto beforeSnap = snapCurrentLayer();
    writeLayerPixels(layer, dst);
    pushFullLayerUndo(layer, beforeSnap, QStringLiteral("Transform"));

    scaleX_ = 1.0; scaleY_ = 1.0; translate_ = QPointF(0, 0);
    transformActive_ = false; activeHandle_ = -1;
    emit canvasUpdated();
}

void CanvasWidget::copySelection() {
    QImage toCopy;
    if (floatingActive_ && !floatingSelection_.isNull()) {
        toCopy = floatingSelection_;
    } else if (!selImage_.isNull()) {
        toCopy = selImage_;
    } else if (selRect_.isValid()) {
        Layer* layer = resolveCurrentLayer();
        if (layer) {
            qreal ox = 0, oy = 0;
            if (layer->type() == LayerType::Raster) {
                auto* r = static_cast<RasterLayer*>(layer);
                ox = r->originX(); oy = r->originY();
            }
            QImage img = readLayerPixels(layer);
            toCopy = img.copy(selRect_.translated(-ox, -oy).toAlignedRect());
        }
    }
    if (toCopy.isNull()) return;
    QApplication::clipboard()->setImage(toCopy);
    emit statusMessage("Copied to clipboard");
}

void CanvasWidget::cutSelection() {
    copySelection();

    if (floatingActive_) {
        Layer* layer = resolveCurrentLayer();
        if (layer && !layer->locked()) {
            pushFullLayerUndo(layer, snapCurrentLayer(), QStringLiteral("Cut"));
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

    Layer* layer = resolveCurrentLayer();
    if (layer && !layer->locked()) {
        qreal ox = 0, oy = 0;
        if (layer->type() == LayerType::Raster) {
            auto* r = static_cast<RasterLayer*>(layer);
            ox = r->originX(); oy = r->originY();
        }
        auto beforeSnap = snapCurrentLayer();
        QImage img = readLayerPixels(layer);
        {
            QPainter cp(&img);
            cp.save();
            cp.translate(-ox, -oy);
            cp.setCompositionMode(QPainter::CompositionMode_DestinationOut);
            cp.fillRect(selRect_, Qt::black);
            cp.restore();
        }
        writeLayerPixels(layer, img);
        pushFullLayerUndo(layer, beforeSnap, QStringLiteral("Cut"));
    }

    selRect_ = QRectF();
    selImage_ = QImage();
    selState_ = SelectionState::None;
    emit canvasUpdated();
    emit statusMessage("Cut to clipboard");
}

void CanvasWidget::pasteClipboard() {
    QImage clip = QApplication::clipboard()->image();
    if (clip.isNull()) return;

    if (floatingActive_ && !floatingSelection_.isNull()) {
        commitFloatingSelection();
    }

    floatingSelection_ = clip.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    originalFloatingSelection_ = floatingSelection_;
    QPointF vc = screenToCanvas(QPointF(width() / 2.0, height() / 2.0));
    int px = qRound(vc.x() - clip.width() / 2.0);
    int py = qRound(vc.y() - clip.height() / 2.0);
    selRect_ = QRectF(px, py, clip.width(), clip.height());
    selImage_ = QImage();
    floatingActive_ = true;
    selState_ = SelectionState::None;
    marchingTimer_.start();
    setTool(9);
    emit toolChangedByKey(9);
    emit canvasUpdated();
    emit statusMessage("Pasted — move/scale then Enter or click outside to commit");
}

void CanvasWidget::deleteSelection() {
    if (floatingActive_ && !floatingSelection_.isNull()) {
        Layer* layer = resolveCurrentLayer();
        if (layer && !layer->locked()) {
            pushFullLayerUndo(layer, snapCurrentLayer(), QStringLiteral("Delete Floating"));
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
        Layer* layer = resolveCurrentLayer();
        if (layer && !layer->locked()) {
            qreal ox = 0, oy = 0;
            if (layer->type() == LayerType::Raster) {
                auto* r = static_cast<RasterLayer*>(layer);
                ox = r->originX(); oy = r->originY();
            }
            auto beforeSnap = snapCurrentLayer();
            QImage img = readLayerPixels(layer);
            {
                QPainter cp(&img);
                cp.save();
                cp.translate(-ox, -oy);
                cp.setCompositionMode(QPainter::CompositionMode_DestinationOut);
                cp.fillRect(selRect_, Qt::black);
                cp.restore();
            }
            writeLayerPixels(layer, img);
            pushFullLayerUndo(layer, beforeSnap, QStringLiteral("Delete"));
        }
        selRect_ = QRectF();
        selImage_ = QImage();
        selState_ = SelectionState::None;
        emit canvasUpdated();
    }
}


void CanvasWidget::renderVectorStrokes(QPainter& p, int frame, LayerUid filterUid) {
    auto it = vectorStrokes_.find(frame);
    if (it == vectorStrokes_.end()) return;
    p.save();
    for (const auto& vs : it->second) {
        if (filterUid != 0 && vs.layerUid != filterUid) continue;
        if (vs.layerUid != 0) {
            Layer* l = currentRootLayer().layerByUid(vs.layerUid);
            if (l && !l->visible()) continue;
        }
        renderVectorStroke(p, vs);
    }
    p.restore();
}

void CanvasWidget::renderVectorStroke(QPainter& p, const RawStroke& vs) {
    if (vs.points.empty()) return;
    p.save();
    if (vs.eraser) {
        p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        float r = vs.baseWidth * 0.5f;
        for (const auto& pt : vs.points)
            p.drawEllipse(QPointF(pt.position.x, pt.position.y), r, r);
        p.restore();
        return;
    }
    if (vs.points.size() < 2) { p.restore(); return; }

    bool usePerPointOpacity = vs.pressureOpacity;
    float minW = vs.pressureSize ? vs.baseWidth * 0.2f : vs.baseWidth;
    float maxW = vs.pressureSize ? vs.baseWidth * 1.5f : vs.baseWidth;
    float padding = maxW + 4.0f;

    if (!usePerPointOpacity && vs.opacity < 0.99f && !vs.eraser) {
        float minX = vs.points[0].position.x, maxX = minX;
        float minY = vs.points[0].position.y, maxY = minY;
        for (const auto& pt : vs.points) {
            minX = std::min(minX, pt.position.x); maxX = std::max(maxX, pt.position.x);
            minY = std::min(minY, pt.position.y); maxY = std::max(maxY, pt.position.y);
        }
        int iw = static_cast<int>(std::ceil(maxX - minX + padding * 2));
        int ih = static_cast<int>(std::ceil(maxY - minY + padding * 2));
        if (iw < 2 || ih < 2) { p.restore(); return; }
        QImage layer(iw, ih, QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);
        QPainter lp(&layer);
        lp.setRenderHint(QPainter::Antialiasing);
        lp.translate(-minX + padding, -minY + padding);
        for (size_t i = 1; i < vs.points.size(); ++i) {
            float pr1 = vs.pressureSize ? vs.points[i-1].pressure : 1.0f;
            float pr2 = vs.pressureSize ? vs.points[i].pressure : 1.0f;
            float midW = (minW + (maxW - minW) * pr1 + minW + (maxW - minW) * pr2) * 0.5f;
            lp.setPen(QPen(vs.color, midW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            lp.drawLine(
                QPointF(vs.points[i-1].position.x, vs.points[i-1].position.y),
                QPointF(vs.points[i].position.x, vs.points[i].position.y));
        }
        lp.end();
        p.setOpacity(vs.opacity);
        p.drawImage(QPointF(minX - padding, minY - padding), layer);
    } else {
        for (size_t i = 1; i < vs.points.size(); ++i) {
            float pr1 = vs.pressureSize ? vs.points[i-1].pressure : 1.0f;
            float pr2 = vs.pressureSize ? vs.points[i].pressure : 1.0f;
            float midW = (minW + (maxW - minW) * pr1 + minW + (maxW - minW) * pr2) * 0.5f;
            QColor c = vs.color;
            float alpha = vs.opacity;
            if (vs.pressureOpacity) {
                float midPr = (pr1 + pr2) * 0.5f;
                alpha *= midPr;
            }
            c.setAlphaF(c.alphaF() * alpha);
            p.setPen(QPen(c, midW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawLine(
                QPointF(vs.points[i-1].position.x, vs.points[i-1].position.y),
                QPointF(vs.points[i].position.x, vs.points[i].position.y));
        }
    }
    p.restore();
}

void CanvasWidget::flattenVectorStrokes(int frame) {
    Layer* layer = resolveCurrentLayer();
    if (!layer) return;
    LayerUid layerUid = layer->uid();

    QImage img = readLayerPixels(layer);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    if (layer->type() == LayerType::Raster) {
        auto* r = static_cast<RasterLayer*>(layer);
        p.translate(-r->originX(), -r->originY());
    }
    renderVectorStrokes(p, frame, layerUid);
    p.end();
    writeLayerPixels(layer, img);

    auto& strokes = vectorStrokes_[frame];
    strokes.erase(
        std::remove_if(strokes.begin(), strokes.end(),
            [layerUid](const RawStroke& vs) { return vs.layerUid == layerUid; }),
        strokes.end());
    if (strokes.empty()) vectorStrokes_.erase(frame);
}

void CanvasWidget::floodFill(QImage& img, int sx, int sy, const QColor& fillColor, const QColor& target, int tolerance) {
    int w = img.width(), h = img.height();
    std::vector<std::pair<int,int>> stack; stack.reserve(4096); stack.push_back({sx, sy});
    while (!stack.empty()) {
        auto [px, py] = stack.back(); stack.pop_back();
        if (px < 0 || px >= w || py < 0 || py >= h) continue;
        QColor c = img.pixelColor(px, py);
        if (std::abs(c.red()-target.red())>tolerance || std::abs(c.green()-target.green())>tolerance ||
            std::abs(c.blue()-target.blue())>tolerance || std::abs(c.alpha()-target.alpha())>tolerance) continue;
        if (c == fillColor) continue;
        int lx = px;
        while (lx >= 0) {
            QColor lc = img.pixelColor(lx, py);
            if (std::abs(lc.red()-target.red())>tolerance || std::abs(lc.green()-target.green())>tolerance) break;
            img.setPixelColor(lx, py, fillColor);
            if (py > 0) stack.push_back({lx, py-1});
            if (py < h-1) stack.push_back({lx, py+1});
            lx--;
        }
        int rx = px + 1;
        while (rx < w) {
            QColor rc = img.pixelColor(rx, py);
            if (std::abs(rc.red()-target.red())>tolerance || std::abs(rc.green()-target.green())>tolerance) break;
            img.setPixelColor(rx, py, fillColor);
            if (py > 0) stack.push_back({rx, py-1});
            if (py < h-1) stack.push_back({rx, py+1});
            rx++;
        }
    }
}

void CanvasWidget::floodFillByAlpha(QImage& img, int sx, int sy, const QColor& fillColor, int boundaryAlpha) {
    int w = img.width(), h = img.height();
    std::vector<std::pair<int,int>> stack; stack.reserve(4096); stack.push_back({sx, sy});
    while (!stack.empty()) {
        auto [px, py] = stack.back(); stack.pop_back();
        if (px < 0 || px >= w || py < 0 || py >= h) continue;
        QColor c = img.pixelColor(px, py);
        if (c.alpha() >= boundaryAlpha) continue;
        if (c == fillColor) continue;
        int lx = px;
        while (lx >= 0) {
            QColor lc = img.pixelColor(lx, py);
            if (lc.alpha() >= boundaryAlpha) break;
            img.setPixelColor(lx, py, fillColor);
            if (py > 0) stack.push_back({lx, py-1});
            if (py < h-1) stack.push_back({lx, py+1});
            lx--;
        }
        int rx = px + 1;
        while (rx < w) {
            QColor rc = img.pixelColor(rx, py);
            if (rc.alpha() >= boundaryAlpha) break;
            img.setPixelColor(rx, py, fillColor);
            if (py > 0) stack.push_back({rx, py-1});
            if (py < h-1) stack.push_back({rx, py+1});
            rx++;
        }
    }
}

QImage CanvasWidget::snapCurrentLayer() {
    if (!doc_) return QImage();
    Layer* layer = resolveCurrentLayer();
    if (!layer) return QImage();
    return readLayerPixels(layer);
}

void CanvasWidget::undo() {
    if (undoStack_.canUndo()) {
        undoStack_.undo();
        emit canvasUpdated();
    }
}

void CanvasWidget::redo() {
    if (undoStack_.canRedo()) {
        undoStack_.redo();
        emit canvasUpdated();
    }
}

void CanvasWidget::purgeMemory() {
    undoStack_.clear();
    rasterEpochs_.clear();
    stabilizerBuffer_.clear();
    strokePoints_.clear();
    beforeImage_ = QImage();
    floatingSelection_ = QImage();
    originalFloatingSelection_ = QImage();
    selImage_ = QImage();
    moveImage_ = QImage();
    update();
}

void CanvasWidget::pushUndo(QUndoCommand* cmd) {
    undoStack_.push(cmd);
}

void CanvasWidget::clearCurrentFrame() {
    if (!doc_) return;
    Layer* layer = resolveCurrentLayer();
    if (layer && layer->type() == LayerType::Raster) {
        static_cast<RasterLayer*>(layer)->clear();
    }
    vectorStrokes_.erase(currentFrame_);
    emit canvasUpdated();
}

void CanvasWidget::resizeCanvas(int newW, int newH) {
    if (!doc_) return;
    if (newW == doc_->width() && newH == doc_->height()) return;

    doc_->setCanvasSize(newW, newH);

    undoStack_.clear();

    fit();
}

} // namespace fap
