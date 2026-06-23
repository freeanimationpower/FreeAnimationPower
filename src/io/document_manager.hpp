#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <memory>
#include <vector>

extern "C" {
#include <miniz.h>
}

namespace fap {

class Document;
class Layer;
class RasterLayer;

class DocumentManager {
public:
    static constexpr int kFormatVersion = 1;
    static constexpr const char* kExtension = ".fa2d";

    explicit DocumentManager();
    ~DocumentManager();

    DocumentManager(const DocumentManager&) = delete;
    DocumentManager& operator=(const DocumentManager&) = delete;

    bool save(const Document& doc, const QString& path);
    bool load(Document& doc, const QString& path);

    QString lastError() const;
    bool isBusy() const;

private:
    bool writeManifest(mz_zip_archive* zip, const Document& doc);
    bool writeTimeline(mz_zip_archive* zip, const Document& doc);
    bool writeLayerData(mz_zip_archive* zip, const Document& doc);

    bool readManifest(mz_zip_archive* zip, Document& doc);
    bool readTimeline(mz_zip_archive* zip, Document& doc);
    bool readLayerData(mz_zip_archive* zip, Document& doc);

    bool prepareAtomicTarget(const QString& finalPath, QString& tmpPath);
    bool commitAtomic(const QString& tmpPath, const QString& finalPath);

    static QJsonObject layerMetadataToJson(const Layer& layer);
    static std::unique_ptr<Layer> layerMetadataFromJson(const QJsonObject& obj);
    static QJsonObject documentToJson(const Document& doc);
    static bool documentFromJson(const QJsonObject& root, Document& doc);

    static QString buildLayerPath(int frameIdx, int layerModelIdx, const char* ext);
    static constexpr const char* kManifestEntry = "manifest.json";
    static constexpr const char* kTimelineEntry  = "timeline.json";

    mutable QMutex mutex_;
    QString lastError_;
    bool busy_ = false;
};

} // namespace fap
