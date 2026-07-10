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
#include <QDir>
#include <QCoreApplication>

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_set>

#include "core/diagnostic/tracer_macros.hpp"

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
        obj[QStringLiteral("width")]       = rl.width();
        obj[QStringLiteral("height")]      = rl.height();
        obj[QStringLiteral("origin_x")]    = rl.originX();
        obj[QStringLiteral("origin_y")]    = rl.originY();
        obj[QStringLiteral("has_content")] = rl.hasContent();
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
            rl->setOrigin(obj[QStringLiteral("origin_x")].toInt(0),
                          obj[QStringLiteral("origin_y")].toInt(0));
            rl->setHasContent(obj[QStringLiteral("has_content")].toBool(false));
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
    root[QStringLiteral("version")]          = kFormatVersion;
    root[QStringLiteral("canvas_w")]         = doc.width();
    root[QStringLiteral("canvas_h")]         = doc.height();
    root[QStringLiteral("active_sequence")]  = static_cast<int>(doc.activeSequenceIndex());

    if (viewState_.zoom > 0) {
        root[QStringLiteral("view_zoom")]    = static_cast<double>(viewState_.zoom);
        root[QStringLiteral("view_offset_x")] = static_cast<double>(viewState_.offsetX);
        root[QStringLiteral("view_offset_y")] = static_cast<double>(viewState_.offsetY);
    }
    qDebug() << "WRITE manifest viewState zoom:" << viewState_.zoom
             << "offsetX:" << viewState_.offsetX << "offsetY:" << viewState_.offsetY;

    QJsonArray seqArr;
    for (size_t si = 0; si < doc.sequenceCount(); ++si) {
        const auto& seq = doc.sequenceAt(si);
        QJsonObject so;
        so[QStringLiteral("name")]            = QString::fromStdString(seq.name());
        so[QStringLiteral("fps")]             = seq.fps();
        so[QStringLiteral("total_frames")]    = seq.totalFrames();
        so[QStringLiteral("current_frame")]   = seq.currentFrame();
        so[QStringLiteral("visible")]         = seq.visible();
        so[QStringLiteral("opacity")]         = static_cast<double>(seq.opacity());
        so[QStringLiteral("locked")]          = seq.locked();
        so[QStringLiteral("work_area_start")] = seq.workAreaStart();
        so[QStringLiteral("work_area_end")]   = seq.workAreaEnd();
        so[QStringLiteral("duration_frames")] = seq.durationFrames();
        so[QStringLiteral("looping")]         = seq.looping();
        seqArr.append(so);
    }
    root[QStringLiteral("sequences")] = seqArr;
    return root;
}

bool DocumentManager::documentFromJson(const QJsonObject& root, Document& doc) {
    int version = root[QStringLiteral("version")].toInt(1);
    Q_UNUSED(version);
    doc.setCanvasSize(
        root[QStringLiteral("canvas_w")].toInt(1920),
        root[QStringLiteral("canvas_h")].toInt(1080));

    viewState_.zoom    = static_cast<float>(root[QStringLiteral("view_zoom")].toDouble(0.0));
    viewState_.offsetX = static_cast<float>(root[QStringLiteral("view_offset_x")].toDouble(0.0));
    viewState_.offsetY = static_cast<float>(root[QStringLiteral("view_offset_y")].toDouble(0.0));

    qDebug() << "READ manifest view_zoom:" << viewState_.zoom
             << "offsetX:" << viewState_.offsetX << "offsetY:" << viewState_.offsetY;

    QJsonArray seqArr = root[QStringLiteral("sequences")].toArray();
    for (int si = 0; si < seqArr.size(); ++si) {
        QJsonObject so = seqArr[si].toObject();
        std::string sname = so[QStringLiteral("name")].toString("Sequence").toStdString();
        auto& seq = (si == 0) ? doc.activeSequence() : doc.addSequence(sname);
        seq.setName(sname);
        seq.setFPS(so[QStringLiteral("fps")].toInt(24));
        seq.setTotalFrames(so[QStringLiteral("total_frames")].toInt(1));
        seq.setCurrentFrame(so[QStringLiteral("current_frame")].toInt(0));
        seq.setVisible(so[QStringLiteral("visible")].toBool(true));
        seq.setOpacity(static_cast<float>(so[QStringLiteral("opacity")].toDouble(1.0)));
        seq.setLocked(so[QStringLiteral("locked")].toBool(false));
        seq.setWorkAreaStart(so[QStringLiteral("work_area_start")].toInt(0));
        seq.setWorkAreaEnd(so[QStringLiteral("work_area_end")].toInt(0));
        seq.setDurationFrames(so[QStringLiteral("duration_frames")].toInt(seq.totalFrames()));
        seq.setLooping(so[QStringLiteral("looping")].toBool(false));
    }

    int activeIdx = root[QStringLiteral("active_sequence")].toInt(0);
    doc.setActiveSequence(static_cast<size_t>(activeIdx));
    return true;
}

QString DocumentManager::buildLayerPath(int frameIdx, int seqIdx, int layerIdx,
                                        const char* ext) {
    return QStringLiteral("layers/S%1/frame_%2/layer_%3.%4")
        .arg(seqIdx)
        .arg(frameIdx)
        .arg(layerIdx, 2, 10, QLatin1Char('0'))
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

bool DocumentManager::save(const Document& doc, const QString& path, const ViewState& vs) {
    QMutexLocker lock(&mutex_);
    viewState_ = vs;
    if (busy_) {
        lastError_ = QStringLiteral("DocumentManager is busy");
        return false;
    }
    busy_ = true;
    lock.unlock();

    FAP_TRACE_IO("save_begin", path.toStdString(), 0.0);

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
    FAP_TRACE_IO("save_end", path.toStdString(), 0.0);
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
    root[QStringLiteral("version")] = kFormatVersion;

    QJsonArray seqArr;
    for (size_t si = 0; si < doc.sequenceCount(); ++si) {
        const auto& seq = doc.sequenceAt(si);
        QJsonObject seqObj;
        seqObj[QStringLiteral("index")]          = static_cast<int>(si);
        seqObj[QStringLiteral("total_frames")]   = seq.totalFrames();
        seqObj[QStringLiteral("fps")]            = seq.fps();

        QJsonArray framesArr;
        for (int f = 0; f < seq.totalFrames(); ++f) {
            const auto* rootLayer = seq.peekRootLayerForFrame(f);
            if (!rootLayer) continue;
            QJsonObject frameObj;
            frameObj[QStringLiteral("index")] = f;
            QJsonArray layersArr;
            for (size_t li = 0; li < rootLayer->layerCount(); ++li) {
                const auto& layerPtr = rootLayer->layers()[li];
                QJsonObject lmeta = layerMetadataToJson(*layerPtr);
                lmeta[QStringLiteral("pixel_file")] =
                    buildLayerPath(f, static_cast<int>(si), static_cast<int>(li), "png");
                lmeta[QStringLiteral("meta_file")] =
                    buildLayerPath(f, static_cast<int>(si), static_cast<int>(li), "json");
                layersArr.append(lmeta);
            }
            frameObj[QStringLiteral("layers")] = layersArr;
            framesArr.append(frameObj);
        }
        seqObj[QStringLiteral("frames")] = framesArr;
        seqArr.append(seqObj);
    }
    root[QStringLiteral("sequences")] = seqArr;

    QJsonArray audioArr;
    size_t ai = 0;
    for (const auto& at : doc.audioTracks()) {
        QJsonObject ao;
        ao[QStringLiteral("filepath")]     = QString::fromStdString(at.filepath);
        ao[QStringLiteral("display_name")] = QString::fromStdString(at.displayName);
        ao[QStringLiteral("zip_entry")]    = QString::fromStdString(at.zipEntry);
        ao[QStringLiteral("muted")]        = at.muted;
        ao[QStringLiteral("volume")]       = at.volume;
        audioArr.append(ao);
        ++ai;
    }
    root[QStringLiteral("audio")] = audioArr;

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

    for (size_t si = 0; si < doc.sequenceCount(); ++si) {
        const auto& seq = doc.sequenceAt(si);
        for (int f = 0; f < seq.totalFrames(); ++f) {
            const auto* rootLayer = seq.peekRootLayerForFrame(f);
            if (!rootLayer) continue;
            for (size_t li = 0; li < rootLayer->layerCount(); ++li) {
                const Layer* layer = rootLayer->layers()[li].get();

                QString metaPath = buildLayerPath(f, static_cast<int>(si),
                                                  static_cast<int>(li), "json");
                QJsonObject lmeta = layerMetadataToJson(*layer);

                if (layer->type() == LayerType::Raster) {
                    const auto& rl = static_cast<const RasterLayer&>(*layer);
                    const void* pixelPtr = static_cast<const void*>(rl.pixelData());

                    qDebug() << "SAVE layer:" << rl.name().c_str()
                             << "origin:" << rl.originX() << "," << rl.originY()
                             << "size:" << rl.width() << "x" << rl.height();

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
                            lastError_ = QStringLiteral("Failed to encode layer as PNG");
                            return false;
                        }

                        QString pngPath = buildLayerPath(f, static_cast<int>(si),
                                                         static_cast<int>(li), "png");
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
    }

    // Embed audio files into ZIP
    for (size_t ai = 0; ai < doc.audioTracks().size(); ++ai) {
        const auto& at = doc.audioTracks()[ai];
        QFile audioFile(QString::fromStdString(at.filepath));
        if (audioFile.open(QIODevice::ReadOnly)) {
            QByteArray audioData = audioFile.readAll();
            audioFile.close();
            QByteArray entryPathUtf8 = QByteArray::fromStdString(at.zipEntry);
            if (!mz_zip_writer_add_mem(zip, entryPathUtf8.constData(),
                                        audioData.constData(), audioData.size(),
                                        MZ_DEFAULT_COMPRESSION)) {
                qWarning() << "Failed to embed audio:" << QString::fromStdString(at.zipEntry);
            }
        } else {
            qWarning() << "Cannot open audio file for embedding:"
                       << QString::fromStdString(at.filepath);
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

    FAP_TRACE_IO("load_begin", path.toStdString(), 0.0);

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
    if (ok && !extractAudio(&zip, doc)) {
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
    FAP_TRACE_IO("load_end", path.toStdString(), 0.0);
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
    QJsonArray seqArr = root[QStringLiteral("sequences")].toArray();

    for (int si = 0; si < seqArr.size(); ++si) {
        QJsonObject seqObj = seqArr[si].toObject();
        int sframeCount = seqObj[QStringLiteral("total_frames")].toInt(1);
        auto& seq = (si == 0) ? doc.activeSequence() : doc.addSequence("");
        seq.setTotalFrames(sframeCount);
        seq.setFPS(seqObj[QStringLiteral("fps")].toInt(24));

        QJsonArray framesArr = seqObj[QStringLiteral("frames")].toArray();
        for (const auto& frameVal : framesArr) {
            QJsonObject frameObj = frameVal.toObject();
            int frameIdx = frameObj[QStringLiteral("index")].toInt(0);
            auto& frameRoot = seq.rootLayerForFrame(frameIdx);
            while (frameRoot.layerCount() > 0)
                frameRoot.removeLayer(0);

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
    }

    // Parse audio tracks
    QJsonArray audioArr = root[QStringLiteral("audio")].toArray();
    doc.clearAudioTracks();
    for (const auto& av : audioArr) {
        QJsonObject ao = av.toObject();
        AudioTrackData at;
        at.filepath    = ao[QStringLiteral("filepath")].toString().toStdString();
        at.displayName = ao[QStringLiteral("display_name")].toString().toStdString();
        at.zipEntry    = ao[QStringLiteral("zip_entry")].toString().toStdString();
        at.muted       = ao[QStringLiteral("muted")].toBool(false);
        at.volume      = ao[QStringLiteral("volume")].toInt(80);
        doc.addAudioTrack(at);
    }

    return true;
}

bool DocumentManager::readLayerData(mz_zip_archive* zip, Document& doc) {
    for (size_t si = 0; si < doc.sequenceCount(); ++si) {
        auto& seq = doc.sequenceAt(si);
        for (int f = 0; f < seq.totalFrames(); ++f) {
            auto* rootLayer = seq.peekRootLayerForFrame(f);
            if (!rootLayer) continue;
            for (size_t li = 0; li < rootLayer->layerCount(); ++li) {
                Layer* layer = rootLayer->layers()[li].get();
                if (!layer || layer->type() != LayerType::Raster) continue;

                auto* rl = static_cast<RasterLayer*>(layer);
                QString pngPath = buildLayerPath(f, static_cast<int>(si),
                                                 static_cast<int>(li), "png");
                QByteArray pngPathUtf8 = pngPath.toUtf8();

                size_t pngSize = 0;
                void* pngData = mz_zip_reader_extract_file_to_heap(
                    zip, pngPathUtf8.constData(), &pngSize, 0);
                if (!pngData) continue;

                QImage png;
                bool loaded = png.loadFromData(static_cast<const uchar*>(pngData),
                                               static_cast<int>(pngSize), "PNG");
                mz_free(pngData);

            if (!loaded || png.isNull()) continue;

            QImage converted = png.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            qDebug() << "LOAD layer:" << rl->name().c_str()
                     << "origin:" << rl->originX() << "," << rl->originY()
                     << "size:" << rl->width() << "x" << rl->height()
                     << "png:" << converted.width() << "x" << converted.height();

            rl->ensureContains(rl->originX(), rl->originY(),
                                   converted.width(), converted.height(), false);

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
                rl->setHasContent(true);
                rl->bufferEpochTick();
            }
        }
    }
    return true;
}

bool DocumentManager::extractAudio(mz_zip_archive* zip, Document& doc) {
    auto& tracks = doc.audioTracks();
    if (tracks.empty()) return true;

    audioTempDir_ = QDir::tempPath() + "/fap_audio_"
                  + QString::number(QCoreApplication::applicationPid());
    QDir().mkpath(audioTempDir_);

    for (auto& at : tracks) {
        if (at.zipEntry.empty()) continue;

        size_t audioSize = 0;
        QByteArray entryUtf8 = QByteArray::fromStdString(at.zipEntry);
        void* audioData = mz_zip_reader_extract_file_to_heap(
            zip, entryUtf8.constData(), &audioSize, 0);
        if (!audioData) {
            qWarning() << "Audio entry not found in ZIP:" << QString::fromStdString(at.zipEntry);
            continue;
        }

        QString fileName = QString::fromStdString(at.displayName);
        QString outPath = audioTempDir_ + "/" + fileName;
        QFile outFile(outPath);
        if (outFile.open(QIODevice::WriteOnly)) {
            outFile.write(static_cast<const char*>(audioData),
                          static_cast<qint64>(audioSize));
            outFile.close();
            at.filepath = outPath.toStdString();
        } else {
            qWarning() << "Failed to write extracted audio:" << outPath;
        }

        mz_free(audioData);
    }

    return true;
}

} // namespace fap
