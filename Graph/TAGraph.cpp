/////////////////////////////////////////////////////////////////////////////////////////////////////////
// TAGraph.cpp
//
// Created By: Bryan J Muscedere
// Date: 10/04/17.
//
// Master system for maintaining the in-memory
// digraph system. This is important to store
// C++ entities and then output to TA.
//
// Copyright (C) 2017, Bryan J. Muscedere
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
/////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "TAGraph.h"
#include <iostream>
using namespace std;

using ConstRexNodeIterator = TAGraph::ConstRexNodeIterator;
using ConstRexNodes = TAGraph::ConstRexNodes;
using ConstRexEdgeIterator = TAGraph::ConstRexEdgeIterator;
using ConstRexEdges = TAGraph::ConstRexEdges;

/**** Node Iterator ****/

ConstRexNodeIterator::ConstRexNodeIterator(ConstRexNodeIterator::inner_iterator it): it{it} {}

const RexNode &ConstRexNodeIterator::operator*() const {
    return *it->second;
}

const RexNode *ConstRexNodeIterator::operator->() const {
    return it->second;
}

bool ConstRexNodeIterator::operator!=(const ConstRexNodeIterator &other) const {
    return it != other.it;
}

// Prefix increment
ConstRexNodeIterator &ConstRexNodeIterator::operator++() {
    ++it;
    return *this;
}

// Postfix increment
ConstRexNodeIterator ConstRexNodeIterator::operator++(int) {
    ConstRexNodeIterator tmp(*this);
    ++*this;
    return tmp;
}

ConstRexNodes::ConstRexNodes(const TAGraph &graph): graph{graph} {}

ConstRexNodeIterator ConstRexNodes::begin() const {
    return ConstRexNodeIterator(graph.idList.cbegin());
}

ConstRexNodeIterator ConstRexNodes::end() const {
    return ConstRexNodeIterator(graph.idList.cend());
}

/**** Edge Iterator ****/

ConstRexEdgeIterator::ConstRexEdgeIterator(
    const edge_map &edges,
    inner_iterator it
): edges{edges}, it{it} {
    if (it != edges.cend()) {
        elem_it = it->second.cbegin();
    }
}

const RexEdge &ConstRexEdgeIterator::operator*() const {
    return **elem_it;
}

const RexEdge *ConstRexEdgeIterator::operator->() const {
    return *elem_it;
}

bool ConstRexEdgeIterator::operator!=(const ConstRexEdgeIterator &other) const {
    // Need to make sure that iterators for different `edge_map` instances are
    // not compared as equal. Taking the address of `edges` because we don't
    // want to do a deep comparison every time.
    if (&edges != &other.edges) {
        return true;
    }

    // Once either side is at the end, comparing `elem_it` no longer has any
    // meaning because there is no element that could possibly be there.
    if (it == edges.cend() || other.it == other.edges.cend()) {
        return it != other.it;
    }

    // Still have some elements left to go, so we can compare them
    return (it != other.it) || (elem_it != other.elem_it);
}

// Prefix increment
ConstRexEdgeIterator &ConstRexEdgeIterator::operator++() {
    ++elem_it;

    // If we're past the end of `elem_it`, see if we can still advance `it`.
    // Need to do this in a while loop because every element in `it` doesn't
    // necessarily have something to contribute to `elem_it`
    while (it != edges.cend() && elem_it == it->second.cend()) {
        ++it;
        // Need to do a second check here because even if `it` wasn't at the end
        // before, it might be there now
        if (it != edges.cend()) {
            elem_it = it->second.cbegin();
        }
    }

    // If the above code found something, `elem_it` will have a new element.
    // Otherwise, ¯\_(ツ)_/¯
    return *this;
}

// Postfix increment
ConstRexEdgeIterator ConstRexEdgeIterator::operator++(int) {
    ConstRexEdgeIterator tmp(*this);
    ++*this;
    return tmp;
}

ConstRexEdges::ConstRexEdges(const TAGraph &graph): graph{graph} {}

ConstRexEdgeIterator ConstRexEdges::begin() const {
    return ConstRexEdgeIterator(graph.edgeSrcList, graph.edgeSrcList.cbegin());
}

ConstRexEdgeIterator ConstRexEdges::end() const {
    return ConstRexEdgeIterator(graph.edgeSrcList, graph.edgeSrcList.cend());
}

/**** TAGraph ****/

TAGraph::TAGraph() {}

/**
 * Destructor. Deletes all nodes and edges.
 */
TAGraph::~TAGraph() {
    for (auto &entry : idList) {
        delete entry.second;
    }
    for (auto &entry : edgeSrcList) {
        for (auto &vecEntry : entry.second) {
            delete vecEntry;
        }
    }
}

/**
 * Adds a new node.
 * @param node The node to add.
 */
void TAGraph::addNode(RexNode *node) {
    // Avoid adding the same node multiple times
    if (!hasNode(node->getID())) {
        idList[node->getID()] = node;
    }
}

/**
 * Adds a new edge.
 * @param edge  The edge to add.
 */
void TAGraph::addEdge(RexEdge *edge) {
    if (hasEdge(edge->getSourceID(), edge->getDestinationID(), edge->getType()))
        return;
    edgeSrcList[edge->getSourceID()].push_back(edge);
    edgeDstList[edge->getDestinationID()].push_back(edge);
}

/**
 * Finds a node by ID. Returns nullptr if the node is not found.
 *
 * @param nodeID The ID to check.
 * @return The pointer to the node (or nullptr)
 */
RexNode *TAGraph::findNode(const std::string &nodeID) {
    try {
        return idList.at(nodeID);
    } catch (out_of_range &) {
        return nullptr;
    }
}

/**
 * Find edge bashed on its IDs. Returns nullptr if the edge is not found.
 *
 * @param srcID The source node ID.
 * @param dstID The destination node ID.
 * @param type The node type.
 * @return The edge that was found.
 */
RexEdge *TAGraph::findEdge(
    const std::string &srcID,
    const std::string &dstID,
    RexEdge::EdgeType type
) {
    // Gets the edges for a source.
    vector<RexEdge *> &edges = edgeSrcList[srcID];
    for (auto &edge : edges) {
        if (edge->getSourceID() == srcID &&
            edge->getDestinationID() == dstID &&
            edge->getType() == type) {
            return edge;
        }
    }

    return nullptr;
}

/**
 * Finds edge by source ID.
 * @param srcID The source ID.
 * @return A vector of matched nodes.
 */
const std::vector<RexEdge *> &TAGraph::findEdgesBySrcID(const std::string &srcID) {
    try {
        return edgeSrcList.at(srcID);
    } catch (out_of_range &) {
        return emptyVec;
    }    
}

/**
 * Finds edge by destination ID.
 * @param srcID The destination ID.
 * @return A vector of matched nodes.
 */
const std::vector<RexEdge *> &TAGraph::findEdgesByDstID(const std::string &dstID) {
    try {
        return edgeDstList.at(dstID);
    } catch (out_of_range &) {
        return emptyVec;
    }  
}

/**
 * Checks whether a node exists.
 * @param nodeID The ID of the node.
 * @return Whether the node exists.
 */
bool TAGraph::hasNode(const std::string &nodeID) {
    return findNode(nodeID) != nullptr;
}

/**
 * Checks whether an edge exists.
 * @param srcID The source ID of the node.
 * @param dstID The destination ID of the node.
 * @type type The edge type.
 * @return Whether the edge exists.
 */
bool TAGraph::hasEdge(const std::string &srcID, const std::string &dstID, RexEdge::EdgeType type) {
    return findEdge(srcID, dstID, type) != nullptr;
}

// Returns the total number of nodes that are marked to be kept in the object file
unsigned int TAGraph::keptNodes() const {
    unsigned int count = 0;
    for (const RexNode &node : nodes()) {
        if (node.keep()) {
            count += 1;
        }
    }
    return count;
}

// Counts the number of unestablished edges
unsigned int TAGraph::unestablishedEdgesSize() const {
    unsigned int count = 0;
    for (const RexEdge &edge : edges()) {
        if (!edge.isEstablished()) {
            count += 1;
        }
    }
    return count;
}

// Counts the number of established edges
unsigned int TAGraph::establishedEdgesSize() const {
    unsigned int count = 0;
    for (const RexEdge &edge : edges()) {
        if (edge.isEstablished()) {
            count += 1;
        }
    }
    return count;
}

// Counts the number of nodes with some number of attributes
unsigned int TAGraph::nodesWithAttrs() const {
    unsigned int count = 0;
    for (const RexNode &node : nodes()) {
        if (node.getNumAttributes() > 0) {
            count += 1;
        }
    }
    return count;
}

// Counts the number of edges with some number of attributes
unsigned int TAGraph::edgesWithAttrs() const {
    unsigned int count = 0;
    for (const RexEdge &edge : edges()) {
        if (edge.getNumAttributes() > 0) {
            count += 1;
        }
    }
    return count;
}

// Returns a constant iterator over the nodes in this graph. This is useful for
// lazily iterating over just the nodes.
ConstRexNodes TAGraph::nodes() const {
    return ConstRexNodes(*this);
}

// Returns a constant iterator over the edges in this graph. This is useful for
// lazily iterating over just the edges.
ConstRexEdges TAGraph::edges() const {
    return ConstRexEdges(*this);
}
