/////////////////////////////////////////////////////////////////////////////////////////////////////////
// RexEdge.cpp
//
// Created By: Bryan J Muscedere
// Date: 10/04/17.
//
// Modified By: Davood Anbarnam
//
// Maintains an edge structure. Basic
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

#include "RexEdge.h"
#include <iostream>
#include <stdexcept>
#include "../Walker/CondScope.h"

using namespace std;

/**
 * Converts an edge type to a string representation.
 * @param type The edge type.
 * @return The string representation.
 */
const char *RexEdge::typeToString(RexEdge::EdgeType type) {
    switch (type) {
        case CONTAINS: return "contain";
        case REFERENCES: return "reference";
        case CALLS: return "call";
        case READS: return "read";
        case WRITES: return "write";
        case ADVERTISE: return "advertise";
        case SUBSCRIBE: return "subscribe";
        case PUBLISH: return "publish";
        case VAR_WRITES: return "varWrite";
        case PAR_WRITES: return "parWrite";
        case RET_WRITES: return "retWrite";
        case VAR_INFLUENCE: return "varInfluence";
        case VAR_INFLUENCE_FUNC: return "varInfFunc";
        case SET_TIME: return "time";
        case PUBLISH_VARIABLE: return "pubVar";
        case PUBLISH_TARGET: return "pubTarget";
        case OBJ: return "obj";
        case MUTATE: return "mutate";
        case NEXT_CFG_BLOCK: return "nextCFGBlock";
        case RET_CFG_BLOCK: return "retCFGBlock";
        case FUNCTION_CFG_LINK: return "functionCFGLink";
        case VW_SOURCE: return "varWriteSource";
        case VW_DESTINATION: return "varWriteDestination";
        case PW_SOURCE: return "parWriteSource";
        case PW_DESTINATION: return "parWriteDestination";
        case RW_SOURCE: return "retWriteSource";
        case RW_DESTINATION: return "retWriteDestination";
        case W_SOURCE: return "writeSource";
        case W_DESTINATION: return "writeDestination";
        case VIF_SOURCE: return "varInfFuncSource";
        case VIF_DESTINATION: return "varInfFuncDestination";
        case VI_SOURCE: return "varInfluenceSource";
        case VI_DESTINATION: return "varInfluenceDestination";
        case C_SOURCE: return "callSource";
        case C_DESTINATION: return "callDestination";
        case PV_SOURCE: return "pubVarSource";
        case PV_DESTINATION: return "pubVarDestination";
        case PT_SOURCE: return "pubTargetSource";
        case PT_DESTINATION: return "pubTargetDestination";
        case SUB_SOURCE: return "subscribeSource";
        case SUB_DESTINATION: return "subscribeDestination";
        case PUB_SOURCE: return "publishSource";
        case PUB_DESTINATION: return "publishDestination";
        // No default case because we want the compiler to warn us when this
        // enum changes and a case isn't covered

        // Should never happen
        case NUM_EDGE_TYPES: throw "unreachable";
    }

    throw domain_error("Unknown edge type");
}

RexEdge::EdgeType RexEdge::stringToType(const std::string &s) {
    // Unfortunately there is no way to write this code so that the compiler
    // will warn us to update it. You just have to remember to do that when you
    // add a new type.
    if (s == "contain") return CONTAINS;
    else if (s == "reference") return REFERENCES;
    else if (s == "call") return CALLS;
    else if (s == "read") return READS;
    else if (s == "write") return WRITES;
    else if (s == "advertise") return ADVERTISE;
    else if (s == "subscribe") return SUBSCRIBE;
    else if (s == "publish") return PUBLISH;
    else if (s == "varWrite") return VAR_WRITES;
    else if (s == "parWrite") return PAR_WRITES;
    else if (s == "retWrite") return RET_WRITES;
    else if (s == "varInfluence") return VAR_INFLUENCE;
    else if (s == "varInfFunc") return VAR_INFLUENCE_FUNC;
    else if (s == "time") return SET_TIME;
    else if (s == "pubVar") return PUBLISH_VARIABLE;
    else if (s == "pubTarget") return PUBLISH_TARGET;
    else if (s == "obj") return OBJ;
    else if (s == "mutate") return MUTATE;
    else if (s == "nextCFGBlock") return NEXT_CFG_BLOCK;
    else if (s == "retCFGBlock") return RET_CFG_BLOCK;
    else if (s == "functionCFGLink") return FUNCTION_CFG_LINK;
    else if (s == "varWriteSource") return VW_SOURCE;
    else if (s == "varWriteDestination") return VW_DESTINATION;
    else if (s == "parWriteSource") return PW_SOURCE;
    else if (s == "parWriteDestination") return PW_DESTINATION;
    else if (s == "retWriteSource") return RW_SOURCE;
    else if (s == "retWriteDestination") return RW_DESTINATION;
    else if (s == "writeSource") return W_SOURCE;
    else if (s == "writeDestination") return W_DESTINATION;
    else if (s == "varInfFuncSource") return VIF_SOURCE;
    else if (s == "varInfFuncDestination") return VIF_DESTINATION;
    else if (s == "varInfluenceSource") return VI_SOURCE;
    else if (s == "varInfluenceDestination") return VI_DESTINATION;
    else if (s == "callSource") return C_SOURCE;
    else if (s == "callDestination") return C_DESTINATION;
    else if (s == "pubVarSource") return PV_SOURCE;
    else if (s == "pubVarDestination") return PV_DESTINATION;
    else if (s == "pubTargetSource") return PT_SOURCE;
    else if (s == "pubTargetDestination") return PT_DESTINATION;
    else if (s == "subscribeSource") return SUB_SOURCE;
    else if (s == "subscribeDestination") return SUB_DESTINATION;
    else if (s == "publishSource") return PUB_SOURCE;
    else if (s == "publishDestination") return PUB_DESTINATION;
    else throw domain_error("Unknown edge type");
}

/**
 * Creates an established edge based on two Rex nodes.
 * @param src The pointer to the source.
 * @param dst The pointer to the destination.
 * @param type The edge type.
 */
RexEdge::RexEdge(RexNode *src, RexNode *dst, RexEdge::EdgeType type)
    : sourceNode{src}, destNode{dst},
      sourceID{src->getID()}, destID{dst->getID()},
      type{type} {}

/**
 * Creates an unestablished edge based on two Rex nodes.
 * @param srcID The string of the source ID.
 * @param dstID The string of the destination ID.
 * @param type The edge type.
 */
RexEdge::RexEdge(string srcID, string dstID, RexEdge::EdgeType type)
    : sourceNode{nullptr}, destNode{nullptr},
      sourceID{srcID}, destID{dstID},
      type{type} {}

/**
 * Creates an unestablished edge based on two Rex nodes.
 * @param srcID The pointer to the source.
 * @param dstID The string of the destination ID.
 * @param type The edge type.
 */
RexEdge::RexEdge(RexNode *src, string dstID, RexEdge::EdgeType type)
    : sourceNode{src}, destNode{nullptr},
      sourceID{src->getID()}, destID{dstID},
      type{type} {}

/**
 * Creates an unestablished edge based on two Rex nodes.
 * @param srcID The string of the source ID.
 * @param dst The pointer to the destination.
 * @param type The edge type.
 */
RexEdge::RexEdge(string srcID, RexNode *dst, RexEdge::EdgeType type)
    : sourceNode{nullptr}, destNode{dst},
      sourceID{srcID}, destID{dst->getID()},
      type{type} {}

/**
 * Checks whether the edge is established.
 * @return Whether the edge is established.
 */
bool RexEdge::isEstablished() const {
    // Check to see if the nodes are in place.
    return sourceNode && sourceNode->keep() && destNode && destNode->keep();
}

/**
 * Gets the source node.
 * @return The node of the source.
 */
RexNode *RexEdge::getSource() const {
    return sourceNode;
}

/**
 * Gets the destination node.
 * @return The node of the destination.
 */
RexNode *RexEdge::getDestination() const {
    return destNode;
}

/**
 * Gets the edge type.
 * @return The edge type.
 */
RexEdge::EdgeType RexEdge::getType() const {
    return type;
}

/**
 * Gets the source ID.
 * @return The source ID.
 */
const string &RexEdge::getSourceID() const {
    return sourceNode ? sourceNode->getID() : sourceID;
}

/**
 * Gets the destination ID.
 * @return The destination ID.
 */
const string &RexEdge::getDestinationID() const {
    return destNode ? destNode->getID() : destID;
}

/**
 * Gets the number of attributes.
 * @return The number of attributes.
 */
unsigned int RexEdge::getNumAttributes() const {
    return getNumSingleAttributes() + getNumMultiAttributes();
}

unsigned int RexEdge::getNumSingleAttributes() const {
    return singleAttributes.size();
}

unsigned int RexEdge::getNumMultiAttributes() const {
    return multiAttributes.size();
}

/**
 * Adds an attribute that only has one value.
 * @param key The key of the attribute.
 * @param value The value of the attribute.
 */
void RexEdge::addSingleAttribute(std::string key, std::string value) {
    singleAttributes[key] = value;
}

/**
 * Adds an attribute that has multiple values.
 * @param key The key of the attribute.
 * @param value The value of the attribute.
 */
void RexEdge::addMultiAttribute(string key, string value) {
    multiAttributes[key].push_back(value);
}

/**
 * Gets a mapping of single attributes by ID
 * @return The mapping of attributes by ID
 */
const std::map<std::string, std::string> &RexEdge::getSingleAttributes() const {
    return singleAttributes;
}

/**
 * Gets a mapping of multi attributes by ID.
 * @return a mapping of multi attributes by ID..
 */
const std::map<std::string, std::vector<std::string>> &RexEdge::getMultiAttributes() const {
    return multiAttributes;
}


// Variability aware
RexEdge::~RexEdge() {
    delete cond;
}


/**
 * Sets the condID of the edge.
 * @param ID The condID of this node.
 */
void RexEdge::setCondID(string ID) {
    condID = ID;
}

/**
 * Sets the cond for this node.
 * @param rc - a heap allocated RexCond that this function gets to own
 */
void RexEdge::setCond(RexCond *rc) {
    if (!rc) return;
    if (!cond) {
        cond = rc;
    } else {
        addDisjunction(rc);
    } 
    if (cond) singleAttributes["condition"] = cond->to_string();
}

/**
 * Updates the cond for this edge with a disjunction. If no cond is set
 * then this becomes the cond, if a cond is set then the cond for this
 * edge becomes a disjunction of the current cond and the parameter
 * @param rc - a heap allocated RexCond that this function gets to own
 */
void RexEdge::addDisjunction(RexCond *rc) {
    if (!rc) return;
    if (!cond) {
        cond = RexBinaryOp::makeBinaryOp("||", new RexTrue{}, rc);
    } else {
        if (!cond->equivalent(rc)) {
            cond = RexBinaryOp::makeBinaryOp("||", cond, rc);
        } else {
            delete rc;
        }
    }
    if (cond) singleAttributes["condition"] = cond->to_string();
}

/**
 * Gets the condID of the edge.
 * @return The condID.
 */
string RexEdge::getCondID() {
    return condID;
}

/**
 * Gets the corresponding RexCond
 * @return pointer to this edge's RexCond. May be the nullptr.
 */
RexCond *RexEdge::getCond() {
    return cond;
}
