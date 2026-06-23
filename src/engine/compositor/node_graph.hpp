#pragma once

#include "core/types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <unordered_map>
#include <optional>

namespace fap {

class Layer;
class RasterLayer;
class GroupLayer;

namespace comp {

using NodeId = uint64_t;

NodeId generateNodeId();

enum class NodeType {
    Input,
    Output,
    Blend,
    Filter,
    Transform,
    Group,
    PassThrough
};

struct NodePort {
    NodeId nodeId = 0;
    std::string name;
    bool connected = false;
};

class CompositorNode {
public:
    explicit CompositorNode(NodeType type, const std::string& name = {});
    virtual ~CompositorNode() = default;

    virtual std::unique_ptr<CompositorNode> clone() const = 0;

    NodeId id() const { return id_; }
    NodeType type() const { return type_; }
    const std::string& name() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    bool enabled() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }

    float opacity() const { return opacity_; }
    void setOpacity(float o) { opacity_ = std::clamp(o, 0.0f, 1.0f); }

    BlendMode blendMode() const { return blend_mode_; }
    void setBlendMode(BlendMode mode) { blend_mode_ = mode; }

    void connectInput(int portIndex, CompositorNode* node);
    void disconnectInput(int portIndex);
    CompositorNode* inputNode(int portIndex) const;
    int inputCount() const { return static_cast<int>(inputs_.size()); }

    int outputCount() const { return static_cast<int>(outputs_.size()); }

    virtual int requiredInputs() const { return 1; }
    virtual int maxInputs() const { return 1; }

    virtual void process(const std::vector<const RasterLayer*>& inputs,
                         RasterLayer& output) = 0;

protected:
    NodeId id_;
    NodeType type_;
    std::string name_;
    bool enabled_ = true;
    float opacity_ = 1.0f;
    BlendMode blend_mode_ = BlendMode::Normal;
    std::vector<CompositorNode*> inputs_;
    std::vector<CompositorNode*> outputs_;

    friend class NodeGraph;
};

class InputNode : public CompositorNode {
public:
    explicit InputNode(const std::string& name = "Input");
    std::unique_ptr<CompositorNode> clone() const override;
    int requiredInputs() const override { return 0; }
    int maxInputs() const override { return 0; }

    void setLayer(const RasterLayer* layer) { source_layer_ = layer; }
    const RasterLayer* layer() const { return source_layer_; }

    void process(const std::vector<const RasterLayer*>& inputs,
                 RasterLayer& output) override;

private:
    const RasterLayer* source_layer_ = nullptr;
};

class OutputNode : public CompositorNode {
public:
    explicit OutputNode(const std::string& name = "Output");
    std::unique_ptr<CompositorNode> clone() const override;

    void process(const std::vector<const RasterLayer*>& inputs,
                 RasterLayer& output) override;
};

class BlendNode : public CompositorNode {
public:
    explicit BlendNode(const std::string& name = "Blend");
    std::unique_ptr<CompositorNode> clone() const override;
    int requiredInputs() const override { return 2; }
    int maxInputs() const override { return 2; }

    void process(const std::vector<const RasterLayer*>& inputs,
                 RasterLayer& output) override;
};

class TransformNode : public CompositorNode {
public:
    explicit TransformNode(const std::string& name = "Transform");
    std::unique_ptr<CompositorNode> clone() const override;

    Vec2 position() const { return position_; }
    void setPosition(const Vec2& pos) { position_ = pos; }
    Vec2 scale() const { return scale_; }
    void setScale(const Vec2& s) { scale_ = s; }
    float rotation() const { return rotation_; }
    void setRotation(float deg) { rotation_ = deg; }
    Vec2 pivot() const { return pivot_; }
    void setPivot(const Vec2& p) { pivot_ = p; }

    void process(const std::vector<const RasterLayer*>& inputs,
                 RasterLayer& output) override;

private:
    Vec2 position_{0.0f, 0.0f};
    Vec2 scale_{1.0f, 1.0f};
    float rotation_ = 0.0f;
    Vec2 pivot_{0.0f, 0.0f};
};

class FilterNode : public CompositorNode {
public:
    enum class FilterType {
        Blur,
        Sharpen,
        ColorCorrect,
        Threshold,
        Invert
    };

    explicit FilterNode(FilterType filterType, const std::string& name = "Filter");
    std::unique_ptr<CompositorNode> clone() const override;

    FilterType filterType() const { return filter_type_; }
    float intensity() const { return intensity_; }
    void setIntensity(float i) { intensity_ = std::clamp(i, 0.0f, 1.0f); }

    void process(const std::vector<const RasterLayer*>& inputs,
                 RasterLayer& output) override;

private:
    FilterType filter_type_;
    float intensity_ = 0.5f;
};

class GroupNode : public CompositorNode {
public:
    explicit GroupNode(const std::string& name = "Group");
    std::unique_ptr<CompositorNode> clone() const override;
    int maxInputs() const override { return 8; }

    void process(const std::vector<const RasterLayer*>& inputs,
                 RasterLayer& output) override;
};

class NodeGraph {
public:
    NodeGraph();

    CompositorNode* addNode(std::unique_ptr<CompositorNode> node);
    void removeNode(NodeId id);
    CompositorNode* nodeById(NodeId id);
    const CompositorNode* nodeById(NodeId id) const;

    void connect(NodeId fromId, int outputPort,
                 NodeId toId, int inputPort);
    void disconnect(NodeId toId, int inputPort);

    void evaluate(RasterLayer& target);

    const std::vector<std::unique_ptr<CompositorNode>>& nodes() const { return nodes_; }

private:
    std::vector<std::unique_ptr<CompositorNode>> nodes_;
    OutputNode* findOutputNode() const;
    void evaluateNode(CompositorNode* node,
                      std::unordered_map<NodeId, std::unique_ptr<RasterLayer>>& cache);

    std::unordered_map<NodeId, bool> visited_;
    std::hash<NodeId> id_hasher_;
};

} // namespace comp
} // namespace fap
