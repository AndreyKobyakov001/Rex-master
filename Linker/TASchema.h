#pragma once

#include <vector> // vector
#include <ostream> // ostream

#include "../Graph/RexNode.h"

// Type for representing the inheritance hierarchy of node types
class NodeScheme {
    RexNode::NodeType type;
    std::vector<NodeScheme> subnodes;

public:
    explicit NodeScheme(RexNode::NodeType type, std::vector<NodeScheme> subnodes);
    explicit NodeScheme(RexNode::NodeType type);

    static const NodeScheme &full();

    friend std::ostream &operator<<(std::ostream &out, const NodeScheme &scheme);
};

void writeEdgeScheme(std::ostream &out);
