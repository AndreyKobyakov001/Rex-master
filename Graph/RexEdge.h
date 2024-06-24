/////////////////////////////////////////////////////////////////////////////////////////////////////////
// RexEdge.h
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

#ifndef REX_REXEDGE_H
#define REX_REXEDGE_H

#include "RexNode.h"
#include <string>

class RexCond;

class RexEdge {
  public:
    // Node Type Information
    enum EdgeType {
        CONTAINS,
        OBJ,
        MUTATE,
        VAR_WRITES,
        PAR_WRITES,
        RET_WRITES,
        REFERENCES,
        CALLS,
        READS,
        WRITES,
        ADVERTISE,
        SUBSCRIBE,
        PUBLISH,
        VAR_INFLUENCE,
        VAR_INFLUENCE_FUNC,
        SET_TIME,
        PUBLISH_VARIABLE,
        PUBLISH_TARGET,
        // If function `f` calls function `g` in f's CFG node `n`, add relation
        // INVOKES `n` `e`, where `e` is g's CFG entry node
        NEXT_CFG_BLOCK,
        RET_CFG_BLOCK,
        FUNCTION_CFG_LINK,
        // Edges for linking facts to CFG Basic Block nodes
        VW_SOURCE,
        VW_DESTINATION,
        PW_SOURCE,
        PW_DESTINATION,
        RW_SOURCE,
        RW_DESTINATION,
        W_SOURCE,
        W_DESTINATION,
        VIF_SOURCE,
        VIF_DESTINATION,
        VI_SOURCE,
        VI_DESTINATION,
        C_SOURCE,
        C_DESTINATION,
        PV_SOURCE,
        PV_DESTINATION,
        PT_SOURCE,
        PT_DESTINATION,
        SUB_SOURCE,
        SUB_DESTINATION,
        PUB_SOURCE,
        PUB_DESTINATION,
        // This must always be the last variant. It is used to help us know how
        // many edges types there are when we output the TA schema.
        NUM_EDGE_TYPES
    };
    static const char *typeToString(EdgeType type);
    static EdgeType stringToType(const std::string &s);

    // For sorting:
    // An "EdgeLike" is any type that has similar getters to RexEdge
    // Needed to allow other non-RexEdge objects to use this function
    template<class EdgeLike>
    static bool compare(const EdgeLike &left, const EdgeLike &right) {
        // This is a really simple function, but we need its logic to be consistent
        // in several places. That's why we've abstracted it here.
        std::string lhs = typeToString(left.getType()) + left.getSourceID() + left.getDestinationID();
        std::string rhs = typeToString(right.getType()) + right.getSourceID() + right.getDestinationID();
        return lhs < rhs;
    }

    // Constructor/Destructor
    RexEdge(RexNode *src, RexNode *dst, EdgeType type);
    RexEdge(std::string srcID, std::string dstID, EdgeType type);
    RexEdge(RexNode *src, std::string dstID, EdgeType type);
    RexEdge(std::string srcID, RexNode *dst, EdgeType type);

    // Information Methods
    bool isEstablished() const;

    // Getters
    RexNode *getSource() const;
    RexNode *getDestination() const;
    EdgeType getType() const;
    const std::string &getSourceID() const;
    const std::string &getDestinationID() const;

    // Attribute Manager
    unsigned int getNumAttributes() const;
    unsigned int getNumSingleAttributes() const;
    unsigned int getNumMultiAttributes() const;
    void addSingleAttribute(std::string key, std::string value);
    void addMultiAttribute(std::string key, std::string value);
    const std::map<std::string, std::string> &getSingleAttributes() const;
    const std::map<std::string, std::vector<std::string>> &getMultiAttributes() const;
    
    // Variability Aware
    void setCondID(std::string name);
    void setCond(RexCond *);
    void addDisjunction(RexCond *);
    std::string getCondID();
    RexCond *getCond();
    ~RexEdge();

  private:
    RexNode *sourceNode;
    RexNode *destNode;

    std::string sourceID;
    std::string destID;

    EdgeType type;

    std::map<std::string, std::string> singleAttributes;
    std::map<std::string, std::vector<std::string>> multiAttributes;
    
    // Variability aware
    RexCond *cond = nullptr;
    std::string condID;
};

#endif // REX_REXEDGE_H
