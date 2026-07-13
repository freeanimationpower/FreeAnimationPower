#include "node_graph.hpp"
#include "core/layer.hpp"
#include "engine/common/blend_utils.hpp"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace fap {
namespace comp {

static std::atomic<NodeId> s_nextNodeId{1};

NodeId generateNodeId() {
    return s_nextNodeId.fetch_add(1, std::memory_order_relaxed);
}

CompositorNode::CompositorNode(NodeType type, const std::string& name)
    : id_(generateNodeId())
    , type_(type)
    , name_(name.empty() ? "Node" : name)
{
}

void CompositorNode::connectInput(int portIndex, CompositorNode* node)
{
    if (portIndex < 0) return;
    if (static_cast<size_t>(portIndex) >= inputs_.size()) {
        inputs_.resize(portIndex + 1, nullptr);
    }
    inputs_[portIndex] = node;
    if (node) {
        node->outputs_.push_back(this);
    }
}

void CompositorNode::disconnectInput(int portIndex)
{
    if (portIndex < 0 || static_cast<size_t>(portIndex) >= inputs_.size()) return;
    auto* prev = inputs_[portIndex];
    if (prev) {
        auto it = std::find(prev->outputs_.begin(), prev->outputs_.end(), this);
        if (it != prev->outputs_.end()) {
            prev->outputs_.erase(it);
        }
    }
    inputs_[portIndex] = nullptr;
}

CompositorNode* CompositorNode::inputNode(int portIndex) const
{
    if (portIndex < 0 || static_cast<size_t>(portIndex) >= inputs_.size()) return nullptr;
    return inputs_[portIndex];
}

InputNode::InputNode(const std::string& name)
    : CompositorNode(NodeType::Input, name.empty() ? "Input" : name)
{
}

std::unique_ptr<CompositorNode> InputNode::clone() const
{
    auto n = std::make_unique<InputNode>(name_);
    n->source_layer_ = source_layer_;
    n->enabled_ = enabled_;
    n->opacity_ = opacity_;
    return n;
}

void InputNode::process(const std::vector<const RasterLayer*>&,
                        RasterLayer& output)
{
    if (!source_layer_ || !enabled_) return;
    const int w = std::min(source_layer_->width(), output.width());
    const int h = std::min(source_layer_->height(), output.height());
    const auto* src = source_layer_->pixelData();
    auto* dst = output.pixelData();
    for (int y = 0; y < h; ++y) {
        size_t row = static_cast<size_t>(y) * w;
        std::memcpy(dst + row, src + row, w * sizeof(uint32_t));
    }
}

OutputNode::OutputNode(const std::string& name)
    : CompositorNode(NodeType::Output, name.empty() ? "Output" : name)
{
}

std::unique_ptr<CompositorNode> OutputNode::clone() const
{
    auto n = std::make_unique<OutputNode>(name_);
    n->enabled_ = enabled_;
    n->opacity_ = opacity_;
    return n;
}

void OutputNode::process(const std::vector<const RasterLayer*>& inputs,
                         RasterLayer& output)
{
    if (!enabled_ || inputs.empty() || !inputs[0]) return;
    const auto* src = inputs[0];
    const int w = std::min(src->width(), output.width());
    const int h = std::min(src->height(), output.height());
    const auto* srcPx = src->pixelData();
    auto* dstPx = output.pixelData();
    for (int y = 0; y < h; ++y) {
        size_t row = static_cast<size_t>(y) * w;
        std::memcpy(dstPx + row, srcPx + row, w * sizeof(uint32_t));
    }
}

BlendNode::BlendNode(const std::string& name)
    : CompositorNode(NodeType::Blend, name.empty() ? "Blend" : name)
{
}

std::unique_ptr<CompositorNode> BlendNode::clone() const
{
    auto n = std::make_unique<BlendNode>(name_);
    n->enabled_ = enabled_;
    n->opacity_ = opacity_;
    n->blend_mode_ = blend_mode_;
    return n;
}

void BlendNode::process(const std::vector<const RasterLayer*>& inputs,
                        RasterLayer& output)
{
    if (!enabled_ || inputs.size() < 2 || !inputs[0] || !inputs[1]) return;

    const auto* bg = inputs[0];
    const auto* fg = inputs[1];
    const int w = std::min({ bg->width(), fg->width(), output.width() });
    const int h = std::min({ bg->height(), fg->height(), output.height() });

    const auto* bgPx = bg->pixelData();
    const auto* fgPx = fg->pixelData();
    auto* dstPx = output.pixelData();

    std::memcpy(dstPx, bgPx, static_cast<size_t>(w) * h * sizeof(uint32_t));

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t idx = static_cast<size_t>(y) * w + x;
            auto fp = unpackPixel(fgPx[idx]);
            float fa = fp.a * opacity_;
            if (fa <= 0.0f) continue;

            blendPixel(dstPx, w, h, x, y, fp.r, fp.g, fp.b, fa, blend_mode_);
        }
    }
}

TransformNode::TransformNode(const std::string& name)
    : CompositorNode(NodeType::Transform, name.empty() ? "Transform" : name)
{
}

std::unique_ptr<CompositorNode> TransformNode::clone() const
{
    auto n = std::make_unique<TransformNode>(name_);
    n->enabled_ = enabled_;
    n->position_ = position_;
    n->scale_ = scale_;
    n->rotation_ = rotation_;
    n->pivot_ = pivot_;
    return n;
}

void TransformNode::process(const std::vector<const RasterLayer*>& inputs,
                            RasterLayer& output)
{
    if (!enabled_ || inputs.empty() || !inputs[0]) return;

    const auto* src = inputs[0];
    const int w = output.width();
    const int h = output.height();
    const auto* srcPx = src->pixelData();
    auto* dstPx = output.pixelData();
    const int srcW = src->width();
    const int srcH = src->height();

    std::fill(dstPx, dstPx + static_cast<size_t>(w) * h, 0);

    float rad = rotation_ * 3.14159265358979323846f / 180.0f;
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);
    float px = pivot_.x, py = pivot_.y;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = (x - px) / scale_.x;
            float dy = (y - py) / scale_.y;

            float rx = dx * cosA + dy * sinA;
            float ry = -dx * sinA + dy * cosA;

            int sx = static_cast<int>(std::round(rx + px - position_.x));
            int sy = static_cast<int>(std::round(ry + py - position_.y));

            if (sx >= 0 && sx < srcW && sy >= 0 && sy < srcH) {
                dstPx[static_cast<size_t>(y) * w + x] =
                    srcPx[static_cast<size_t>(sy) * srcW + sx];
            }
        }
    }
}

FilterNode::FilterNode(FilterType ft, const std::string& name)
    : CompositorNode(NodeType::Filter, name.empty() ? "Filter" : name)
    , filter_type_(ft)
{
}

std::unique_ptr<CompositorNode> FilterNode::clone() const
{
    auto n = std::make_unique<FilterNode>(filter_type_, name_);
    n->enabled_ = enabled_;
    n->intensity_ = intensity_;
    return n;
}

void FilterNode::process(const std::vector<const RasterLayer*>& inputs,
                         RasterLayer& output)
{
    if (!enabled_ || inputs.empty() || !inputs[0]) return;

    const auto* src = inputs[0];
    const int w = std::min(src->width(), output.width());
    const int h = std::min(src->height(), output.height());
    const auto* srcPx = src->pixelData();
    auto* dstPx = output.pixelData();

    switch (filter_type_) {
    case FilterType::Blur: {
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                float sumR = 0, sumG = 0, sumB = 0, sumA = 0;
                int count = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        size_t idx = static_cast<size_t>(y + dy) * w + (x + dx);
                        auto fp = unpackPixel(srcPx[idx]);
                        sumR += fp.r; sumG += fp.g; sumB += fp.b; sumA += fp.a;
                        ++count;
                    }
                }
                size_t ci = static_cast<size_t>(y) * w + x;
                auto c = unpackPixel(srcPx[ci]);
                dstPx[ci] = packPixel({
                    lerpf(c.r, sumR / count, intensity_),
                    lerpf(c.g, sumG / count, intensity_),
                    lerpf(c.b, sumB / count, intensity_),
                    lerpf(c.a, sumA / count, intensity_)
                });
            }
        }
        break;
    }
    case FilterType::Invert: {
        for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
            auto fp = unpackPixel(srcPx[i]);
            dstPx[i] = packPixel({
                lerpf(fp.r, 1.0f - fp.r, intensity_),
                lerpf(fp.g, 1.0f - fp.g, intensity_),
                lerpf(fp.b, 1.0f - fp.b, intensity_),
                fp.a
            });
        }
        break;
    }
    default:
        std::memcpy(dstPx, srcPx, static_cast<size_t>(w) * h * sizeof(uint32_t));
        break;
    }
}

GroupNode::GroupNode(const std::string& name)
    : CompositorNode(NodeType::Group, name.empty() ? "Group" : name)
{
}

std::unique_ptr<CompositorNode> GroupNode::clone() const
{
    auto n = std::make_unique<GroupNode>(name_);
    n->enabled_ = enabled_;
    n->opacity_ = opacity_;
    n->blend_mode_ = blend_mode_;
    return n;
}

void GroupNode::process(const std::vector<const RasterLayer*>& inputs,
                        RasterLayer& output)
{
    if (!enabled_ || inputs.empty()) return;

    output.clear({ 0.0f, 0.0f, 0.0f, 0.0f });
    const int w = output.width();
    const int h = output.height();
    auto* dstPx = output.pixelData();

    for (const auto* layer : inputs) {
        if (!layer) continue;
        const int lw = std::min(layer->width(), w);
        const int lh = std::min(layer->height(), h);
        const auto* srcPx = layer->pixelData();

        for (int y = 0; y < lh; ++y) {
            for (int x = 0; x < lw; ++x) {
                auto fp = unpackPixel(srcPx[static_cast<size_t>(y) * lw + x]);
                float fa = fp.a * opacity_;
                if (fa <= 0.0f) continue;

                blendPixel(dstPx, w, h, x, y, fp.r, fp.g, fp.b, fa, blend_mode_);
            }
        }
    }
}

NodeGraph::NodeGraph() = default;

CompositorNode* NodeGraph::addNode(std::unique_ptr<CompositorNode> node)
{
    auto* ptr = node.get();
    nodes_.push_back(std::move(node));
    return ptr;
}

void NodeGraph::removeNode(NodeId id)
{
    for (auto& n : nodes_) {
        for (int i = 0; i < n->inputCount(); ++i) {
            if (n->inputNode(i) && n->inputNode(i)->id() == id) {
                n->disconnectInput(i);
            }
        }
    }
    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [id](const auto& n) { return n->id() == id; });
    if (it != nodes_.end()) {
        nodes_.erase(it);
    }
}

CompositorNode* NodeGraph::nodeById(NodeId id)
{
    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [id](const auto& n) { return n->id() == id; });
    return it != nodes_.end() ? it->get() : nullptr;
}

const CompositorNode* NodeGraph::nodeById(NodeId id) const
{
    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [id](const auto& n) { return n->id() == id; });
    return it != nodes_.end() ? it->get() : nullptr;
}

void NodeGraph::connect(NodeId fromId, int, NodeId toId, int inputPort)
{
    auto* from = nodeById(fromId);
    auto* to = nodeById(toId);
    if (from && to) {
        to->connectInput(inputPort, from);
    }
}

void NodeGraph::disconnect(NodeId toId, int inputPort)
{
    auto* to = nodeById(toId);
    if (to) {
        to->disconnectInput(inputPort);
    }
}

void NodeGraph::evaluate(RasterLayer& target)
{
    auto* output = findOutputNode();
    if (!output) return;

    std::unordered_map<NodeId, std::unique_ptr<RasterLayer>> cache;
    visited_.clear();
    evaluateNode(output, cache);

    auto it = cache.find(output->id());
    if (it == cache.end() || !it->second) return;

    const auto& result = *it->second;
    const int w = std::min(result.width(), target.width());
    const int h = std::min(result.height(), target.height());
    const auto* srcPx = result.pixelData();
    auto* dstPx = target.pixelData();
    for (int y = 0; y < h; ++y) {
        std::memcpy(dstPx + static_cast<size_t>(y) * w,
                    srcPx + static_cast<size_t>(y) * w,
                    static_cast<size_t>(w) * sizeof(uint32_t));
    }
    target.bufferEpochTick();
    target.setHasContent(true);
}

void NodeGraph::evaluateNode(CompositorNode* node,
                             std::unordered_map<NodeId, std::unique_ptr<RasterLayer>>& cache)
{
    if (!node || visited_[node->id()]) return;
    visited_[node->id()] = true;

    std::vector<const RasterLayer*> input_layers;
    for (int i = 0; i < node->inputCount(); ++i) {
        auto* inputNode = node->inputNode(i);
        if (inputNode && !visited_[inputNode->id()]) {
            evaluateNode(inputNode, cache);
        }
        if (inputNode) {
            auto it = cache.find(inputNode->id());
            if (it != cache.end() && it->second) {
                input_layers.push_back(it->second.get());
            }
        }
    }

    if (node->type() == NodeType::Input) {
        auto* in = static_cast<InputNode*>(node);
        if (in->layer()) {
            auto& layer = cache[node->id()];
            if (!layer) {
                layer = std::make_unique<RasterLayer>("cache", in->layer()->width(), in->layer()->height());
            }
            node->process({}, *layer);
        }
    } else if (!input_layers.empty()) {
        auto& layer = cache[node->id()];
        if (!layer && input_layers[0]) {
            layer = std::make_unique<RasterLayer>("cache", input_layers[0]->width(), input_layers[0]->height());
        }
        if (layer) {
            node->process(input_layers, *layer);
        }
    }
}

OutputNode* NodeGraph::findOutputNode() const
{
    for (const auto& n : nodes_) {
        if (n->type() == NodeType::Output && n->enabled()) {
            return static_cast<OutputNode*>(n.get());
        }
    }
    return nullptr;
}

} // namespace comp
} // namespace fap
