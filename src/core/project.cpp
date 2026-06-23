#include "core/document.hpp"
#include <string>
#include <filesystem>

namespace fap {

// Project management - wraps Document with file path tracking and project-level operations.
// The Document class handles the animation data model. This file provides
// project-level utilities.

class Project {
public:
    Project() : doc_(std::make_unique<Document>()) {}
    explicit Project(const std::string& path)
        : filepath_(path), doc_(std::make_unique<Document>(path)) {}

    Document& document() { return *doc_; }
    const Document& document() const { return *doc_; }

    const std::string& filepath() const { return filepath_; }
    void setFilepath(const std::string& path) { filepath_ = path; doc_->setModified(true); }

    std::string projectName() const {
        if (filepath_.empty()) return "Untitled";
        std::filesystem::path p(filepath_);
        return p.stem().string();
    }

private:
    std::string filepath_;
    std::unique_ptr<Document> doc_;
};

} // namespace fap
