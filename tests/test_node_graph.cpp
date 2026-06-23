#include <gtest/gtest.h>
#include "engine/compositor/node_graph.hpp"

using namespace fap;
using namespace fap::comp;

TEST(NodeGraphTest, CreateNodes) {
    NodeGraph graph;

    auto* input = graph.addNode(std::make_unique<InputNode>("Layer1"));
    auto* blend = graph.addNode(std::make_unique<BlendNode>());
    auto* output = graph.addNode(std::make_unique<OutputNode>());

    ASSERT_NE(input, nullptr);
    ASSERT_NE(blend, nullptr);
    ASSERT_NE(output, nullptr);

    EXPECT_EQ(input->type(), NodeType::Input);
    EXPECT_EQ(blend->type(), NodeType::Blend);
    EXPECT_EQ(output->type(), NodeType::Output);
}

TEST(NodeGraphTest, NodeById) {
    NodeGraph graph;
    auto* node = graph.addNode(std::make_unique<InputNode>("Test"));
    NodeId id = node->id();

    EXPECT_EQ(graph.nodeById(id), node);
    EXPECT_EQ(graph.nodeById(99999), nullptr);
}

TEST(NodeGraphTest, ConnectNodes) {
    NodeGraph graph;
    auto* a = graph.addNode(std::make_unique<InputNode>("A"));
    auto* b = graph.addNode(std::make_unique<BlendNode>());

    graph.connect(a->id(), 0, b->id(), 0);
    EXPECT_EQ(b->inputNode(0), a);
}

TEST(NodeGraphTest, DisconnectNodes) {
    NodeGraph graph;
    auto* a = graph.addNode(std::make_unique<InputNode>("A"));
    auto* b = graph.addNode(std::make_unique<BlendNode>());

    graph.connect(a->id(), 0, b->id(), 0);
    graph.disconnect(b->id(), 0);
    EXPECT_EQ(b->inputNode(0), nullptr);
}

TEST(NodeGraphTest, RemoveNode) {
    NodeGraph graph;
    auto* node = graph.addNode(std::make_unique<InputNode>("ToRemove"));
    NodeId id = node->id();

    graph.removeNode(id);
    EXPECT_EQ(graph.nodeById(id), nullptr);
}

TEST(NodeGraphTest, NodeEnabled) {
    auto node = std::make_unique<InputNode>("Test");
    EXPECT_TRUE(node->enabled());
    node->setEnabled(false);
    EXPECT_FALSE(node->enabled());
}

TEST(NodeGraphTest, NodeOpacity) {
    auto node = std::make_unique<BlendNode>();
    EXPECT_FLOAT_EQ(node->opacity(), 1.0f);
    node->setOpacity(0.5f);
    EXPECT_FLOAT_EQ(node->opacity(), 0.5f);
}

TEST(NodeGraphTest, BlendModeSet) {
    auto node = std::make_unique<BlendNode>();
    node->setBlendMode(BlendMode::Multiply);
    EXPECT_EQ(node->blendMode(), BlendMode::Multiply);
}

TEST(NodeGraphTest, TransformNodeDefaults) {
    auto node = std::make_unique<TransformNode>();
    EXPECT_FLOAT_EQ(node->position().x, 0.0f);
    EXPECT_FLOAT_EQ(node->position().y, 0.0f);
    EXPECT_FLOAT_EQ(node->scale().x, 1.0f);
    EXPECT_FLOAT_EQ(node->scale().y, 1.0f);
    EXPECT_FLOAT_EQ(node->rotation(), 0.0f);
}

TEST(NodeGraphTest, FilterNodeTypes) {
    auto blur = std::make_unique<FilterNode>(FilterNode::FilterType::Blur);
    EXPECT_EQ(blur->filterType(), FilterNode::FilterType::Blur);

    auto invert = std::make_unique<FilterNode>(FilterNode::FilterType::Invert);
    EXPECT_EQ(invert->filterType(), FilterNode::FilterType::Invert);

    blur->setIntensity(0.8f);
    EXPECT_FLOAT_EQ(blur->intensity(), 0.8f);
}

TEST(NodeGraphTest, InputNodeNoInputs) {
    auto node = std::make_unique<InputNode>();
    EXPECT_EQ(node->requiredInputs(), 0);
    EXPECT_EQ(node->maxInputs(), 0);
}

TEST(NodeGraphTest, BlendNodeRequiresTwo) {
    auto node = std::make_unique<BlendNode>();
    EXPECT_EQ(node->requiredInputs(), 2);
    EXPECT_EQ(node->maxInputs(), 2);
}

TEST(NodeGraphTest, GroupNodeMaxInputs) {
    auto node = std::make_unique<GroupNode>();
    EXPECT_EQ(node->maxInputs(), 8);
}

TEST(NodeGraphTest, CloneNodes) {
    auto original = std::make_unique<TransformNode>("Orig");
    original->setPosition({ 10, 20 });
    original->setScale({ 2, 2 });

    auto cloned = original->clone();
    auto* clonePtr = static_cast<TransformNode*>(cloned.get());
    EXPECT_FLOAT_EQ(clonePtr->position().x, 10.0f);
    EXPECT_FLOAT_EQ(clonePtr->scale().x, 2.0f);
}

TEST(NodeGraphTest, GraphNodeCount) {
    NodeGraph graph;
    graph.addNode(std::make_unique<InputNode>());
    graph.addNode(std::make_unique<BlendNode>());
    graph.addNode(std::make_unique<OutputNode>());

    EXPECT_EQ(graph.nodes().size(), 3);
}
