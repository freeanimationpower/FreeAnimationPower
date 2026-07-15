#pragma once

#include <QString>
#include <QImage>
#include <QMutex>
#include <unordered_map>
#include <string>
#include <cstdint>

namespace fap {

struct VideoMetadata {
    int width = 0;
    int height = 0;
    double fps = 24.0;
    int totalFrames = 0;
    double durationSecs = 0.0;
    bool valid = false;
};

VideoMetadata probeVideoMetadata(const QString& filepath);

QImage decodeVideoFrame(const QString& filepath, int frameIndex,
                        double fps, int maxW = 1920, int maxH = 1080);

class VideoFrameCache {
public:
    static constexpr int kMaxCacheFrames = 50;

    QImage get(int frameIndex) const;
    void put(int frameIndex, QImage img);
    void clear();
    bool has(int frameIndex) const;

private:
    struct Entry {
        int frameIndex;
        QImage image;
    };
    mutable QMutex mutex_;
    std::vector<Entry> cache_;
    int findIndex(int frameIndex) const;
};

} // namespace fap
