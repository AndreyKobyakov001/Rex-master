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
#include "CondScope.h"
// TODO: Clean up includes... may have gotten fed up and just
//       copy pasted them from RexWalker...
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/PrettyPrinter.h"
#include "llvm/Support/raw_os_ostream.h"
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <regex>

using namespace llvm;
using namespace clang;
using namespace std;

long unsigned int RexCond::UID = 0;

RexCond::RexCond() : pUID{UID++} {}

RexCond::~RexCond() {}

long unsigned int RexCond::getUID() {
    return UID;
}

RexCond* makeUnaryOp(std::string op, RexCond *c) {
    return RexUnaryOp::makeUnaryOp(op, c);
}

RexCond* makeFnCall(clang::CallExpr *expr, std::vector<RexCond *> args, const VariabilityOptions &vOpts) {
    return RexFnCall::makeFnCall(expr, args, vOpts);
}

RexUnaryOp::RexUnaryOp(std::string op, RexCond *c) : child{c}, op{op} {}

RexCond* RexUnaryOp::makeUnaryOp(std::string op, RexCond *c) {
    if (!c) return new RexTrue;
    return new RexUnaryOp(op, c);
}

RexUnaryOp::~RexUnaryOp() {
    delete child;
}
string RexUnaryOp::to_string() {
    return op + child->to_string();
}

RexCond *RexUnaryOp::copy() {
    return RexUnaryOp::makeUnaryOp(op, child->copy());
}

string RexUnaryOp::getTopID() {
    if (op == "!") {
        return "NOT" + std::to_string(pUID);
    } else {
        return child->getTopID();
    }
}


bool RexUnaryOp::containsCfgVar() {
    return child->containsCfgVar();
}

bool RexUnaryOp::equivalent(RexCond *node) {
    RexUnaryOp *other = dynamic_cast<RexUnaryOp*>(node);
    if (!other) return false;
    if (op != other->op) return false;
    return child->equivalent(other->child);
}

RexBinaryOp::RexBinaryOp(std::string op, RexCond * l, RexCond *r) : lhs{l}, rhs{r}, op{op} {
    if (!lhs) {
        cerr << "Missing lhs"<< endl;
        throw 10;
    }
    if (!rhs) {
        cerr << "Missing rhs" << endl;
        throw "hello";
    }
}

RexCond *makeBinaryOp(std::string op, RexCond *l, RexCond *r) {
    return RexBinaryOp::makeBinaryOp(op, l, r);
}

RexCond *RexBinaryOp::makeBinaryOp(std::string op, RexCond *l, RexCond *r) {
    // Factory to just make RexTrue's if the binaryOp is not creatable.
    // Possibly not the best solution but at the moment it might make sense
    // to ignore conditions that can't be resolved. Hopefully it is mostly
    // things the user should not care about anyways (implicit things such as range loops)
    if (!l || !r) {
        return new RexTrue;
    }
    return new RexBinaryOp(op, l, r);
}

RexBinaryOp::~RexBinaryOp() {
    delete lhs;
    delete rhs;
}

string RexBinaryOp::to_string() {
    string suff = ")";
    string pref = "(";
    if (op == "||") {
        suff = ")";
        pref = "(";
    }
    return pref + lhs->to_string() + op + rhs->to_string() + suff;
}

RexCond *RexBinaryOp::copy() {
    return RexBinaryOp::makeBinaryOp(op, lhs->copy(), rhs->copy());
}

string RexBinaryOp::getTopID() {
    if (op == "&&") {
        return "AND" + std::to_string(pUID);
    } else if (op == "||") {
        return "OR" + std::to_string(pUID);
    } else if (op == "==") {
        return "EQUAL" + std::to_string(pUID);
    }
    return "ARBOP" + std::to_string(pUID);
}

bool RexBinaryOp::containsCfgVar() {
    return lhs->containsCfgVar() || \
           rhs->containsCfgVar();
}

bool RexBinaryOp::equivalent(RexCond *node) {
    RexBinaryOp *other = dynamic_cast<RexBinaryOp*>(node);
    if (!other) return false;
    if (op != other->op) return false;
    return lhs->equivalent(other->lhs) && rhs->equivalent(other->rhs);
}


RexCondAtom::RexCondAtom(Expr *e,const VariabilityOptions &vOpts) :
    expr{e}, vOpts{vOpts} {}

string RexCondAtom::to_string() {
    if (auto ref = dyn_cast<DeclRefExpr>(expr)) {
        return ref->getFoundDecl()->getNameAsString();
    } else if (auto ref = dyn_cast<IntegerLiteral>(expr)) {
        return ref->getValue().toString(10,true);
    } else if (auto ref = dyn_cast<CharacterLiteral>(expr)) {
        return "'" + string{static_cast<char>(ref->getValue())} +"'";
    } else if (auto ref = dyn_cast<MemberExpr>(expr)) {
        auto parent = dyn_cast<DeclRefExpr>(ref->getBase());
        string ret;
        if (parent) {
            ret += parent->getFoundDecl()->getNameAsString();
        } else {
            ret += "UNKNOWN";
        }
        ret += ".";
        ret += ref->getMemberDecl()->getNameAsString();
        return ret;
    } else if (auto ref = dyn_cast<FloatingLiteral>(expr)) {
        stringstream ss;
        raw_os_ostream os{ss};
        ref->getValue().print(os);
        return ss.str();
    }
    return "Error: UNKNOWN";
}

bool RexCondAtom::equivalent(RexCond *node) {
    RexCondAtom *other = dynamic_cast<RexCondAtom*>(node);
    if (!other) return false;
    if (auto self = dyn_cast<DeclRefExpr>(expr)) {
        auto otherRef = dyn_cast<DeclRefExpr>(other->expr);
        if (!otherRef) return false;
        return self->getFoundDecl()->getNameAsString() == \
               otherRef->getFoundDecl()->getNameAsString();
    }
    else if (auto self = dyn_cast<IntegerLiteral>(expr)) {
        auto otherRef = dyn_cast<IntegerLiteral>(other->expr);
        if (!otherRef) return false;
        return self->getValue() == otherRef->getValue();
    }
    else if (auto self = dyn_cast<CharacterLiteral>(expr)) {
        auto otherRef = dyn_cast<CharacterLiteral>(other->expr);
        if (!otherRef) return false;
        return self->getValue() == otherRef->getValue();
    }
    else if (dyn_cast<MemberExpr>(expr)) {
        auto otherRef = dyn_cast<MemberExpr>(other->expr);
        if (!otherRef) return false;
        return to_string() == other->to_string();
    }
    else if (auto self = dyn_cast<FloatingLiteral>(expr)) {
        auto otherRef = dyn_cast<FloatingLiteral>(other->expr);
        if (!otherRef) return false;
        return self->getValue().bitwiseIsEqual(otherRef->getValue());
    }
    return false;
}
string RexCondAtom::getTopID() {
    return "ATOM" + std::to_string(pUID);
}


bool RexCondAtom::containsCfgVar() {
    DeclRefExpr *ref = dyn_cast<DeclRefExpr>(expr);
    if (!ref) return false;
    VarDecl* v = dyn_cast<VarDecl>(ref->getDecl());
    if (!v)  return false;
    QualType t = v->getType();
    const clang::Type * tp = t.getTypePtr();
    if (!tp) return false;
    bool isCfg = vOpts.allVariables() || vOpts.extractAll;

    // For some reason using the regex stored in vOpts constructed
    // in CondScope's constructor didn't work, so have to create a new
    // regex object each time.
    regex r{vOpts.nameMatch};

    isCfg = isCfg ||
            ((v->hasGlobalStorage() || !vOpts.globalsOnly)
            && \
            (!v->hasGlobalStorage() || !vOpts.nonglobalsOnly)
            && \
            (!vOpts.typeSpecific() ||
            ((tp->isEnumeralType() && vOpts.enums) ||
             (tp->isBooleanType() && vOpts.bools) || \
             (tp->isCharType() && vOpts.chars) ||
             (tp->isIntegerType() && vOpts.ints)))
            && \
            ((vOpts.nameMatch == "") ||
             (regex_search(to_string(), r))));
    return isCfg;

}

RexCond *RexCondAtom::copy() {
    return new RexCondAtom(expr, vOpts);
}

string RexTrue::to_string() {
    return "true";
}

string RexTrue::getTopID() {
    return "TRUE" + std::to_string(pUID);
}

RexCond *RexTrue::copy() {
    return new RexTrue;
}

bool RexTrue::containsCfgVar() {
    return false;
}

bool RexTrue::equivalent(RexCond *other) {
    return dynamic_cast<RexTrue*>(other);
}

RexCondParen::RexCondParen(RexCond *c) : child{c} {}

string RexCondParen::to_string() {
    return "(" + child->to_string() + ")";
}

string RexCondParen::getTopID() {
    return child->getTopID();
}

RexCond *RexCondParen::copy() {
    return new RexCondParen(child->copy());
}

bool RexCondParen::containsCfgVar() {
    return child->containsCfgVar();
}

RexCondParen::~RexCondParen() { delete child; }

bool RexCondParen::equivalent(RexCond *node) {
    RexCondParen *other = dynamic_cast<RexCondParen*>(node);
    if (!other) return false;
    return child->equivalent(other->child);
}


CondScope::CondScope(clang::ASTContext *context, VariabilityOptions vOpts) : vOpts{vOpts}, context{context} {}

/**
 * Takes a Expr* which is part of a condition and makes the corresponding
 * rex cond.
 * @param stmt The conditional statement.
 * @return Heap allocated RexCond* if we care about it, otherwise nullptr
 */
RexCond *CondScope::makeRexCond(Expr *cond) {
    //Expr *original = cond;
    cond = cond->IgnoreImplicit()->IgnoreParenImpCasts();
    if (auto binOp = dyn_cast<BinaryOperator>(cond)) {
        string opCode = static_cast<string>(binOp->getOpcodeStr());
        RexCond *lhs = makeRexCond(binOp->getLHS());
        RexCond *rhs = makeRexCond(binOp->getRHS());
        if (!lhs || !rhs) {
            delete lhs;
            delete rhs;
            return nullptr;
        }
        return RexBinaryOp::makeBinaryOp(opCode, lhs, rhs);
    } else if (auto unOp = dyn_cast<UnaryOperator>(cond)) {
        string opCode = static_cast<string>(UnaryOperator::getOpcodeStr(unOp->getOpcode()));
        RexCond *child = makeRexCond(unOp->getSubExpr());
        if (!child) return nullptr;
        return RexUnaryOp::makeUnaryOp(opCode, child);
    } else if (auto ref = dyn_cast<ParenExpr>(cond)) {
        RexCond *child = makeRexCond(ref->getSubExpr());
        if (!child) return nullptr;
        return new RexCondParen(child);
    } else if (auto ref = dyn_cast<ArraySubscriptExpr>(cond)) {
        // For now only care about which array is read, not the index
        return makeRexCond(ref->getBase());
    } else if (auto ref = dyn_cast<DeclRefExpr>(cond)) {
        return new RexCondAtom(ref, vOpts);
    } else if (auto ref = dyn_cast<AtomicExpr>(cond)) {
        return new RexCondAtom(ref, vOpts);
    } else if (auto ref = dyn_cast<IntegerLiteral>(cond)) {
        return new RexCondAtom(ref, vOpts);
    } else if (auto ref = dyn_cast<CastExpr>(cond)) {
        return makeRexCond(ref->getSubExpr());
    } else if (auto ref = dyn_cast<CharacterLiteral>(cond)) {
        return new RexCondAtom(ref, vOpts);
    } else if (auto ref = dyn_cast<MemberExpr>(cond)) {
        return new RexCondAtom(ref, vOpts);
    } else if (auto ref = dyn_cast<CastExpr>(cond)) {
        return makeRexCond(ref->getSubExprAsWritten());
    } else if (auto ref = dyn_cast<ConstantExpr>(cond)) {
        return makeRexCond(ref->getSubExpr());
    } else if (auto ref = dyn_cast<CallExpr>(cond)) {
        // cerr << "Found a callRef" << endl;
        // Probably not the right thing to do. WOrk on this
        vector<RexCond*> args;
        for (auto it = ref->arg_begin(); it != ref->arg_end(); ++it) {
            args.emplace_back(makeRexCond(*it));
        }
        return makeFnCall(ref, args, vOpts);
    /*} else if (auto ref = dyn_cast<OpaqueValueExpr>(cond)) {
        cerr << "Found opaque Vlaue expr" << endl;
        raw_os_ostream os{cerr};
        ref->getSourceExpr()->dump(os);
        cond->dump(os);
        ref->getLocation().print(os, context->getSourceManager());
        cerr << "Original Location" << endl;
        original->getExprLoc().print(os, context->getSourceManager());
        return nullptr;*/
    } else if (auto ref = dyn_cast<FloatingLiteral>(cond)) {
        return new RexCondAtom(ref, vOpts);
    } else if (auto ref = dyn_cast<ParenExpr>(cond)) {
        return makeRexCond(ref->getSubExpr());
    } else if (auto ref = dyn_cast<ConditionalOperator>(cond)) {
        return new RexTernary{makeRexCond(ref->getCond()), makeRexCond(ref->getTrueExpr()), makeRexCond(ref->getFalseExpr()), vOpts};
    } else if (auto ref = dyn_cast<CStyleCastExpr>(cond)) {
        string castKind{ref->getCastKindName()};
        return makeUnaryOp("(" + castKind +")", makeRexCond(ref->getSubExpr()));

    }
    // Maybe  ImplicitCastExpr
    /*
    cerr << endl << "Didn't understand this cond: " << endl;
    raw_os_ostream os{cerr};
    cond->dump(os);
    os.flush();
    cerr << endl << "Location of not understood: " << endl;
    original->getExprLoc().print(os, context->getSourceManager());
    os.flush();*/
    return nullptr;
}

Expr *getCond(Stmt *s) {
    if (IfStmt* cStmt = dyn_cast<IfStmt>(s)) {
        return cStmt->getCond();
    }
    else if (WhileStmt* cStmt = dyn_cast<WhileStmt>(s)) {
        return cStmt->getCond();
    }
    else if (ForStmt* cStmt = dyn_cast<ForStmt>(s)) {
        return cStmt->getCond();
    }
    else if (DoStmt* cStmt = dyn_cast<DoStmt>(s)) {
        return cStmt->getCond();
    }
    return nullptr;
}

/**
 * Takes a Stmt* which should enforce a logical condition on code.
 * @param stmt The conditional statement.
 * @return void
 */
void CondScope::push(Stmt *s) {
    if (!s) return;
    Expr *cond = getCond(s);
    RexCond *rCond = nullptr;
    if (cond) {
        rCond = makeRexCond(cond->IgnoreImplicit()->IgnoreParenImpCasts());
    }
    if (auto cStmt = dyn_cast<CaseStmt>(s)) {
        RexCond *caseVal = makeRexCond(cStmt->getLHS());
        rCond = RexBinaryOp::makeBinaryOp("==", makeRexCond(switchVar.back()->getCond()), caseVal);
    }
    if (!rCond) {
        rCond = new RexTrue;
    } else {
        for (auto &it : condStack) {
            if (rCond->equivalent(it)) {
                delete rCond;
                rCond = new RexTrue;
                break;
            }
        }
    }
    condStack.emplace_back(rCond);
}

void CondScope::push(SwitchStmt* s) {
    if (!s) return;
    // switch.getCond() is the actual switch var
    switchVar.emplace_back(s);
    caseStack.emplace_back(vector<RexCond*>());
}


void CondScope::push(CaseStmt* s) {
    if (!s) return;
    RexCond *caseVal = makeRexCond(s->getLHS());
    if (!caseVal) {
        cout << s->getLHS() << " type was not found" << endl;
        cout << "type is: " << s->getLHS()->getStmtClassName() << endl;
        if (auto ref = dyn_cast<CastExpr>(s->getLHS())) {
            cout << ref->getSubExpr()->getStmtClassName() << endl;
        }
    }
    RexCond *caseEq = RexBinaryOp::makeBinaryOp("==", makeRexCond(switchVar.back()->getCond()), caseVal);
    caseStack.back().emplace_back(caseEq);
}

void CondScope::push(IfStmt* s) {
    if (!s) return;
    RexCond* p = makeRexCond(s->getCond());
    ifStack.emplace_back(p ? p : new RexTrue);
}

void CondScope::negate_top(IfStmt* s) {
    if (!s) return;
    ifStack.back() = RexUnaryOp::makeUnaryOp("!", ifStack.back());
}

void CondScope::pop(SwitchStmt* s) {
    if (!s) return;
    switchVar.pop_back();
    for (auto cond : caseStack.back()) delete cond;
    caseStack.pop_back();
}

void CondScope::pop(clang::IfStmt* s) {
    if (!s) return;
    delete ifStack.back();
    ifStack.pop_back();
}

void CondScope::pop(Stmt* s) {
    if (!s) return;
    pop();
}

void CondScope::pop(CaseStmt* s) {
    if (!s) return;
    delete caseStack.back().back();
    caseStack.back().pop_back();
}

void CondScope::pop() {
    delete condStack.back();
    condStack.pop_back();
}

CondScope::~CondScope() {
    for (auto &it : condStack) {
        delete it;
    }
}

RexCond *CondScope::disjunction(std::vector<RexCond*> v) {
    if (v.size() == 0) return nullptr;
    if (v.size() == 1) return v.at(0)->copy();
    auto it = v.begin();
    RexCond *dj = RexBinaryOp::makeBinaryOp("||", (*it)->copy(), (*(it + 1))->copy());
    for (it += 2; it != v.end(); ++it) {
        dj = RexBinaryOp::makeBinaryOp("||", dj, (*it)->copy());
    }
    return dj;
}

/**
 * Creates a RexCond that is the conjunction of the current stack.
 * @return a heap allocated RexCond that represents the current
 * conjunction of the entire stack. Caller now owns that memory.
 */
RexCond *CondScope::conjunction() {
    vector<RexCond*> total;
    total.insert(total.end(), condStack.begin(), condStack.end());
    total.insert(total.end(), ifStack.begin(), ifStack.end());
    vector<RexCond*> cases;
    for (auto vec : caseStack) {
        RexCond * dj = disjunction(vec);
        if (dj) {
            cases.emplace_back(dj);
            total.emplace_back(dj);
        }
    }
    if (!total.size()) return nullptr;
    auto it = total.begin();
    while (it != total.end() && !((*it)->containsCfgVar())) {
        ++it;
    }
    if (it == total.end()) {
        for (auto ptr : cases) delete ptr;
        return nullptr;
    }
    RexCond *ret = (*it)->copy();
    ++it;
    for (;it != total.end(); ++it) {
        // Don't bother concatenating a bunch of trues.
        if (!((*it)->containsCfgVar())) continue;
        ret = RexBinaryOp::makeBinaryOp("&&", ret, (*it)->copy());
    }
    for (auto ptr : cases) delete ptr;
    return ret;
}



RexFnCall::RexFnCall(clang::CallExpr *expr, vector<RexCond *> args, const VariabilityOptions &vOpts) : args{args}, vOpts{vOpts} {
    // Doing stuff out here for now maybe need to move in to these ifs.
    if (auto ref = dyn_cast<CXXOperatorCallExpr>(expr)) {
        // TODO
        (void) ref;
    } else if (auto ref = dyn_cast<CXXMemberCallExpr>(expr)) {
        // Is it a member call
        CXXMethodDecl *decl = ref->getMethodDecl();
        if (!decl) {
            cerr << "Decl is null" << endl;
            raw_os_ostream os{cerr};
            expr->dump(os);
        }
        Expr *obj = ref->getImplicitObjectArgument()->IgnoreImplicit();
        raw_os_ostream os{cerr};
        obj->dump(os);
        //cerr << "Method call: " << obj->getFoundDecl()->getNameAsString();
        //cerr << "." << decl->getNameInfo().getAsString() << endl;
    } else {
        // Might just want to do this ffor all cases if we don't care about the other stuff.
        FunctionDecl *decl = expr->getDirectCallee();
        if (!decl) {
            fnName = "UNKNOWN";
        } else {
            fnName = decl->getNameInfo().getName().getAsString();
        }
    }
}

RexFnCall::RexFnCall(string fnName, vector<RexCond *> args, const VariabilityOptions &vOpts) : args{args}, vOpts{vOpts}, fnName{fnName} {}

RexFnCall::~RexFnCall() {
    for (auto &p : args) delete p;
}

string RexFnCall::to_string() {
    string ret = fnName + "(";
    auto it = args.begin();
    string prefix = "";
    while (it != args.end()) {
        ret += prefix;
        ret += (*it)->to_string();
        ++it;
        prefix = ",";
    }
    ret += ")";
    return ret;
}

RexCond *RexFnCall::copy() {
    std::vector<RexCond *> argCopy;
    for (auto &p : args) {
        argCopy.emplace_back(p->copy());
    }
    return new RexFnCall(fnName, argCopy, vOpts);
}

std::string RexFnCall::getTopID() {
    return "FNCALL" + std::to_string(pUID);
}

bool RexFnCall::containsCfgVar() {
    bool b = vOpts.extractAll;
    for (auto &p : args) {
        b = b || p->containsCfgVar();
    }
    return b;
}

bool RexFnCall::equivalent(RexCond *c) {
    RexFnCall *p = dynamic_cast<RexFnCall*>(c);
    if (!p) return false;
    if (fnName != p->fnName) return false;
    if (args.size() != p->args.size()) return false;
    for (size_t i = 0; i < args.size(); ++i) {
        if (!args[i]->equivalent(p->args[i])) return false;
    }
    return true;
}

RexTernary::RexTernary(RexCond *ifC, RexCond *thenC, RexCond *elseC, const VariabilityOptions &vOpts) : ifC{ifC}, thenC{thenC}, elseC{elseC}, vOpts{vOpts} {}

RexTernary::~RexTernary() {
    delete ifC; delete elseC; delete thenC;
}

std::string RexTernary::to_string() {
    // Currently implemented to just produce the then, because for now that's quickest and cleanest solution for readable conds.
    // In the future when conds are actually printing out, having an internal Ternary should actually change the parents behaviour but
    // thats for another time.
    return thenC->to_string();
}

RexCond *RexTernary::copy() {
    return new RexTernary{ifC->copy(), thenC->copy(), elseC->copy(), vOpts};
}

std::string RexTernary::getTopID() {
    return "TERNARY" + std::to_string(pUID);
}

bool RexTernary::containsCfgVar() {
    return ifC->containsCfgVar() || elseC->containsCfgVar() || thenC->containsCfgVar();
}

bool RexTernary::equivalent(RexCond *c) {
    RexTernary *other = dynamic_cast<RexTernary*>(c);
    if (!other) return false;
    return ifC->equivalent(other->ifC) && thenC->equivalent(other->thenC) && elseC->equivalent(other->elseC);
}
