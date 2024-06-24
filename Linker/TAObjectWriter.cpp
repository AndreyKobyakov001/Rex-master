#include "TAObjectWriter.h"

#include "TAObjectFile.h"

#include <algorithm> // for std::sort

using namespace std;

TAObjectWriter::TAObjectWriter(const TAGraph &graph): graph{graph} {}

ostream &operator<<(ostream &out, const TAObjectWriter &writer) {
    const TAGraph &graph = writer.graph;

    // Much of this code favors making multiple passes over the nodes and edges
    // instead of storing intermediate results so that we don't have to allocate

    // Write out the sizes so we can quickly jump to any point in the file
    TAOFileMetadata::write(out, graph) << endl;

    // Nodes first so we can load them into the symbol table
    for (const RexNode &node : graph.nodes()) {
        // Only keep nodes that were explicitly marked as nodes we should keep
        // Some nodes may not be kept because they are external to the file
        // being analysed (e.g. system headers, other source files that will be
        // walked later, etc.)
        if (node.keep()) {
            TAONode::write(out, node) << endl;
        }
    }

    // Unestablished edges next so we can establish them as we go
    for (const RexEdge &edge : graph.edges()) {
        if (!edge.isEstablished()) {
            TAOEdge::write(out, edge) << endl;
        }
    }

    // Established edges can be copied verbatim without any further lookups
    for (const RexEdge &edge : graph.edges()) {
        if (edge.isEstablished()) {
            TAOEdge::write(out, edge) << endl;
        }
    }

    // Attributes for both nodes and edges are written in sorted order

    // Write out attributes for all nodes (even empty attributes)
    vector<const RexNode *> nodes;
    for (const RexNode &node : graph.nodes()) {
        if(node.keep())
        {
            nodes.push_back(&node);
        }
    }
    std::sort(nodes.begin(), nodes.end(), [](const RexNode *left, const RexNode *right) {
        return RexNode::compare(*left, *right);
    });
    for (const RexNode *node : nodes) {
        TAONodeAttrs::write(out, *node) << endl;
    }

    // Edge attributes will be filtered during linking to only include edges
    // that are established. We don't know which edges will be filtered at this
    // point, so we have no choice but to write the attributes for all of them.
    vector<const RexEdge *> edges;
    for (const RexEdge &edge : graph.edges()) {
        edges.push_back(&edge);
    }
    std::sort(edges.begin(), edges.end(), [](const RexEdge *left, const RexEdge *right) {
        return RexEdge::compare(*left, *right);
    });
    for (const RexEdge *edge : edges) {
        TAOEdgeAttrs::write(out, *edge) << endl;
    }

    return out;
}
