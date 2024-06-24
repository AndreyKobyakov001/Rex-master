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

#ifndef REX_VARWALKER_H
#define REX_VARWALKER_H

#include <stack>

#include "ROSWalker.h"
#include "CondScope.h"

class VarWalker : public ROSWalker {
public:
    VarWalker(const WalkerConfig &config, clang::ASTContext *Context);
    virtual ~VarWalker() override;

    // Variability Aware.
    bool TraverseIfStmt(clang::IfStmt *s) override;
    bool TraverseSwitchStmt(clang::SwitchStmt *s) override;
    bool TraverseCaseStmt(clang::CaseStmt *s) override;
    bool TraverseWhileStmt(clang::WhileStmt *s) override;
    bool TraverseForStmt(clang::ForStmt *s) override;
    bool TraverseDoStmt(clang::DoStmt *s) override;
    bool TraverseChildren(clang::Stmt *s) override;
private:
    CondScope condScope;

    virtual void addEdgeToGraph(RexEdge *) override;
    virtual void addNodeToGraph(RexNode *) override;
    virtual void updateEdge(RexEdge *) override;
};


#endif
