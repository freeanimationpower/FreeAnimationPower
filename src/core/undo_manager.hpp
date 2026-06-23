#pragma once

#include <vector>
#include <memory>
#include <cstddef>

namespace fap {

class UndoCommand {
public:
    virtual ~UndoCommand() = default;
    virtual void undo() = 0;
    virtual void redo() = 0;
};

class UndoManager {
public:
    static constexpr size_t kMaxEntries = 128;

    void pushCommand(std::unique_ptr<UndoCommand> cmd);
    void undo();
    void redo();
    void clear();

    bool canUndo() const;
    bool canRedo() const;

private:
    std::vector<std::unique_ptr<UndoCommand>> undoStack_;
    std::vector<std::unique_ptr<UndoCommand>> redoStack_;
};

} // namespace fap
