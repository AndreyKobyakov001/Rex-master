////////////////////////////////////////////////////////////////////////////////////////////////////////
// ROSWalker.h
//
// Created By: Rob Hackman
// Date: 06/07/20.
//
// Walks through Clang's AST using adding Variability
// mode methodology. This is achieved using the ROS
// walker class to help obtain information about each
// AST node.
//
// Copyright (C) 2017, Rob Hackman
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

#include "VarWalker.h"

#include <fstream>
#include <iostream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <clang/AST/Mangle.h>

#include "../Graph/RexNode.h"
#include "ROSConsumer.h"
#include "CondScope.h"
#include "RexID.h"
using namespace llvm;
using namespace clang;
using namespace std;

/**
 * Constructor
 * @param Context AST Context
 */
VarWalker::VarWalker(const WalkerConfig &config, ASTContext *Context) :
        ROSWalker{config, Context}, condScope{Context, config.vOpts} {}

/**
 * Destructor
 */
VarWalker::~VarWalker() {}


void VarWalker::addEdgeToGraph(RexEdge *e) {
    RexCond *c = condScope.conjunction();
    e->setCond(c);
    graph.addEdge(e);
}

void VarWalker::addNodeToGraph(RexNode *n) {
    RexCond *c = condScope.conjunction();
    n->setCond(c);
    graph.addNode(n);
}

void VarWalker::updateEdge(RexEdge *e) {
    e->addDisjunction(condScope.conjunction());
}

// In C++17, an IfStmt has at most five child expressions:
// Init, ConditionVariableDeclStmt, Cond, Then, and Else.
// In the example "if (int x = 5; bool b = x > 5) {}", we have:
// Init is "int x = 5"
// ConditionVariableDeclStatement is "bool b = x > 5"
// Cond is "b"
bool VarWalker::TraverseIfStmt(IfStmt *s) {
    if (!s) return true;
    WalkUpFromIfStmt(s); // Visits s

    // Traverse children that are evaluated unconditionally
    RecursiveASTVisitor<BaselineWalker>::TraverseStmt(s->getInit());
    RecursiveASTVisitor<BaselineWalker>::TraverseStmt(s->getConditionVariableDeclStmt());
    RecursiveASTVisitor<BaselineWalker>::TraverseStmt(s->getCond());

    // Traverse children that are evaluated conditionally
    condScope.push(s);
    RecursiveASTVisitor<BaselineWalker>::TraverseStmt(s->getThen());
    condScope.negate_top(s);
    RecursiveASTVisitor<BaselineWalker>::TraverseStmt(s->getElse());
    condScope.pop(s);

    return true;
}

bool VarWalker::TraverseSwitchStmt(SwitchStmt *s) {
    if (!s) return true;
    WalkUpFromSwitchStmt(s); // Visits s
    condScope.push(s);
    TraverseChildren(s);
    condScope.pop(s);
    return true;
}

bool VarWalker::TraverseCaseStmt(CaseStmt *s) {
    if (!s) return true;
    WalkUpFromCaseStmt(s); // Visits s
    condScope.push(s);
    TraverseChildren(s);
    condScope.pop(s);
    return true;
}

bool VarWalker::TraverseWhileStmt(WhileStmt *s) {
    if (!s) return true;
    WalkUpFromWhileStmt(s); // Visits s
    condScope.push(s);
    TraverseChildren(s);
    condScope.pop(s);
    return true;
}

bool VarWalker::TraverseForStmt(ForStmt *s) {
    if (!s) return true;
    WalkUpFromForStmt(s); // Visits s
    condScope.push(s);
    TraverseChildren(s);
    condScope.pop(s);
    return true;
}

bool VarWalker::TraverseDoStmt(DoStmt *s) {
    if (!s) return true;
    WalkUpFromDoStmt(s); // Visits s
    condScope.push(s);
    TraverseChildren(s);
    condScope.pop(s);
    return true;
}

bool VarWalker::TraverseChildren(Stmt *s) {
    for (auto & child : s->children()) {
        RecursiveASTVisitor<BaselineWalker>::TraverseStmt(child);
    }
    return true;
}
