#include "document.hpp"

namespace fap {

Document::Document() {
    sequences_.push_back(std::make_unique<Sequence>("Sequence 1", width_, height_));
}

Document::Document(const std::string& filepath)
    : filepath_(filepath) {
    sequences_.push_back(std::make_unique<Sequence>("Sequence 1", width_, height_));
}

void Document::setCanvasSize(int w, int h) {
    width_ = w;
    height_ = h;
    modified_ = true;
}

void Document::setActiveSequence(size_t index) {
    if (index < sequences_.size() && index != active_sequence_) {
        active_sequence_ = index;
        modified_ = true;
    }
}

Sequence& Document::addSequence(const std::string& name) {
    auto seqName = name.empty()
        ? "Sequence " + std::to_string(sequences_.size() + 1)
        : name;
    sequences_.push_back(std::make_unique<Sequence>(seqName, width_, height_));
    modified_ = true;
    return *sequences_.back();
}

void Document::removeSequence(size_t index) {
    if (sequences_.size() <= 1) return;
    if (index >= sequences_.size()) return;

    sequences_.erase(sequences_.begin() + static_cast<ptrdiff_t>(index));
    if (active_sequence_ >= sequences_.size()) {
        active_sequence_ = sequences_.size() - 1;
    }
    modified_ = true;
}

void Document::duplicateSequence(size_t index) {
    if (index >= sequences_.size()) return;

    auto copy = sequences_[index]->clone();
    sequences_.insert(sequences_.begin() + static_cast<ptrdiff_t>(index) + 1, std::move(copy));
    modified_ = true;
}

void Document::renameSequence(size_t index, const std::string& name) {
    if (index < sequences_.size()) {
        sequences_[index]->setName(name);
        modified_ = true;
    }
}

} // namespace fap
