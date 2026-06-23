#include "io/document_manager.hpp"
#include "io/serialization_common.hpp"
#include "core/document.hpp"
#include "core/layer.hpp"
#include "core/types.hpp"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QDebug>

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_set>

namespace fap {

using namespace io::utils;

QJsonObject DocumentManager::layerMetadataToJson(const Layer& layer) {
    QJsonObject obj;
    obj[QStringLiteral("name")]       = QString::fromStdString(layer.name());
    obj[QStringLiteral("type")]       = layerTypeToString(layer.type());
    obj[QStringLiteral("visible")]    = layer.visible();
    obj[QStringLiteral("opacity")]    = static_cast<double>(layer.opacity());
    obj[QStringLiteral("blend_mode")] = blendModeToString(layer.blendMode());
    obj[QStringLiteral("locked")]     = layer.locked();

    if (layer.type() == LayerType::Raster) {
        const auto& rl = static_cast<const RasterLayer&>(layer);
        obj[QStringLiteral("width")]    = rl.width();
        obj[QStringLiteral("height")]   = rl.height();
        obj[QStringLiteral("origin_x")] = rl.originX();
        obj[QStringLiteral("origin_y")] = rl.originY();
    }

    if (layer.type() == LayerType::Group) {
        const auto& gl = static_cast<const GroupLayer&>(layer);
        QJsonArray children;
        for (size_t i = 0; i < gl.layerCount(); ++i) {
            children.append(layerMetadataToJson(*gl.layers()[i]));
        }
        obj[QStringLiteral("children")] = children;
    }

    return obj;
}

std::unique_ptr<Layer> DocumentManager::layerMetadataFromJson(const QJsonObject& obj) {
    QString name    = obj[QStringLiteral("name")].toString(QStringLiteral("Layer"));
    LayerType type  = layerTypeFromString(obj[QStringLiteral("type")].toString(QStringLiteral("raster")));
    bool visible    = obj[QStringLiteral("visible")].toBool(true);
    double opacity  = obj[QStringLiteral("opacity")].toDouble(1.0);
    BlendMode blend = blendModeFromString(obj[QStringLiteral("blend_mode")].toString(QStringLiteral("normal")));
    bool locked     = obj[QStringLiteral("locked")].toBool(false);

    std::unique_ptr<Layer> layer;

    switch (type) {
        case LayerType::Raster: {
            int w = obj[QStringLiteral("width")].toInt(1920);
            int h = obj[QStringLiteral("height")].toInt(1080);
            auto rl = std::make_unique<RasterLayer>(name.toStdString(), w, h);
            layer = std::move(rl);
            break;
        }
        case LayerType::Vector:
            layer = std::make_unique<VectorLayer>(name.toStdString());
            break;
        case LayerType::Group: {
            auto group = std::make_unique<GroupLayer>(name.toStdString());
            QJsonArray children = obj[QStringLiteral("children")].toArray();
            for (const auto& child : children) {
                auto childLayer = layerMetadataFromJson(child.toObject());
                if (childLayer) {
                    group->addLayer(std::move(childLayer));
                }
            }
            layer = std::move(group);
            break;
        }
        case LayerType::Audio:
        case LayerType::Camera:
            layer = std::make_unique<RasterLayer>(name.toStdString(), 1, 1);
            break;
    }

    if (layer) {
        layer->setVisible(visible);
        layer->setOpacity(static_cast<float>(opacity));
        layer->setBlendMode(blend);
        layer->setLocked(locked);
    }

    return layer;
}

QJsonObject DocumentManager::documentToJson(const Document& doc) {
    QJsonObject root;
    root[QStringLiteral("version")]      = kFormatVersion;
    root[QStringLiteral("canvas_w")]     = doc.width();
    root[QStringLiteral("canvas_h")]     = doc.height();
    root[QStringLiteral("fps")]          = doc.fps();
    root[QStringLiteral("total_frames")] = doc.totalFrames();
    return root;
}

bool DocumentManager::documentFromJson(const QJsonObject& root, Document& doc) {
    int version = root[QStringLiteral("version")].toInt(1);
    Q_UNUSED(version);
    doc.setCanvasSize(
        root[QStringLiteral("canvas_w")].toInt(1920),
        root[QStringLiteral("canvas_h")].toInt(1080));
    doc.setFPS(root[QStringLiteral("fps")].toInt(24));
    doc.setTotalFrames(root[QStringLiteral("total_frames")].toInt(1));
    return true;
}

QString DocumentManager::buildLayerPath(int frameIdx, int layerModelIdx,
                                        const char* ext) {
    return QStringLiteral("layers/frame_%1/layer_%2.%3")
        .arg(frameIdx).arg(layerModelIdx, 2, 10, QLatin1Char('0'))
        .arg(QString::fromLatin1(ext));
}

DocumentManager::DocumentManager() = default;
DocumentManager::~DocumentManager() = default;

QString DocumentManager::lastError() const {
    QMutexLocker lock(&mutex_);
    return lastError_;
}

bool DocumentManager::isBusy() const {
    QMutexLocker lock(&mutex_);
    return busy_;
}

bool DocumentManager::prepareAtomicTarget(const QString& finalPath,
                                          QString& tmpPath) {
    tmpPath = finalPath + QStringLiteral(".tmp");
    return true;
}

bool DocumentManager::commitAtomic(const QString& tmpPath,
                                   const QString& finalPath) {
    if (QFile::exists(finalPath)) {
        if (!QFile::remove(finalPath)) {
            lastError_ = QStringLiteral("Cannot remove existing file: ") + finalPath;
            QFile::remove(tmpPath);
            return false;
        }
    }
    if (!QFile::rename(tmpPath, finalPath)) {
        lastError_ = QStringLiteral("Atomic rename failed: ") + tmpPath
                   + QStringLiteral(" -> ") + finalPath;
        QFile::remove(tmpPath);
        return false;
    }
    return true;
}

bool DocumentManager::save(const Document& doc, const QString& path) {
    QMutexLocker lock(&mutex_);
    if (busy_) {
        lastError_ = QStringLiteral("DocumentManager is busy");
        return false;
    }
    busy_ = true;
    lock.unlock();

    QString tmpPath;
    if (!prepareAtomicTarget(path, tmpPath)) {
        QMutexLocker relock(&mutex_);
        busy_ = false;
        return false;
    }

    mz_zip_archive zip{};
    QByteArray tmpPathUtf8 = tmpPath.toUtf8();
    if (!mz_zip_writer_init_file(&zip, tmpPathUtf8.constData(), 0)) {
        lastError_ = QStringLiteral("Failed to initialize ZIP writer");
        QFile::remove(tmpPath);
        QMutexLocker relock(&mutex_);
        busy_ = false;
        return false;
    }

    bool ok = true;

    if (ok && !writeManifest(&zip, doc)) {
        ok = false;
    }
    if (ok && !writeTimeline(&zip, doc)) {
        ok = false;
    }
    if (ok && !writeLayerData(&zip, doc)) {
        ok = false;
    }

    if (ok) {
        if (!mz_zip_writer_finalize_archive(&zip)) {
            lastError_ = QStringLiteral("Failed to finalize ZIP archive");
            ok = false;
        }
    }

    mz_zip_writer_end(&zip);

    if (!ok) {
        QFile::remove(tmpPath);
        QMutexLocker relock(&mutex_);
        busy_ = false;
        return false;
    }

    if (!commitAtomic(tmpPath, path)) {
        QMutexLocker relock(&mutex_);
        busy_ = false;
        return false;
    }

    QMutexLocker relock(&mutex_);
    busy_ = false;
    return true;
}

bool DocumentManager::writeManifest(mz_zip_archive* zip, const Document& doc) {
    QJsonObject root = documentToJson(doc);
    QJsonDocument jdoc(root);
    QByteArray data = jdoc.toJson(QJsonDocument::Indented);
    if (!mz_zip_writer_add_mem(zip, kManifestEntry, data.constData(),
                                data.size(), MZ_DEFAULT_COMPRESSION)) {
        lastError_ = QStringLiteral("Failed to write ") +
                     QString::fromLatin1(kManifestEntry);
        return false;
    }
    return true;
}

bool DocumentManager::writeTimeline(mz_zip_archive* zip, const Document& doc) {
    QJsonObject root;
    root[QStringLiteral("version")]      = kFormatVersion;
    root[QStringLiteral("total_frames")] = doc.totalFrames();
    root[QStringLiteral("fps")]          = doc.fps();

    QJsonArray framesArr;
    for (int f = 0; f < doc.totalFrames(); ++f) {
        const auto& frameRoot = doc.rootLayerForFrame(f);
        QJsonObject frameObj;
        frameObj[QStringLiteral("index")] = f;
        QJsonArray layersArr;
        for (size_t li = 0; li < frameRoot.layerCount(); ++li) {
            const auto& layerPtr = frameRoot.layers()[li];
            QJsonObject lmeta = layerMetadataToJson(*layerPtr);
            lmeta[QStringLiteral("pixel_file")] =
                buildLayerPath(f, static_cast<int>(li), "png");
            lmeta[QStringLiteral("meta_file")] =
                buildLayerPath(f, static_cast<int>(li), "json");
            layersArr.append(lmeta);
        }
        frameObj[QStringLiteral("layers")] = layersArr;
        framesArr.append(frameObj);
    }
    root[QStringLiteral("frames")] = framesArr;

    QJsonDocument jdoc(root);
    QByteArray data = jdoc.toJson(QJsonDocument::Indented);
    if (!mz_zip_writer_add_mem(zip, kTimelineEntry, data.constData(),
                                data.size(), MZ_DEFAULT_COMPRESSION)) {
        lastError_ = QStringLiteral("Failed to write ") +
                     QString::fromLatin1(kTimelineEntry);
        return false;
    }
    return true;
}

bool DocumentManager::writeLayerData(mz_zip_archive* zip, const Document& doc) {
    std::unordered_set<const void*> writtenPixelBuffers;

    for (int f = 0; f < doc.totalFrames(); ++f) {
        const auto& frameRoot = doc.rootLayerForFrame(f);
        for (size_t li = 0; li < frameRoot.layerCount(); ++li) {
            const Layer* layer = frameRoot.layers()[li].get();

            QString metaPath = buildLayerPath(f, static_cast<int>(li), "json");
            QJsonObject lmeta = layerMetadataToJson(*layer);

            if (layer->type() == LayerType::Raster) {
                const auto& rl = static_cast<const RasterLayer&>(*layer);
                const void* pixelPtr = static_cast<const void*>(rl.pixelData());

                if (writtenPixelBuffers.count(pixelPtr)) {
                    lmeta[QStringLiteral("shared_pixels")] = true;
                } else {
                    writtenPixelBuffers.insert(pixelPtr);

                    QImage layerImage(
                        reinterpret_cast<const uchar*>(rl.pixelData()),
                        rl.width(), rl.height(),
                        rl.width() * static_cast<int>(sizeof(uint32_t)),
                        QImage::Format_ARGB32_Premultiplied);

                    if (layerImage.isNull()) {
                        lastError_ = QStringLiteral("Failed to create image from raster layer");
                        return false;
                    }

                    QByteArray pngData;
                    QBuffer pngBuffer(&pngData);
                    if (!pngBuffer.open(QIODevice::WriteOnly)) {
                        lastError_ = QStringLiteral("Failed to open PNG buffer");
                        return false;
                    }
                    if (!layerImage.save(&pngBuffer, "PNG")) {
                        lastError_ = QStringLiteral("Failed to encode layer %1 frame %2 as PNG")
                            .arg(static_cast<int>(li)).arg(f);
                        return false;
                    }

                    QString pngPath = buildLayerPath(f, static_cast<int>(li), "png");
                    QByteArray pngPathUtf8 = pngPath.toUtf8();
                    if (!mz_zip_writer_add_mem(zip, pngPathUtf8.constData(),
                                                pngData.constData(), pngData.size(),
                                                MZ_DEFAULT_COMPRESSION)) {
                        lastError_ = QStringLiteral("Failed to write PNG: ") + pngPath;
                        return false;
                    }
                }
            }

            QJsonDocument metaDoc(lmeta);
            QByteArray metaData = metaDoc.toJson(QJsonDocument::Indented);
            QByteArray metaPathUtf8 = metaPath.toUtf8();
            if (!mz_zip_writer_add_mem(zip, metaPathUtf8.constData(),
                                        metaData.constData(), metaData.size(),
                                        MZ_DEFAULT_COMPRESSION)) {
                lastError_ = QStringLiteral("Failed to write layer metadata: ") + metaPath;
                return false;
            }
        }
    }
    return true;
}

bool DocumentManager::load(Document& doc, const QString& path) {
    QMutexLocker lock(&mutex_);
    if (busy_) {
        lastError_ = QStringLiteral("DocumentManager is busy");
        return false;
    }
    busy_ = true;
    lock.unlock();

    if (!QFileInfo::exists(path)) {
        lastError_ = QStringLiteral("File not found: ") + path;
        QMutexLocker relock(&mutex_);
        busy_ = false;
        return false;
    }

    mz_zip_archive zip{};
    QByteArray pathUtf8 = path.toUtf8();
    if (!mz_zip_reader_init_file(&zip, pathUtf8.constData(), 0)) {
        lastError_ = QStringLiteral("Failed to open archive: ") + path;
        QMutexLocker relock(&mutex_);
        busy_ = false;
        return false;
    }

    bool ok = true;

    if (ok && !readManifest(&zip, doc)) {
        ok = false;
    }
    if (ok && !readTimeline(&zip, doc)) {
        ok = false;
    }
    if (ok && !readLayerData(&zip, doc)) {
        ok = false;
    }

    mz_zip_reader_end(&zip);

    if (!ok) {
        QMutexLocker relock(&mutex_);
        busy_ = false;
        return false;
    }

    doc.setModified(false);

    QMutexLocker relock(&mutex_);
    busy_ = false;
    return true;
}

bool DocumentManager::readManifest(mz_zip_archive* zip, Document& doc) {
    size_t size = 0;
    void* data = mz_zip_reader_extract_file_to_heap(zip, kManifestEntry, &size, 0);
    if (!data) {
        lastError_ = QStringLiteral("Missing ") +
                     QString::fromLatin1(kManifestEntry) +
                     QStringLiteral(" in archive");
        return false;
    }

    QByteArray rawData(static_cast<const char*>(data), static_cast<int>(size));
    mz_free(data);

    QJsonParseError err;
    QJsonDocument jdoc = QJsonDocument::fromJson(rawData, &err);
    if (err.error != QJsonParseError::NoError) {
        lastError_ = QStringLiteral("manifest.json parse error: ") + err.errorString();
        return false;
    }

    if (!jdoc.isObject()) {
        lastError_ = QStringLiteral("manifest.json is not a JSON object");
        return false;
    }

    return documentFromJson(jdoc.object(), doc);
}

bool DocumentManager::readTimeline(mz_zip_archive* zip, Document& doc) {
    size_t size = 0;
    void* data = mz_zip_reader_extract_file_to_heap(zip, kTimelineEntry, &size, 0);
    if (!data) {
        lastError_ = QStringLiteral("Missing ") +
                     QString::fromLatin1(kTimelineEntry) +
                     QStringLiteral(" in archive");
        return false;
    }

    QByteArray rawData(static_cast<const char*>(data), static_cast<int>(size));
    mz_free(data);

    QJsonParseError err;
    QJsonDocument jdoc = QJsonDocument::fromJson(rawData, &err);
    if (err.error != QJsonParseError::NoError) {
        lastError_ = QStringLiteral("timeline.json parse error: ") + err.errorString();
        return false;
    }

    QJsonObject root = jdoc.object();
    QJsonArray framesArr = root[QStringLiteral("frames")].toArray();

    for (const auto& frameVal : framesArr) {
        QJsonObject frameObj = frameVal.toObject();
        int frameIdx = frameObj[QStringLiteral("index")].toInt(0);
        auto& frameRoot = doc.rootLayerForFrame(frameIdx);

        QJsonArray layersArr = frameObj[QStringLiteral("layers")].toArray();
        for (const auto& layerVal : layersArr) {
            QJsonObject lobj = layerVal.toObject();
            QString metaFile = lobj[QStringLiteral("meta_file")].toString();

            std::unique_ptr<Layer> layer;
            if (!metaFile.isEmpty()) {
                size_t metaSize = 0;
                void* metaData = mz_zip_reader_extract_file_to_heap(
                    zip, metaFile.toUtf8().constData(), &metaSize, 0);
                if (metaData) {
                    QByteArray metaRaw(static_cast<const char*>(metaData),
                                       static_cast<int>(metaSize));
                    mz_free(metaData);

                    QJsonDocument metaDoc = QJsonDocument::fromJson(metaRaw);
                    if (metaDoc.isObject()) {
                        layer = layerMetadataFromJson(metaDoc.object());
                    }
                }
            }

            if (!layer) {
                layer = layerMetadataFromJson(lobj);
            }

            if (layer) {
                frameRoot.addLayer(std::move(layer));
            }
        }
    }

    return true;
}

bool DocumentManager::readLayerData(mz_zip_archive* zip, Document& doc) {
    for (int f = 0; f < doc.totalFrames(); ++f) {
        auto& frameRoot = doc.rootLayerForFrame(f);
        for (size_t li = 0; li < frameRoot.layerCount(); ++li) {
            Layer* layer = frameRoot.layers()[li].get();
            if (!layer || layer->type() != LayerType::Raster) continue;

            auto* rl = static_cast<RasterLayer*>(layer);
            QString pngPath = buildLayerPath(f, static_cast<int>(li), "png");
            QByteArray pngPathUtf8 = pngPath.toUtf8();

            size_t pngSize = 0;
            void* pngData = mz_zip_reader_extract_file_to_heap(
                zip, pngPathUtf8.constData(), &pngSize, 0);
            if (!pngData) continue;

            QImage png;
            bool loaded = png.loadFromData(static_cast<const uchar*>(pngData),
                                           static_cast<int>(pngSize), "PNG");
            mz_free(pngData);

            if (!loaded || png.isNull()) {
                qWarning() << "[DocumentManager] Failed to decode PNG:" << pngPath;
                continue;
            }

            QImage converted = png.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            rl->ensureContains(rl->originX(), rl->originY(),
                               converted.width(), converted.height());

            for (int y = 0; y < std::min(converted.height(), rl->height()); ++y) {
                const uint32_t* src =
                    reinterpret_cast<const uint32_t*>(converted.constScanLine(y));
                if (!src) continue;
                uint32_t* dst = rl->pixelData() +
                                static_cast<size_t>(y) * static_cast<size_t>(rl->width());
                std::copy(src,
                          src + std::min(static_cast<size_t>(converted.width()),
                                         static_cast<size_t>(rl->width())),
                          dst);
            }
            rl->bufferEpochTick();
        }
    }
    return true;
}

} // namespace fap
