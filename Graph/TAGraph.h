/////////////////////////////////////////////////////////////////////////////////////////////////////////
// TAGraph.h
//
// Created By: Bryan J Muscedere
// Date: 10/04/17.
//
// Modifed By: Davood Anbarnam
// Date: 22/12/18.
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

#ifndef REX_TAGRAPH_H
#define REX_TAGRAPH_H

#include "RexEdge.h"
#include "RexNode.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class TAGraph {
public:
    class ConstRexNodes; // Need to forward declare
    class ConstRexNodeIterator {
        typedef std::unordered_map<std::string, RexNode *>::const_iterator inner_iterator;
        inner_iterator it;

        explicit ConstRexNodeIterator(inner_iterator it);
    public:
        const RexNode &operator*() const;
        const RexNode *operator->() const;
        bool operator!=(const ConstRexNodeIterator &other) const;

        ConstRexNodeIterator &operator++(); // prefix
        ConstRexNodeIterator operator++(int); // postfix

        friend ConstRexNodes; // Ability to access the private constructor
    };

    // For use in range-for loops
    class ConstRexNodes {
        const TAGraph &graph;

        ConstRexNodes(const TAGraph &graph);
    public:
        ConstRexNodeIterator begin() const;
        ConstRexNodeIterator end() const;

        friend TAGraph; // Ability to access the private constructor
    };

    class ConstRexEdges; // Need to forward declare
    class ConstRexEdgeIterator {
        // This iterator needs to iterate both through the edges map and the
        // vector of edges within it

        typedef std::unordered_map<std::string, std::vector<RexEdge *>> edge_map;
        typedef edge_map::const_iterator inner_iterator;
        typedef std::vector<RexEdge *> edge_list;
        typedef edge_list::const_iterator element_iterator;

        const edge_map &edges;
        inner_iterator it;
        element_iterator elem_it;

        ConstRexEdgeIterator(const edge_map &edges, inner_iterator it);
    public:
        const RexEdge &operator*() const;
        const RexEdge *operator->() const;
        bool operator!=(const ConstRexEdgeIterator &other) const;

        ConstRexEdgeIterator &operator++(); // prefix
        ConstRexEdgeIterator operator++(int); // postfix

        friend ConstRexEdges; // Ability to access the private constructor
    };

    // For use in range-for loops
    class ConstRexEdges {
        const TAGraph &graph;

        ConstRexEdges(const TAGraph &graph);
    public:
        ConstRexEdgeIterator begin() const;
        ConstRexEdgeIterator end() const;

        friend TAGraph; // Ability to access the private constructor
    };

    // Constructor/Destructor
    TAGraph();
    virtual ~TAGraph();

    // Node/Edge Adders
    virtual void addNode(RexNode *node);
    virtual void addEdge(RexEdge *edge);

    // Find Methods
    RexNode *findNode(const std::string &nodeID);
    RexEdge *findEdge(const std::string &srcID, const std::string &dstID, RexEdge::EdgeType type);
    const std::vector<RexEdge *> &findEdgesBySrcID(const std::string &srcID);
    const std::vector<RexEdge *> &findEdgesByDstID(const std::string &dstID);

    // Element Exist Methods
    bool hasNode(const std::string &nodeID);
    bool hasEdge(const std::string &srcID, const std::string &dstID, RexEdge::EdgeType type);

    // Size/Counting methods
    unsigned int keptNodes() const;
    unsigned int unestablishedEdgesSize() const;
    unsigned int establishedEdgesSize() const;
    unsigned int nodesWithAttrs() const;
    unsigned int edgesWithAttrs() const;

    // Iterators
    ConstRexNodes nodes() const;
    ConstRexEdges edges() const;

private:
    // Member variables
    std::unordered_map<std::string, RexNode *> idList;
    std::unordered_map<std::string, std::vector<RexEdge *>> edgeSrcList;
    std::unordered_map<std::string, std::vector<RexEdge *>> edgeDstList;
    std::vector<RexEdge *> emptyVec; // for out of range situations
};

#endif // REX_TAGRAPH_H
