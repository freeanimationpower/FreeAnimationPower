#include "core/document.hpp"
#include "core/types.hpp"

#include <QDir>
#include <QFileInfo>
#include <QString>

namespace fap {

extern bool loadFAP(const QString& path, Document& doc);
extern bool saveFAP(const QString& path, const Document& doc);

enum class DocumentFormat {
    Unknown,
    FAP,
    FA2D
};

static DocumentFormat detectFormat(const QString& path) {
    QFileInfo fi(path);

    if (fi.isDir()) {
        if (QFileInfo::exists(fi.filePath() + "/document.json")) {
            return DocumentFormat::FAP;
        }
    }

    if (fi.isFile()) {
        QString suffix = fi.suffix().toLower();
        if (suffix == "fa2d") {
            return DocumentFormat::FA2D;
        }
        if (suffix == "fap") {
            if (QFileInfo::exists(path)) {
                return DocumentFormat::FAP;
            }
        }
    }

    return DocumentFormat::Unknown;
}

bool loadDocument(Document& doc, const QString& path) {
    DocumentFormat fmt = detectFormat(path);

    if (fmt == DocumentFormat::FAP) {
        bool ok = loadFAP(path, doc);
        if (ok) {
            doc.setModified(false);
            return true;
        }
        qWarning("loadDocument: failed to load FAP from: %s", qPrintable(path));
        return false;
    }

    if (fmt == DocumentFormat::FA2D) {
        qWarning("loadDocument: .fa2d format detected. ZIP-based format support requires "
                 "DocumentManager. Currently unsupported in this build.");
        return false;
    }

    qWarning("loadDocument: unrecognized format: %s", qPrintable(path));
    return false;
}

bool saveDocument(const Document& doc, const QString& path) {
    QFileInfo fi(path);
    QString suffix = fi.suffix().toLower();

    QString targetPath = path;
    if (suffix.isEmpty() || (suffix != "fap" && suffix != "fa2d")) {
        targetPath = fi.filePath() + ".fap";
        suffix = "fap";
    }

    if (suffix == "fa2d") {
        qWarning("saveDocument: .fa2d format requested. ZIP-based format support requires "
                 "DocumentManager. Saving as .fap instead.");
        targetPath = fi.filePath();
        if (fi.suffix().toLower() == "fa2d") {
            targetPath = fi.filePath().left(fi.filePath().length() - 5) + ".fap";
        }
    }

    QDir dir(targetPath);
    if (dir.exists()) {
        QDir framesDir(dir.filePath("frames"));
        if (framesDir.exists()) {
            framesDir.removeRecursively();
        }
        QFile jsonFile(dir.filePath("document.json"));
        if (jsonFile.exists()) {
            jsonFile.remove();
        }
    }

    bool ok = saveFAP(targetPath, doc);
    if (!ok) {
        qWarning("saveDocument: failed to save to: %s", qPrintable(targetPath));
        return false;
    }

    return true;
}

} // namespace fap
