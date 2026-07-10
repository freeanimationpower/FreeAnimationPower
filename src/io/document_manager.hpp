#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>

extern "C" {
#include <miniz.h>
}

namespace fap {

class Document;
class Layer;
class RasterLayer;
struct AudioTrackData;

struct ViewState {
    float zoom = 0.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
};

class DocumentManager {
public:
    static constexpr int kFormatVersion = 2;
    static constexpr const char* kExtension = ".fap";

    explicit DocumentManager();
    ~DocumentManager();

    DocumentManager(const DocumentManager&) = delete;
    DocumentManager& operator=(const DocumentManager&) = delete;

    bool save(const Document& doc, const QString& path, const ViewState& vs = {});
    bool load(Document& doc, const QString& path);

    ViewState viewState() const { return viewState_; }
    QString lastError() const;
    bool isBusy() const;

    // Extracted audio temp dir (valid after load())
    QString audioTempDir() const { return audioTempDir_; }

private: 
    bool writeManifest(mz_zip_archive* zip, const Document& doc);
    bool writeTimeline(mz_zip_archive* zip, const Document& doc);
    bool writeLayerData(mz_zip_archive* zip, const Document& doc);

    bool readManifest(mz_zip_archive* zip, Document& doc);
    bool readTimeline(mz_zip_archive* zip, Document& doc);
    bool readLayerData(mz_zip_archive* zip, Document& doc);
    bool extractAudio(mz_zip_archive* zip, Document& doc);

    bool prepareAtomicTarget(const QString& finalPath, QString& tmpPath);
    bool commitAtomic(const QString& tmpPath, const QString& finalPath);

    static QJsonObject layerMetadataToJson(const Layer& layer);
    static std::unique_ptr<Layer> layerMetadataFromJson(const QJsonObject& obj);
    QJsonObject documentToJson(const Document& doc);
    bool documentFromJson(const QJsonObject& root, Document& doc);

    static QString buildLayerPath(int frameIdx, int seqIdx, int layerIdx, const char* ext);
    static constexpr const char* kManifestEntry = "manifest.json";
    static constexpr const char* kTimelineEntry  = "timeline.json";

    mutable QMutex mutex_;
    QString lastError_;
    bool busy_ = false;
    ViewState viewState_;
    QString audioTempDir_;

    // Temporary map for shared pixel resolution during load
    std::unordered_map<uint64_t, uint64_t> sharePixelMap_;
};

} // namespace fap
