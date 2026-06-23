#include "frame_thumbnail.hpp"
#include "core/document.hpp"
#include "core/layer.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace fap {

FrameThumbnailCache::FrameThumbnailCache(int thumbnailWidth, int thumbnailHeight)
    : thumb_width_(thumbnailWidth)
    , thumb_height_(thumbnailHeight)
{
}

void FrameThumbnailCache::setThumbnailSize(int w, int h)
{
    if (w != thumb_width_ || h != thumb_height_) {
        thumb_width_ = w;
        thumb_height_ = h;
        invalidateAll();
    }
}

void FrameThumbnailCache::invalidateFrame(int frame)
{
    auto* entry = findEntry(frame);
    if (entry) {
        entry->dirty = true;
    }
}

void FrameThumbnailCache::invalidateAll()
{
    for (auto& entry : cache_) {
        entry.dirty = true;
    }
}

ThumbnailCacheEntry* FrameThumbnailCache::findEntry(int frame)
{
    for (auto& entry : cache_) {
        if (entry.frame == frame) return &entry;
    }
    return nullptr;
}

const ThumbnailCacheEntry* FrameThumbnailCache::findEntry(int frame) const
{
    for (const auto& entry : cache_) {
        if (entry.frame == frame) return &entry;
    }
    return nullptr;
}

const ThumbnailCacheEntry* FrameThumbnailCache::getThumbnail(int frame)
{
    auto* entry = findEntry(frame);
    if (!entry) {
        ThumbnailCacheEntry newEntry;
        newEntry.frame = frame;
        newEntry.dirty = true;
        cache_.push_back(newEntry);
        entry = &cache_.back();
    }

    if (entry->dirty) {
        generateThumbnail(frame);
        entry->dirty = false;
    }

    return entry;
}

void FrameThumbnailCache::generateThumbnail(int frame)
{
    if (!document_) return;

    auto* entry = findEntry(frame);
    if (!entry) return;

    entry->width = thumb_width_;
    entry->height = thumb_height_;
    entry->pixels.assign(thumb_width_ * thumb_height_, 0);

    const GroupLayer* root = document_->peekRootLayerForFrame(frame);
    if (!root || root->layerCount() == 0) return;

    int docW = document_->width();
    int docH = document_->height();
    std::vector<uint32_t> fullPixels(docW * docH, 0);

    for (const auto& layer : root->layers()) {
        if (!layer || !layer->visible() || layer->type() != LayerType::Raster) continue;
        const auto* raster = static_cast<const RasterLayer*>(layer.get());
        if (!raster) continue;

        int lw = raster->width();
        int lh = raster->height();
        int ox = raster->originX();
        int oy = raster->originY();
        const auto* src = raster->pixelData();

        for (int y = 0; y < lh; ++y) {
            int globalY = oy + y;
            if (globalY < 0 || globalY >= docH) continue;
            for (int x = 0; x < lw; ++x) {
                int globalX = ox + x;
                if (globalX < 0 || globalX >= docW) continue;
                size_t srcIdx = static_cast<size_t>(y) * lw + x;
                size_t dstIdx = static_cast<size_t>(globalY) * docW + globalX;
                uint32_t px = src[srcIdx];
                if ((px >> 24) > 0) {
                    fullPixels[dstIdx] = px;
                }
            }
        }
    }

    downscaleToThumbnail(fullPixels.data(), docW, docH, *entry);
}

void FrameThumbnailCache::downscaleToThumbnail(const uint32_t* src, int srcW, int srcH,
                                                ThumbnailCacheEntry& entry)
{
    float scaleX = static_cast<float>(srcW) / thumb_width_;
    float scaleY = static_cast<float>(srcH) / thumb_height_;

    for (int y = 0; y < thumb_height_; ++y) {
        int srcY = static_cast<int>(y * scaleY);
        for (int x = 0; x < thumb_width_; ++x) {
            int srcX = static_cast<int>(x * scaleX);
            if (srcX >= 0 && srcX < srcW && srcY >= 0 && srcY < srcH) {
                size_t srcIdx = static_cast<size_t>(srcY) * srcW + srcX;
                size_t dstIdx = static_cast<size_t>(y) * thumb_width_ + x;
                entry.pixels[dstIdx] = src[srcIdx];
            }
        }
    }
}

void FrameThumbnailCache::preloadRange(int fromFrame, int toFrame)
{
    for (int f = fromFrame; f <= toFrame && f >= 0; ++f) {
        getThumbnail(f);
    }
}

FrameViewerData::FrameInfo FrameViewerData::infoForFrame(int frame) const
{
    FrameInfo info;
    info.frame_number = frame;

    if (!document_) return info;

    info.frame_width = document_->width();
    info.frame_height = document_->height();

    const GroupLayer* root = document_->peekRootLayerForFrame(frame);
    if (root && root->layerCount() > 0) {
        info.has_content = true;
    }

    return info;
}

std::vector<FrameViewerData::FrameInfo> FrameViewerData::allFrameInfos() const
{
    std::vector<FrameInfo> infos;
    if (!document_) return infos;

    int total = document_->totalFrames();
    infos.reserve(total);
    for (int f = 0; f < total; ++f) {
        infos.push_back(infoForFrame(f));
    }
    return infos;
}

int FrameViewerData::fps() const
{
    return document_ ? document_->fps() : 24;
}

int FrameViewerData::totalFrames() const
{
    return document_ ? document_->totalFrames() : 1;
}

int FrameViewerData::canvasWidth() const
{
    return document_ ? document_->width() : 1920;
}

int FrameViewerData::canvasHeight() const
{
    return document_ ? document_->height() : 1080;
}

} // namespace fap
