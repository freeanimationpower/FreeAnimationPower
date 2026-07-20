#include "video_decoder.hpp"

#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDebug>

namespace fap {
namespace {
    thread_local bool s_inDecoder = false;
}

VideoMetadata probeVideoMetadata(const QString& filepath) {
    VideoMetadata meta;
    QProcess ffprobe;
    QStringList args;
    args << "-v" << "quiet"
         << "-print_format" << "json"
         << "-show_format"
         << "-show_streams"
         << filepath;

    ffprobe.start("ffprobe", args);
    s_inDecoder = true;
    for (int waited = 0; waited < 15000; waited += 100) {
        if (ffprobe.waitForFinished(100))
            break;
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    s_inDecoder = false;
    if (ffprobe.state() != QProcess::NotRunning) {
        qWarning() << "probeVideoMetadata: ffprobe timeout for" << filepath;
        ffprobe.kill();
        ffprobe.waitForFinished(3000);
        return meta;
    }

    QByteArray output = ffprobe.readAllStandardOutput();
    QJsonDocument doc = QJsonDocument::fromJson(output);
    if (doc.isNull()) {
        qWarning() << "probeVideoMetadata: invalid JSON from ffprobe";
        return meta;
    }

    QJsonObject root = doc.object();
    QJsonArray streams = root["streams"].toArray();
    for (auto&& s : streams) {
        QJsonObject stream = s.toObject();
        if (stream["codec_type"].toString() != "video") continue;

        meta.width  = stream["width"].toInt();
        meta.height = stream["height"].toInt();

        QString fpsStr = stream["r_frame_rate"].toString();
        auto parts = fpsStr.split('/');
        if (parts.size() == 2) {
            double num = parts[0].toDouble();
            double den = parts[1].toDouble();
            if (den > 0) meta.fps = num / den;
        } else {
            meta.fps = fpsStr.toDouble();
        }
        if (meta.fps <= 0) meta.fps = 24.0;

        meta.totalFrames = stream["nb_frames"].toInt();
        break;
    }

    QJsonObject format = root["format"].toObject();
    meta.durationSecs = format["duration"].toString().toDouble();
    if (meta.totalFrames <= 0 && meta.durationSecs > 0) {
        meta.totalFrames = static_cast<int>(meta.durationSecs * meta.fps);
    }

    meta.valid = (meta.width > 0 && meta.height > 0);
    return meta;
}

QImage decodeVideoFrame(const QString& filepath, int frameIndex,
                        double fps, int maxW, int maxH) {
    if (s_inDecoder) return {};
    double timeSecs = static_cast<double>(frameIndex) / fps;

    QProcess ffmpeg;
    QStringList args;
    args << "-ss" << QString::number(timeSecs, 'f', 3)
         << "-i" << filepath
         << "-vframes" << "1"
         << "-f" << "image2pipe"
         << "-vcodec" << "png";

    if (maxW > 0 && maxH > 0) {
        args << "-vf" << QString("scale=min(%1\\,iw):min(%2\\,ih):force_original_aspect_ratio=decrease")
                           .arg(maxW).arg(maxH);
    }

    args << "-";

    ffmpeg.start("ffmpeg", args);
    s_inDecoder = true;
    for (int waited = 0; waited < 10000; waited += 100) {
        if (ffmpeg.waitForFinished(100))
            break;
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    s_inDecoder = false;
    if (ffmpeg.state() != QProcess::NotRunning) {
        qWarning() << "decodeVideoFrame: ffmpeg timeout at frame" << frameIndex;
        ffmpeg.kill();
        ffmpeg.waitForFinished(3000);
        return {};
    }

    QByteArray pngData = ffmpeg.readAllStandardOutput();
    if (pngData.isEmpty()) {
        qWarning() << "decodeVideoFrame: empty output at frame" << frameIndex;
        return {};
    }

    QImage img = QImage::fromData(pngData, "PNG");
    if (img.isNull()) {
        qWarning() << "decodeVideoFrame: invalid PNG at frame" << frameIndex;
        return {};
    }

    return img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

int VideoFrameCache::findIndex(int frameIndex) const {
    for (size_t i = 0; i < cache_.size(); ++i) {
        if (cache_[i].frameIndex == frameIndex)
            return static_cast<int>(i);
    }
    return -1;
}

QImage VideoFrameCache::get(int frameIndex) const {
    QMutexLocker lock(&mutex_);
    int idx = findIndex(frameIndex);
    if (idx >= 0) return cache_[idx].image;
    return {};
}

void VideoFrameCache::put(int frameIndex, QImage img) {
    QMutexLocker lock(&mutex_);
    int idx = findIndex(frameIndex);
    if (idx >= 0) {
        cache_[idx].image = std::move(img);
        return;
    }
    if (cache_.size() >= static_cast<size_t>(kMaxCacheFrames)) {
        cache_.erase(cache_.begin());
    }
    cache_.push_back({frameIndex, std::move(img)});
}

void VideoFrameCache::clear() {
    QMutexLocker lock(&mutex_);
    cache_.clear();
}

bool VideoFrameCache::has(int frameIndex) const {
    QMutexLocker lock(&mutex_);
    return findIndex(frameIndex) >= 0;
}

} // namespace fap
