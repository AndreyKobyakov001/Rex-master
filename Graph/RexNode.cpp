/////////////////////////////////////////////////////////////////////////////////////////////////////////
// RexNode.cpp
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

#include "RexNode.h"
#include "../Walker/CondScope.h"
#include <iostream>
using namespace std;

/**
 * Converts an node type to a string representation.
 * @param type The node type.
 * @return The string representation.
 */
const char *RexNode::typeToString(RexNode::NodeType type) {
    switch (type) {
        case ROOT: return "cRoot";
        case ASG_NODE: return "cAsgNds";
        case ROS_MSG: return "rosMsg";
        case FUNCTION: return "cFunction";
        case VARIABLE: return "cVariable";
        case CLASS: return "cClass";
        case STRUCT: return "cStruct";
        case UNION: return "cUnion";
        case ENUM: return "cEnum";
        case RETURN: return "cReturn";
        case TOPIC: return "rosTopic";
        case PUBLISHER: return "rosPublisher";
        case SUBSCRIBER: return "rosSubscriber";
        case TIMER: return "rosTimer";
        case NODE_HANDLE: return "rosNodeHandle";
        case CFG_BLOCK: return "cCFGBlock";

        // No default case because we want the compiler to warn us when this
        // enum changes and a case isn't covered
    }

    throw domain_error("Unknown node type");
}

RexNode::NodeType RexNode::stringToType(const std::string &s) {
    // Unfortunately there is no way to write this code so that the compiler
    // will warn us to update it. You just have to remember to do that when you
    // add a new type.

    if (s == "cRoot") return ROOT;
    else if (s == "cAsgNds") return ASG_NODE;
    else if (s == "rosMsg") return ROS_MSG;
    else if (s == "cFunction") return FUNCTION;
    else if (s == "cVariable") return VARIABLE;
    else if (s == "cClass") return CLASS;
    else if (s == "cStruct") return STRUCT;
    else if (s == "cUnion") return UNION;
    else if (s == "cEnum") return ENUM;
    else if (s == "cReturn") return RETURN;
    else if (s == "rosTopic") return TOPIC;
    else if (s == "rosPublisher") return PUBLISHER;
    else if (s == "rosSubscriber") return SUBSCRIBER;
    else if (s == "rosTimer") return TIMER;
    else if (s == "rosNodeHandle") return NODE_HANDLE;
    else if (s == "cCFGBlock") return CFG_BLOCK;
    else {
        throw domain_error("Unknown node type: " + s);
    }
}

// Creates a Rex node with the given ID and type. Set shouldKeep to true if
// this node should be recorded in the .tao file for the compilation unit
// currently being walked.
//
// Note that there is no way to set the type of a node once it is constructed.
// Having a `setType` wouldn't always work because not all types are valid for
// all IDs.
RexNode::RexNode(std::string id, NodeType type, bool shouldKeep):
    id{id}, type{type}, shouldKeep{shouldKeep} {}

/**
 * Gets the ID.
 * @return The node ID.
 */
const std::string &RexNode::getID() const { return id; }

// Returns true if this node was marked as a node that should be kept
bool RexNode::keep() const {
    return shouldKeep;
}

/**
 * Gets the type.
 * @return The node type.
 */
RexNode::NodeType RexNode::getType() const { return type; }

/**
 * Gets a mapping of single attributes by ID
 * @return The mapping of attributes by ID
 */
const std::map<std::string, std::string> &RexNode::getSingleAttributes() const {
    return singleAttributes;
}

/**
 * Gets a mapping of multi attributes by ID.
 * @return a mapping of multi attributes by ID..
 */
const std::map<std::string, std::vector<std::string>> &RexNode::getMultiAttributes() const {
    return multiAttributes;
}

/**
 * Gets the number of attributes.
 * @return The number of attributes.
 */
unsigned int RexNode::getNumAttributes() const {
    return getNumSingleAttributes() + getNumMultiAttributes();
}

unsigned int RexNode::getNumSingleAttributes() const {
    return singleAttributes.size();
}

unsigned int RexNode::getNumMultiAttributes() const {
    return multiAttributes.size();
}

/**
 * Adds a single attribute.
 *
 * @param key The key.
 * @param value The value.
 */
void RexNode::addSingleAttribute(std::string key, std::string value) {
    // Overwrite the value if any is already present
    singleAttributes[key] = value;
}

/**
 * Adds a multi attribute.
 *
 * @param key The key.
 * @param value The value.
 */
void RexNode::addMultiAttribute(std::string key, std::string value) {
    // Add the new value to the list.
    multiAttributes[key].push_back(value);
}


// Variability Aware
RexNode::~RexNode(){ 
    delete cond;
}

/**
 * Gets the corresponding RexCond
 * @return pointer to this nodes RexCond. May be the nullptr.
 */
RexCond *RexNode::getCond() {
    return cond;
}

/**
 * Sets the cond for this node.
 * @param rc - a heap allocated RexCond that this function gets to own
 */
void RexNode::setCond(RexCond *rc) {
    if (!rc) return;
    cond = rc;
    if (cond) singleAttributes["condition"] = cond->to_string(); 
}