// The purpose of these declarations is to keep the code for serializing and
// deserializing .tao files local to each other. This will make auditing for
// mismatches easier.

#pragma once

#include <ostream>
#include <istream>
#include <string>

#include "../Graph/RexNode.h"
#include "../Graph/RexEdge.h"
#include "../Graph/TAGraph.h"

// No single TAObjectFile class because .tao file should never be read entirely
// into memory.

// Metadata about the data stored in the file. Allows us to quickly jump to any
// section of the file.
struct TAOFileMetadata {
    unsigned int nodesSize;
    unsigned int unestablishedEdges;
    unsigned int establishedEdges;
    unsigned int nodesWithAttrs;
    unsigned int edgesWithAttrs;

    TAOFileMetadata();

    // Not an operator (to avoid copying)
    static std::ostream &write(std::ostream &out, const TAGraph &graph);

    friend std::istream &operator>>(std::istream &in, TAOFileMetadata &meta);
};

struct TAONode {
    std::string id;
    RexNode::NodeType type;

    TAONode();

    // Need this to match the interface of RexNode
    const std::string &getID() const;

    // Not an operator (to avoid copying)
    static std::ostream &write(std::ostream &out, const RexNode &node);

    friend std::istream &operator>>(std::istream &in, TAONode &node);
};

struct TAOEdge {
    RexEdge::EdgeType type;
    std::string sourceId;
    std::string destId;

    TAOEdge();
    bool operator==(const TAOEdge &other) const;
    bool operator<(const TAOEdge &other) const;

    // Need these to match the interface of RexEdge
    RexEdge::EdgeType getType() const;
    const std::string &getSourceID() const;
    const std::string &getDestinationID() const;

    // Not an operator (to avoid copying)
    static std::ostream &write(std::ostream &out, const RexEdge &edge);

    friend std::istream &operator>>(std::istream &in, TAOEdge &edge);
};

struct TAOAttrs {
    virtual ~TAOAttrs() = default;
    std::map<std::string, std::string> singleAttrs;
    std::map<std::string, std::vector<std::string>> multiAttrs;

    bool empty() const;
};

struct TAONodeAttrs: public TAOAttrs {
    std::string id;
    RexNode::NodeType type;

    TAONodeAttrs();

    // Need this to match the interface of RexNode
    const std::string &getID() const;

    bool canMerge(const TAONodeAttrs &other) const;
    void merge(const TAONodeAttrs &other);
    bool operator<(const TAONodeAttrs &other) const;
    bool empty() const;

    // Not an operator (to avoid copying)
    static std::ostream &write(std::ostream &out, const RexNode &node);

    friend std::istream &operator>>(std::istream &in, TAONodeAttrs &attrs);
};

struct TAOEdgeAttrs: public TAOAttrs {
    TAOEdge edge;

    TAOEdgeAttrs();

    bool canMerge(const TAOEdgeAttrs &other) const;
    void merge(const TAOEdgeAttrs &other);
    bool operator<(const TAOEdgeAttrs &other) const;
    bool empty() const;

    // Not an operator (to avoid copying)
    static std::ostream &write(std::ostream &out, const RexEdge &edge);

    friend std::istream &operator>>(std::istream &in, TAOEdgeAttrs &attrs);
};
