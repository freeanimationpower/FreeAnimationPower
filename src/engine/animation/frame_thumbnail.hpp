#pragma once

#include "core/types.hpp"
#include <vector>
#include <cstdint>
#include <memory>
#include <optional>

namespace fap {

class Document;

struct ThumbnailCacheEntry {
    int frame = 0;
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels;
    bool dirty = true;
};

class FrameThumbnailCache {
public:
    FrameThumbnailCache(int thumbnailWidth = 160, int thumbnailHeight = 90);

    int thumbnailWidth() const { return thumb_width_; }
    int thumbnailHeight() const { return thumb_height_; }
    void setThumbnailSize(int w, int h);

    void setDocument(Document* doc) { document_ = doc; }

    void invalidateFrame(int frame);
    void invalidateAll();

    const ThumbnailCacheEntry* getThumbnail(int frame);
    void generateThumbnail(int frame);

    int cachedCount() const { return static_cast<int>(cache_.size()); }
    void preloadRange(int fromFrame, int toFrame);

private:
    int thumb_width_;
    int thumb_height_;
    Document* document_ = nullptr;
    std::vector<ThumbnailCacheEntry> cache_;

    ThumbnailCacheEntry* findEntry(int frame);
    const ThumbnailCacheEntry* findEntry(int frame) const;
    void downscaleToThumbnail(const uint32_t* src, int srcW, int srcH,
                              ThumbnailCacheEntry& entry);
};

class FrameViewerData {
public:
    struct FrameInfo {
        int frame_number = 0;
        int frame_width = 0;
        int frame_height = 0;
        bool has_content = false;
        bool is_keyframe = false;
    };

    void setDocument(Document* doc) { document_ = doc; }

    FrameInfo infoForFrame(int frame) const;
    std::vector<FrameInfo> allFrameInfos() const;

    int fps() const;
    int totalFrames() const;
    int canvasWidth() const;
    int canvasHeight() const;

private:
    Document* document_ = nullptr;
};

} // namespace fap
