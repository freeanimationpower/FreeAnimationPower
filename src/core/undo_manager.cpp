#include "core/undo_manager.hpp"
#include "core/diagnostic/tracer_macros.hpp"

namespace fap {

void UndoManager::pushCommand(std::unique_ptr<UndoCommand> cmd) {
    if (!cmd) return;
    cmd->redo();
    undoStack_.push_back(std::move(cmd));
    if (undoStack_.size() > kMaxEntries) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
    FAP_TRACE_UNDO("push", undoStack_.size(), redoStack_.size());
}

void UndoManager::pushApplied(std::unique_ptr<UndoCommand> cmd) {
    if (!cmd) return;
    undoStack_.push_back(std::move(cmd));
    if (undoStack_.size() > kMaxEntries) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
    FAP_TRACE_UNDO("push_applied", undoStack_.size(), redoStack_.size());
}

void UndoManager::undo() {
    if (undoStack_.empty()) return;
    auto cmd = std::move(undoStack_.back());
    undoStack_.pop_back();
    cmd->undo();
    redoStack_.push_back(std::move(cmd));
    FAP_TRACE_UNDO("undo", undoStack_.size(), redoStack_.size());
}

void UndoManager::redo() {
    if (redoStack_.empty()) return;
    auto cmd = std::move(redoStack_.back());
    redoStack_.pop_back();
    cmd->redo();
    undoStack_.push_back(std::move(cmd));
    FAP_TRACE_UNDO("redo", undoStack_.size(), redoStack_.size());
}

void UndoManager::clear() {
    undoStack_.clear();
    redoStack_.clear();
}

bool UndoManager::canUndo() const {
    return !undoStack_.empty();
}

bool UndoManager::canRedo() const {
    return !redoStack_.empty();
}

} // namespace fap
