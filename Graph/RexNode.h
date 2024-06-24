/////////////////////////////////////////////////////////////////////////////////////////////////////////
// RexNode.h
//
// Created By: Bryan J Muscedere
// Date: 10/04/17.
//
// Modified By: Davood Anbarnam
//
// Maintains an node structure. Basic
// system for storing information about
// edge information and for printing to
// TA.
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

#ifndef REX_REXNODE_H
#define REX_REXNODE_H

#include <map>
#include <string>
#include <vector>

class RexCond;

class RexNode {
  public:
    // Node Type Information
    //
    // NOTE: When adding to this enum, you may also want to consider specifying
    // where it falls in the inheritance hierarchy (TASchema.cpp) and which
    // attributes it is allowed to have (TASchemeAttribute.cpp)
    enum NodeType {
        // Parent node types (inner nodes on the inheritance tree)

        // Root of the inheritance hierarchy, do not use directly
        ROOT,
        // Nodes that are a regular part of the C++ AST, do not use directly
        ASG_NODE,
        // Nodes that have to do with ROS pub/sub, do not use directly
        ROS_MSG,

        // Specific node types (leaf nodes on the inheritance tree)

        FUNCTION,
        VARIABLE,
        CLASS,
        STRUCT,
        UNION,
        ENUM,
        RETURN,
        TOPIC,
        PUBLISHER,
        SUBSCRIBER,
        TIMER,
        NODE_HANDLE,
        CFG_BLOCK,
    };
    static const char *typeToString(NodeType type);
    static NodeType stringToType(const std::string &s);

    // For sorting:
    // A "NodeLike" is any type that has similar getters to RexNode
    // Needed to allow other non-RexNode objects to use this function
    template<class NodeLike>
    static bool compare(const NodeLike &left, const NodeLike &right) {
        // This is a really simple function, but we need its logic to be consistent
        // in several places. That's why we've abstracted it here.
        return left.getID() < right.getID();
    }

    // Constructor/Destructor
    RexNode(std::string id, NodeType type, bool shouldKeep);

    // Getters
    NodeType getType() const;
    bool keep() const;
    const std::string &getID() const;
    const std::map<std::string, std::string> &getSingleAttributes() const;
    const std::map<std::string, std::vector<std::string>> &getMultiAttributes() const;
    unsigned int getNumAttributes() const;
    unsigned int getNumSingleAttributes() const;
    unsigned int getNumMultiAttributes() const;

    // Attribute Managers
    void addSingleAttribute(std::string key, std::string value);
    void addMultiAttribute(std::string key, std::string value);
    
    // Variability Aware
    RexCond *getCond();
    void setCond(RexCond *);
    ~RexNode();

  private:
    std::string id;
    NodeType type;
    // Used to filter the nodes included in .tao files
    bool shouldKeep;

    std::map<std::string, std::string> singleAttributes;
    std::map<std::string, std::vector<std::string>> multiAttributes;
    
    // Variability Aware
    RexCond *cond = nullptr;
};

#endif // REX_REXNODE_H