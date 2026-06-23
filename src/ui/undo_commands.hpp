#pragma once

#include <QUndoCommand>
#include <QImage>
#include <QRect>
#include <QPointF>
#include <memory>
#include <vector>
#include <string>

#include "core/layer.hpp"
#include "canvas_widget.hpp"

namespace fap {

class CanvasCommand : public QUndoCommand {
public:
    CanvasCommand(CanvasWidget* canvas, int frame, int layerIdx,
                  const QString& text);
    ~CanvasCommand() override = default;

protected:
    CanvasWidget* canvas_;
    int frame_;
    int layerModelIdx_;

    GroupLayer& root() const;
    Layer* activeLayer() const;
    void repaint() const;
};

class PaintCommand : public CanvasCommand {
public:
    PaintCommand(CanvasWidget* canvas, int frame, int layerIdx,
                 LayerUid layerUid,
                 QImage beforePixels, QImage afterPixels, QRect dirtyRect,
                 std::vector<RawStroke> beforeStrokes,
                 std::vector<RawStroke> afterStrokes,
                 const QString& text = QStringLiteral("Paint"));

    void undo() override;
    void redo() override;
    int id() const override { return 1; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    LayerUid layerUid_;
    QImage beforePixels_;
    QImage afterPixels_;
    QRect dirtyRect_;
    std::vector<RawStroke> beforeStrokes_;
    std::vector<RawStroke> afterStrokes_;
};

class MoveCommand : public CanvasCommand {
public:
    MoveCommand(CanvasWidget* canvas, int frame, int layerIdx,
                LayerUid layerUid, QRectF srcRect, QPointF delta,
                QImage contentImage, bool isSelection,
                const QString& text = QStringLiteral("Move"));

    void undo() override;
    void redo() override;
    int id() const override { return 2; }

private:
    LayerUid layerUid_;
    QRectF srcRect_;
    QPointF delta_;
    QImage contentImage_;
    bool isSelection_;
};

class DeleteLayerCommand : public CanvasCommand {
public:
    DeleteLayerCommand(CanvasWidget* canvas, int frame, int modelIdx,
                       std::unique_ptr<Layer> layer,
                       const QString& text = QStringLiteral("Delete Layer"));

    void undo() override;
    void redo() override;
    int id() const override { return 3; }

private:
    std::unique_ptr<Layer> storedLayer_;
};

class RenameLayerCommand : public CanvasCommand {
public:
    RenameLayerCommand(CanvasWidget* canvas, int frame, int layerIdx,
                       LayerUid layerUid,
                       std::string oldName, std::string newName,
                       const QString& text = QStringLiteral("Rename Layer"));

    void undo() override;
    void redo() override;
    int id() const override { return 4; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    LayerUid layerUid_;
    std::string oldName_;
    std::string newName_;
};

class ToggleVisibilityCommand : public CanvasCommand {
public:
    ToggleVisibilityCommand(CanvasWidget* canvas, int frame, int layerIdx,
                            LayerUid layerUid,
                            const QString& text = QStringLiteral("Toggle Visibility"));

    void undo() override;
    void redo() override;
    int id() const override { return 5; }

private:
    LayerUid layerUid_;
};

class SetBlendModeCommand : public CanvasCommand {
public:
    SetBlendModeCommand(CanvasWidget* canvas, int frame, int layerIdx,
                        LayerUid layerUid,
                        BlendMode oldMode, BlendMode newMode,
                        const QString& text = QStringLiteral("Change Blend Mode"));

    void undo() override;
    void redo() override;
    int id() const override { return 6; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    LayerUid layerUid_;
    BlendMode oldMode_;
    BlendMode newMode_;
};

} // namespace fap
