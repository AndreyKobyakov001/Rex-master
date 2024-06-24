////////////////////////////////////////////////////////////////////////////////////////////////////////
// CondScope.h
//
// Created By: Rob Hackman
// Date: 15/02/19.
//
// Helper class that simply maintains a stack of conditions we are
// under within our walk. The ROSWalker class will maintain a
// CondScope object which helps it keep track of how many conditions
// deep it is in its walk, and what those are. This is for tying
// feature expressions to the facts extracted.
//
// Copyright (C) 2019, Rob Hackman
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
#ifndef CONDSCOPE_H_
#define CONDSCOPE_H_
#include "clang/AST/Stmt.h"
#include <vector>
#include <string>
#include "../Driver/RexArgs.h"
#include "clang/AST/Expr.h"
#include <regex>

class RexCond {
protected:
    static unsigned long UID;
    unsigned long pUID;
public:
    RexCond();
    virtual ~RexCond();
    virtual std::string to_string() = 0;
    virtual RexCond *copy() = 0;
    virtual std::string getTopID() = 0;
    virtual bool equivalent(RexCond *) = 0;
    virtual bool containsCfgVar() = 0;
    static unsigned long getUID();
};

class RexUnaryOp : public RexCond {
    RexCond *child;
    std::string op;
    RexUnaryOp(std::string, RexCond *);
public:
    static RexCond* makeUnaryOp(std::string, RexCond *);
    virtual ~RexUnaryOp();
    std::string to_string() override;
    RexCond *copy() override;
    std::string getTopID() override;
    bool containsCfgVar() override;
    bool equivalent(RexCond *) override;
};

class RexBinaryOp : public RexCond {
    RexCond *lhs;
    RexCond *rhs;
    std::string op;
    // Constructor has been made private so that these can only
    // be constructed through the factory.
    RexBinaryOp(std::string, RexCond *, RexCond *);
public:
    // Factory for creating BinaryOps. Could give a RexTrue or RexBinaryOp pointer.
    static RexCond *makeBinaryOp(std::string, RexCond *, RexCond *);

    virtual ~RexBinaryOp();
    std::string to_string() override;
    RexCond *copy() override;
    std::string getTopID() override;
    bool containsCfgVar() override;
    bool equivalent(RexCond *) override;
};

class RexCondAtom : public RexCond {
    clang::Expr *expr;
    const VariabilityOptions &vOpts;
public:
    RexCondAtom(clang::Expr *, const VariabilityOptions &vOpts);
    std::string to_string() override;
    RexCond *copy() override;
    std::string getTopID() override;
    bool containsCfgVar() override;
    bool equivalent(RexCond *) override;
};

class RexCondParen : public RexCond {
    RexCond *child;
public:
    RexCondParen(RexCond *);
    ~RexCondParen();
    std::string to_string() override;
    RexCond *copy() override;
    std::string getTopID() override;
    bool containsCfgVar() override;
    bool equivalent(RexCond *) override;
};

class RexTrue : public RexCond {
public:
    std::string to_string() override;
    RexCond *copy() override;
    std::string getTopID() override;
    bool containsCfgVar() override;
    bool equivalent(RexCond *) override;
};

class RexFnCall : public RexCond {
    std::vector<RexCond *> args;
    const VariabilityOptions &vOpts;
    std::string fnName;
    RexFnCall(std::string fnName, std::vector<RexCond *> args, const VariabilityOptions &vOpts);
    RexFnCall(clang::CallExpr *, std::vector<RexCond *> args, const VariabilityOptions &vOpts);
  public:
    static RexCond *makeFnCall(clang::CallExpr *expr, std::vector<RexCond *> args, const VariabilityOptions &vOpts) {
        if (!expr) return new RexTrue;
        for (auto &p : args) {
            if (!p) return new RexTrue;
        }
        return new RexFnCall{expr,args,vOpts};
    }
    ~RexFnCall();
    std::string to_string() override;
    RexCond *copy() override;
    std::string getTopID() override;
    bool containsCfgVar() override;
    bool equivalent(RexCond *) override;
};

class RexTernary : public RexCond {
    RexCond *ifC, *thenC, *elseC;
    const VariabilityOptions &vOpts;
  public:
    RexTernary(RexCond *ifC, RexCond *thenC, RexCond *elseC, const VariabilityOptions &vOpts);
    ~RexTernary();
    std::string to_string() override;
    RexCond *copy() override;
    std::string getTopID() override;
    bool containsCfgVar() override;
    bool equivalent(RexCond *) override;
};

class CondScope {
    std::vector<RexCond*> condStack;
    std::vector<RexCond*> ifStack;
    std::vector<std::vector<RexCond*> > caseStack;
    std::vector<clang::SwitchStmt*> switchVar;
    RexCond *makeRexCond(clang::Expr *cond);
    RexCond *disjunction(std::vector<RexCond*>);

    VariabilityOptions vOpts;
    std::regex cfgVarRegex;

    clang::ASTContext *context;

public:
    void push(clang::Stmt* s);
    void push(clang::SwitchStmt*);
    void push(clang::CaseStmt*);
    void push(clang::IfStmt*);
    void negate_top(clang::IfStmt*);
    void pop();
    void pop(clang::Stmt*);
    void pop(clang::SwitchStmt*);
    void pop(clang::IfStmt*);
    void pop(clang::CaseStmt*);
    RexCond *conjunction();
    CondScope(clang::ASTContext *context, VariabilityOptions vOpts = VariabilityOptions{});
    ~CondScope();
    //rexCond peek();
    // rexCond fullCond();
};
#endif //CONDSCOPE_H_
