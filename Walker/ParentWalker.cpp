/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParentWalker.cpp
//
// Created By: Bryan J Muscedere
// Date: 06/07/17.
//
// Modified By: Davood Anbarnam
//
// Contains the majority of the logic for adding
// nodes and relations to the graph. Many of these
// are helper functions like ID generation or
// class resolution.
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

#include "ParentWalker.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <tuple>

#include <map> //NEW
#include <string> //NEW

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <llvm/Support/Path.h>
#include <llvm/ADT/SmallString.h>

#include "ROSConsumer.h"
#include "RexID.h"
#include "../Driver/IgnoreMatcher.h"

using namespace llvm;
using namespace clang;
using namespace clang::tooling;
using namespace std;
namespace fs = boost::filesystem;

namespace { // helper functions

const clang::Expr* simplifyExpr(const Expr* expr) {
    const Expr *simplifiedExpr;
    do {
        simplifiedExpr = expr;
        expr = expr->IgnoreParenCasts();
        expr = expr->IgnoreImpCasts();
        expr = expr->IgnoreImplicit();
        expr = expr->IgnoreCasts();
        expr = expr->IgnoreParens();
        expr = expr->IgnoreConversionOperator();
        expr = expr->IgnoreParenImpCasts();
        expr = expr->IgnoreParenLValueCasts();
    } while (simplifiedExpr != expr);
    return simplifiedExpr;
}

std::set<std::string> mergeSets(const std::set<std::string>& s1, const std::set<std::string>& s2) {
    std::set<std::string> ret = s1;
    ret.insert(s2.begin(), s2.end());
    return ret;
}

// We don't currently support indirect function calls (i.e., function pointer, lambdas, ...), nor variadics
const FunctionDecl* getNonVariadicDirectCallee(const CallExpr* call) {
    const FunctionDecl *directCallee = call->getDirectCallee();
    if (!directCallee)
        return nullptr;
    if (directCallee->isVariadic())
        return nullptr;
    return directCallee;
}

} // helpers


////////////// Dataflow extraction ////////////////////


/////////////////
// Stmt
// - A statement can create a dataflow, but does not return anything
// `addWriteRelationships` records the dataflow
//

// Delegate to more specific type overloads
void ParentWalker::addWriteRelationships(const Stmt* stmt, const FunctionDecl* curFunc) {
    curDflowStmt = stmt;
    if (auto cleanups = dyn_cast<ExprWithCleanups>(curDflowStmt)) {
        // `ExprWithCleanups` are unwrapped by the CFG -- the CFG only includes the `getSubExpr()` result.
        // Using BuildOptions.AddTemporaryDtors adds the corresponding temporary destructor, but still doesn't include
        // the `ExprWithCleanups` parent expression
        curDflowStmt = cleanups->getSubExpr();
    }

    if (auto expr = dyn_cast<Expr>(stmt)) {
        addWriteRelationships(expr, curFunc);
    } else {
        if (auto retstmt = dyn_cast<ReturnStmt>(stmt)) {
            addWriteRelationships(retstmt, curFunc);
        } else if (auto declstmt = dyn_cast<DeclStmt>(stmt)) {
            addWriteRelationships(declstmt, curFunc);
        } else if (auto rangefor = dyn_cast<CXXForRangeStmt>(stmt)) {
            addWriteRelationships(rangefor, curFunc);
        }
    }
    // cerr << "addWriteRelationships Stmt: unexpected statement type." << endl;
}

////////////
// ReturnStmt
//
// int plusOne(int x) {
//   return x + 1; // x writes to the return value of `plusOne`
//                // Essentially equivalent to a = x + 1; a is thus the LS and the source line.
// }
//
void ParentWalker::addWriteRelationships(const ReturnStmt* stmt, const FunctionDecl* curFunc) {
    auto retval = stmt->getRetValue();
    if (!retval)
        return; // the return statement is just like this one!

    std::set<std::string> rValues = extractRValues(retval);
    std::string retID = generateRetID(curFunc);
    recordFunctionVarUsage(curFunc, {retID}, rValues);
}

////////////
// DeclStmt
//
// void foo(int x, int y) {
//   int a = x, b = y; // x writes to a, y writes to b
// }
//
void ParentWalker::addWriteRelationships(const DeclStmt* stmt, const FunctionDecl* curFunc) {

    for (auto it = stmt->decl_begin(); it != stmt->decl_end(); it++) {
        auto decl = dyn_cast<VarDecl>(*it);
        if (!decl) // does not declare a variable
            continue;

        auto initializer = decl->getInit();
        if (!initializer)
            continue;

        std::set<std::string> rValues = extractRValues(initializer);

        std::string declID = generateID(decl);
        recordFunctionVarUsage(curFunc, {declID}, rValues);
    }
}

////////
// CXXForRangeStmt
// for (auto v: var) {} // var writes to v
//

void ParentWalker::addWriteRelationships(const CXXForRangeStmt* rangefor, const FunctionDecl* curFunc) {
    const VarDecl* loopvar = rangefor->getLoopVariable();
    const Expr* rangeinit = rangefor->getRangeInit();

    recordFunctionVarUsage(curFunc, {generateID(loopvar)}, extractRValues(rangeinit));
}


/////////////////
// Expr
// - An expression can create a dataflow, and can also return values
// `addWriteRelationships` records the dataflow
// `extractLValue` returns the variables being written to
// `extractRValue` returns the variables doing the writing
//

// Delegate to more specific type overloads
void ParentWalker::addWriteRelationships(const Expr* expr, const FunctionDecl* curFunc) {

    if (auto* binop = dyn_cast<BinaryOperator>(expr)) {
        addWriteRelationships(binop, curFunc);
    } else if (auto* unop = dyn_cast<UnaryOperator>(expr)) {
        addWriteRelationships(unop, curFunc);
    } else if (auto* call = dyn_cast<CallExpr>(expr)) {
        addWriteRelationships(call, curFunc);
    } else if (auto* ctor = dyn_cast<CXXConstructExpr>(expr)) {
        addWriteRelationships(ctor, curFunc);
    } else {
        // cerr << "add write relationships Expr: unexpected expression type" << endl;
    }
}

// Delegate to more specific type overloads
std::set<std::string> ParentWalker::extractRValues(const Expr* expr) {
    expr = simplifyExpr(expr);


    if (auto* binop = dyn_cast<BinaryOperator>(expr)) {
        return extractRValues(binop);
    } else if (auto* unop = dyn_cast<UnaryOperator>(expr)) {
        return extractRValues(unop);
    } else if (auto *dref = dyn_cast<DeclRefExpr>(expr)) {
        return extractRValues(dref);
    } else if (auto *member = dyn_cast<MemberExpr>(expr)) {
        return extractRValues(member);
    } else if (auto *arr = dyn_cast<ArraySubscriptExpr>(expr)) {
        return extractRValues(arr);
    } else if (auto *ctor = dyn_cast<CXXConstructExpr>(expr)) {
        return extractRValues(ctor);
    } else if (auto *callexpr = dyn_cast<CallExpr>(expr)) {
        return extractRValues(callexpr);
    }

    if (isa<IntegerLiteral>(expr) || isa<CharacterLiteral>(expr) ||
        isa<FloatingLiteral>(expr) || isa<ImaginaryLiteral>(expr) ||
        isa<UserDefinedLiteral>(expr) || isa<GNUNullExpr>(expr) || isa<CXXThisExpr>(expr)) {
        return {};
    }

    // cerr << "RVALUE: UNEXPECTED EXPRESSION TYPE" << endl;
    return {};
}

// Delegate to more specific type overloads
std::set<std::string> ParentWalker::extractLValues(const Expr* expr) {
    expr = simplifyExpr(expr);

    if (auto* binop = dyn_cast<BinaryOperator>(expr)) {
        return extractLValues(binop);
    } else if (auto* unop = dyn_cast<UnaryOperator>(expr)) {
        return extractLValues(unop);
    } else if (auto *dref = dyn_cast<DeclRefExpr>(expr)) {
        return extractLValues(dref);
    } else if (auto *member = dyn_cast<MemberExpr>(expr)) {
        return extractLValues(member);
    } else if (auto *arr = dyn_cast<ArraySubscriptExpr>(expr)) {
        return extractLValues(arr);
    } else if (auto *callexpr = dyn_cast<CallExpr>(expr)) {
        return extractLValues(callexpr);
    }

    if (isa<IntegerLiteral>(expr) || isa<CharacterLiteral>(expr) ||
        isa<FloatingLiteral>(expr) || isa<ImaginaryLiteral>(expr) ||
        isa<UserDefinedLiteral>(expr) || isa<GNUNullExpr>(expr) || isa<CXXThisExpr>(expr)) {
        return {};
    }

    // cerr << "LVALUE: UNEXPECTED EXPRESSION TYPE" << endl;
    return {};
}


//////////////
// DeclRefExpr
//
std::set<std::string> ParentWalker::extractRValues(const DeclRefExpr* declref) {
    return {generateID(declref->getDecl())};
}
std::set<std::string> ParentWalker::extractLValues(const DeclRefExpr* declref) {
    return {generateID(declref->getDecl())};
}

//////////////
// MemberExpr
//
// a.b->c => a
// this->a.b->c => MyClass::a
//
void ParentWalker::createThisVariables(const ValueDecl *valueDecl) {
    if (const VarDecl *varDecl = dyn_cast<VarDecl>(valueDecl)) {
        recordVarDecl(varDecl);
    } else if (const FieldDecl *fieldDecl = dyn_cast<FieldDecl>(valueDecl)) {
        recordFieldDecl(fieldDecl);
    }
}

std::set<std::string> ParentWalker::extractRValues(const MemberExpr* member) {
    auto base = member->getBase();
    if (isa<CXXThisExpr>(base)) {
        createThisVariables(member->getMemberDecl());
        return {generateID(member->getMemberDecl())};
    } else {
        return extractRValues(member->getBase());
    }
}
std::set<std::string> ParentWalker::extractLValues(const MemberExpr* member) {
    auto base = member->getBase();
    if (isa<CXXThisExpr>(base)) {
        createThisVariables(member->getMemberDecl());
        return {generateID(member->getMemberDecl())};
    } else {
        return extractLValues(member->getBase());
    }
}

/////////////////
// CXXConstructExpr
// - can never return an l-value
// - the writing to the instance is covered by DeclStmt dataflow. Here, only need to add writes for the parameters.
//

void ParentWalker::addWriteRelationships(const CXXConstructExpr* ctor, const FunctionDecl* curFunc) {
    int i = 0;

    for (auto arg: ctor->arguments()) {
        recordFunctionVarUsage(curFunc, {generateID(ctor->getConstructor()->getParamDecl(i))}, extractRValues(arg));
        i++;
    }
    

    
}

std::set<std::string> ParentWalker::extractRValues(const CXXConstructExpr* ctor) {
    std::set<std::string> ret = {};
    for (auto arg: ctor->arguments()) {
        ret = mergeSets(ret, extractRValues(arg));
    }
    return ret;
}

/////////////////
// BinaryOperator
//

void ParentWalker::addWriteRelationships(const BinaryOperator* binop, const FunctionDecl* curFunc) {
    addWriteRelationships_BinaryOperatorCommon(binop->getLHS(), binop->getRHS(), binop->getOpcode(), curFunc);
}
std::set<std::string> ParentWalker::extractRValues(const BinaryOperator* binop) {
    return extractRValues_BinaryOperatorCommon(binop->getLHS(), binop->getRHS(), binop->getOpcode());
}
std::set<std::string> ParentWalker::extractLValues(const BinaryOperator* binop) {
    return extractLValues_BinaryOperatorCommon(binop->getLHS(), binop->getOpcode());
}

// Binary operator logic that is shared between the standard `BinaryOperator` as well as overloads
void ParentWalker::addWriteRelationships_BinaryOperatorCommon(const Expr* lhs,
                                                              const Expr* rhs,
                                                              const BinaryOperator::Opcode opcode,
                                                              const FunctionDecl* curFunc)
{
    switch (opcode) {
        case BinaryOperator::Opcode::BO_Assign:
        case BinaryOperator::Opcode::BO_AddAssign:
        case BinaryOperator::Opcode::BO_DivAssign:
        case BinaryOperator::Opcode::BO_MulAssign:
        case BinaryOperator::Opcode::BO_SubAssign:
        case BinaryOperator::Opcode::BO_AndAssign:
        case BinaryOperator::Opcode::BO_OrAssign:
        case BinaryOperator::Opcode::BO_RemAssign:
        case BinaryOperator::Opcode::BO_ShlAssign:
        case BinaryOperator::Opcode::BO_ShrAssign:
        case BinaryOperator::Opcode::BO_XorAssign:
        {
            recordFunctionVarUsage(curFunc, extractLValues(lhs), extractRValues(rhs));
            break;
        }
        case BinaryOperator::Opcode::BO_Add:
        case BinaryOperator::Opcode::BO_Div:
        case BinaryOperator::Opcode::BO_Mul:
        case BinaryOperator::Opcode::BO_Sub:
        case BinaryOperator::Opcode::BO_And:
        case BinaryOperator::Opcode::BO_Or:
        case BinaryOperator::Opcode::BO_Rem:
        case BinaryOperator::Opcode::BO_Shl:
        case BinaryOperator::Opcode::BO_Shr:
        case BinaryOperator::Opcode::BO_Xor:
        case BinaryOperator::Opcode::BO_PtrMemD:
        case BinaryOperator::Opcode::BO_PtrMemI:
            break;
        default:
            // cerr << "Dataflow binary operator common: unexpected operator" << endl;
            break;
    }
}
std::set<std::string> ParentWalker::extractRValues_BinaryOperatorCommon(const Expr* lhs,
                                                                        const Expr* rhs,
                                                                        const BinaryOperator::Opcode opcode)
{
    switch (opcode) {
        // Note, MISRA C++ 2008 6.2.1 forbids assignment from being used in sub-expressions. However,
        // I don't think `foo(a = b)` is covered by this rule...
        // MISRA C 2012 says "The result of an assignment operator *should* not be used" <-- this is an advisory rule.
        // Plus, autonomoose has some logging code which uses the `foo(a = b)` pattern (boost::log::add_file_log).
        // So for now, allow extracting "rvalues" from assignment.
        case BinaryOperator::Opcode::BO_Assign:
        case BinaryOperator::Opcode::BO_AddAssign:
        case BinaryOperator::Opcode::BO_DivAssign:
        case BinaryOperator::Opcode::BO_MulAssign:
        case BinaryOperator::Opcode::BO_SubAssign:
        case BinaryOperator::Opcode::BO_AndAssign:
        case BinaryOperator::Opcode::BO_OrAssign:
        case BinaryOperator::Opcode::BO_RemAssign:
        case BinaryOperator::Opcode::BO_ShlAssign:
        case BinaryOperator::Opcode::BO_ShrAssign:
        case BinaryOperator::Opcode::BO_XorAssign:
            return extractRValues(rhs);
        case BinaryOperator::Opcode::BO_Add:
        case BinaryOperator::Opcode::BO_Div:
        case BinaryOperator::Opcode::BO_Mul:
        case BinaryOperator::Opcode::BO_Sub:
        case BinaryOperator::Opcode::BO_And:
        case BinaryOperator::Opcode::BO_Or:
        case BinaryOperator::Opcode::BO_Rem:
        case BinaryOperator::Opcode::BO_Shl:
        case BinaryOperator::Opcode::BO_Shr:
        case BinaryOperator::Opcode::BO_Xor:
            return mergeSets(extractRValues(lhs), extractRValues(rhs));
        case BinaryOperator::Opcode::BO_PtrMemD:
        case BinaryOperator::Opcode::BO_PtrMemI:
            // cerr << "rvalue binary operator common: unsupported operator" << endl;
            return {};
        default:
            // cerr << "rvalue binary operator common: unexpected operator" << endl;
            return {};
    }
}
std::set<std::string> ParentWalker::extractLValues_BinaryOperatorCommon(const Expr* lhs,
                                                                        const BinaryOperator::Opcode opcode)
{
    switch (opcode) {
        case BinaryOperator::Opcode::BO_AddAssign:
        case BinaryOperator::Opcode::BO_DivAssign:
        case BinaryOperator::Opcode::BO_MulAssign:
        case BinaryOperator::Opcode::BO_SubAssign:
        case BinaryOperator::Opcode::BO_Assign:
        case BinaryOperator::Opcode::BO_AndAssign:
        case BinaryOperator::Opcode::BO_OrAssign:
        case BinaryOperator::Opcode::BO_RemAssign:
        case BinaryOperator::Opcode::BO_ShlAssign:
        case BinaryOperator::Opcode::BO_ShrAssign:
        case BinaryOperator::Opcode::BO_XorAssign:
            return extractLValues(lhs);
        case BinaryOperator::Opcode::BO_PtrMemD:
        case BinaryOperator::Opcode::BO_PtrMemI:
            // cerr << "lvalue binary operator common: unsupported operator" << endl;
            return {};
        default:
            // cerr << "lvalue binary operator common: unexpected operator" << endl;
            return {};
    }
}

/////////////////
// UnaryOperator
//

void ParentWalker::addWriteRelationships(const UnaryOperator* unop, const FunctionDecl* curFunc) {
    addWriteRelationships_UnaryOperatorCommon(unop->getSubExpr(), unop->getOpcode(), curFunc);
}
std::set<std::string> ParentWalker::extractRValues(const UnaryOperator* unop) {
    return extractRValues_UnaryOperatorCommon(unop->getSubExpr());
}
std::set<std::string> ParentWalker::extractLValues(const UnaryOperator* unop) {
    return extractLValues_UnaryOperatorCommon(unop->getSubExpr());
}

// Unary operator logic that is shared between the standard `UnaryOperator` as well as overloads
// @param arg corresopnds to ++(arg)

void ParentWalker::addWriteRelationships_UnaryOperatorCommon(const Expr* arg,
                                                             const UnaryOperator::Opcode opcode,
                                                             const FunctionDecl* curFunc)
{
    switch (opcode) {
        case UnaryOperator::Opcode::UO_PostDec:
        case UnaryOperator::Opcode::UO_PostInc:
        case UnaryOperator::Opcode::UO_PreDec:
        case UnaryOperator::Opcode::UO_PreInc:
            recordFunctionVarUsage(curFunc, extractLValues(arg), {});
            break;
        case UnaryOperator::Opcode::UO_Plus:
        case UnaryOperator::Opcode::UO_Minus:
        case UnaryOperator::Opcode::UO_Deref:
            // operator is read-only
            break;
        default:
            // cerr << "add write relationships unary operator: unexpected operator" << endl;
            break;
    }
}
std::set<std::string> ParentWalker::extractRValues_UnaryOperatorCommon(const Expr* arg) {
    return extractRValues(arg);
}
std::set<std::string> ParentWalker::extractLValues_UnaryOperatorCommon(const Expr* arg) {
    return extractLValues(arg);
}

/////////////////
// ArraySubscriptExpr
// arr[idx] = x;
//
// Note: the `idx` only serves as a pointer to the data. It doesn't actually provide nor receive data.
//

void ParentWalker::addWriteRelationships(const ArraySubscriptExpr* arr, const FunctionDecl* curFunc) {
    addWriteRelationships(arr->getLHS(), curFunc);
}
std::set<std::string> ParentWalker::extractRValues(const ArraySubscriptExpr* arr) {
    return extractRValues(arr->getLHS());
}
std::set<std::string> ParentWalker::extractLValues(const ArraySubscriptExpr* arr) {
    return extractLValues(arr->getLHS());
}

// Common behavior shared by the raw ArraySubscriptExpr type as well as operator[] overloads
// @arrayOb corresopnds to `arrayObj[idx]`

void ParentWalker::addWriteRelationships_SubscriptOperatorCommon(const Expr* arrayObj, const FunctionDecl* curFunc) {
    addWriteRelationships(arrayObj, curFunc);
}
std::set<std::string> ParentWalker::extractRValues_SubscriptOperatorCommon(const Expr* arrayObj) {
    return extractRValues(arrayObj);
}
std::set<std::string> ParentWalker::extractLValues_SubscriptOperatorCommon(const Expr* arrayObj) {
    return extractLValues(arrayObj);
}


///////////////////////////////////////////////////
// CallExpr, CXXMethodCallExpr, CXXOperatorCallExpr
//
// There are 2 categories of calls that fall under the `CallExpr` umbrella that we are interested in:
// - operator overloads
// - member and non-member calls
//    variadics and indirect function calls (function pointers, lambdas, ...) are not currently supported
//     - relevant: https://git.uwaterloo.ca/swag/Rex/-/issues/31
//
//


// Differentiate between function/method calls, and operator overloads
void ParentWalker::addWriteRelationships(const CallExpr* call, const FunctionDecl* curFunc) {
    if (auto opcall = dyn_cast<CXXOperatorCallExpr>(call)) {
        addWriteRelationships_OverloadedOperator(opcall, curFunc);
        return;
    }

    auto decl = getNonVariadicDirectCallee(call);
    if (!decl)
        return; // unsupported type

    for (unsigned i = 0; i < call->getNumArgs(); ++i) {
        if (const auto *methodCall = dyn_cast<clang::CXXMemberCallExpr>(call)) {
            // This definitely generates false positives, especially for methods like `vector::at(i)`, but it's
            // how Rex has worked even prior to the re-write of the read/write algo.
            recordFunctionVarUsage(curFunc,
                                   extractLValues(methodCall->getImplicitObjectArgument()),
                                   extractRValues(call->getArg(i)));
        }


        // attempts to get the definition of funcDecl
        // some declarations does not have parameter names
        if (const FunctionDecl *tempFunc = decl->getDefinition()) {
            recordParameterWrite(tempFunc, generateID(tempFunc->getParamDecl(i)), extractRValues(call->getArg(i)));
        }
       
        if (decl->getCanonicalDecl() == nullptr || decl->param_size() == 0 
            || i > decl->getNumParams()) {
            continue;
        }
        

        recordParameterWrite(decl, generateID(decl->getParamDecl(i)), extractRValues(call->getArg(i)));
        
    }
}
std::set<std::string> ParentWalker::extractRValues(const CallExpr* call) {

    if (auto opcall = dyn_cast<CXXOperatorCallExpr>(call)) {
        return extractRValues_OverloadedOperator(opcall);
    }

    auto decl = getNonVariadicDirectCallee(call);
    if (!decl)
        return {}; // unsupported type

    set<string> ret = {};
    // To catch all cases, assume that the instance also flows into the return.
    if (const auto *methodCall = dyn_cast<clang::CXXMemberCallExpr>(call)) {
        ret = mergeSets(ret, extractRValues(methodCall->getImplicitObjectArgument()));
    }
    if (isInSystemHeader(decl)) {
        // We don't visit the definitions of library functions, so the function's "return" node will never get written to.
        // So for library functions, be optimistic and assume all arguments flow into the return value.
        // This is especially useful for math functions like `sin`, which are used many places in Autonomoose.
        // This will create false positives for member functions (i.e., auto x = vector::at(i) --> i writes to x), so it
        // may potentially be removed for members in the future.
        for (const Expr *arg : call->arguments()) {
            ret = mergeSets(ret, extractRValues(arg));
        }
    } else {
        ret.insert(generateRetID(decl));
    }
    
    return ret;
}
std::set<std::string> ParentWalker::extractLValues(const CallExpr* call) {
    if (auto opcall = dyn_cast<CXXOperatorCallExpr>(call)) {
        return extractLValues_OverloadedOperator(opcall);
    }
    auto decl = getNonVariadicDirectCallee(call);
    if (!decl)
        return {}; // unsupported function type

    if (const auto *methodCall = dyn_cast<clang::CXXMemberCallExpr>(call)) {
        // Assume the method returns a mutable reference to the instance
        // This allows for expressions such as `mem.getVec().push_back(x)` --> `x -varWrite-> mem`
        return extractLValues(methodCall->getImplicitObjectArgument());
    } else {
        // Don't return the non-member function's "ret" node because in order for it to be an lvalue,
        // it would have to be a non-const reference/ptr, but we don't support those yet
        // (in the member method case above, the reference is assumed to be to the instance, but here the referee is unknown)
        return {};
    }
}

/////////////////
// CXXOperatorCallExpr
// - Represents an overloaded operator call. Re-route to the appropriate operator's logic
//

void ParentWalker::addWriteRelationships_OverloadedOperator(const CXXOperatorCallExpr* opcall,
                                                            const FunctionDecl* curFunc)
{
    if (opcall->isAssignmentOp()) {
        addWriteRelationships_BinaryOperatorCommon(opcall->getArg(0), opcall->getArg(1),
                                                   BinaryOperator::getOverloadedOpcode(opcall->getOperator()), curFunc);
    } else if (opcall->getOperator() == OO_Star && !opcall->isInfixBinaryOp()) {
        addWriteRelationships_UnaryOperatorCommon(opcall->getArg(0),
                                                  UnaryOperator::getOverloadedOpcode(opcall->getOperator(),
                                                                                     false /* =postfix */),
                                                  curFunc);
    }

    // cerr << "add write relationships overloaded operator: unexpected operator." << endl;
}
std::set<std::string> ParentWalker::extractRValues_OverloadedOperator(const clang::CXXOperatorCallExpr* opcall) {
    if (opcall->isAssignmentOp()) {
        return extractRValues_BinaryOperatorCommon(opcall->getArg(0),
                                                   opcall->getArg(1),
                                                   BinaryOperator::getOverloadedOpcode(opcall->getOperator()));
    } else if (opcall->getOperator() == OO_Star && !opcall->isInfixBinaryOp()) {
        return extractRValues_UnaryOperatorCommon(opcall->getArg(0));
    } if (opcall->getOperator() == OO_Arrow) {
        // `operator->()` just has one argument. It's just a wrapper for the LHS of a MemberExpr
        return extractRValues(opcall->getArg(0));
    } if (opcall->getOperator() == OO_Subscript) { // operator[]
        return extractRValues_SubscriptOperatorCommon(opcall->getArg(0));
    }

    // cerr << "rvalues overloaded operator: unexpected operator" << endl;
    return {};
}
std::set<std::string> ParentWalker::extractLValues_OverloadedOperator(const clang::CXXOperatorCallExpr* opcall) {
    if (opcall->isAssignmentOp()) {
        return extractLValues_BinaryOperatorCommon(opcall->getArg(0), BinaryOperator::getOverloadedOpcode(opcall->getOperator()));
    } else if (opcall->getOperator() == OO_Star && !opcall->isInfixBinaryOp()) {
        return extractLValues_UnaryOperatorCommon(opcall->getArg(0));
    } if (opcall->getOperator() == OO_Arrow) {
        // `operator->()` just has one argument. It's just a wrapper for the LHS of a MemberExpr
        return extractLValues(opcall->getArg(0));
    } if (opcall->getOperator() == OO_Subscript) { // operator[]
        return extractLValues_SubscriptOperatorCommon(opcall->getArg(0));
    }

    // cerr << "lvalue overloaded operator: unexpected operator" << endl;
    return {};
}



/**
 * Constructor for the parent walker class.
 * @param Context The AST context.
 */
ParentWalker::ParentWalker(const WalkerConfig &config, ASTContext *Context):
    graph{config.graph},
    ignored{config.ignored},
    featureName{config.featureName},
    langFeats{config.lFeats},
    buildCFG{config.buildCFG},
    ROSFeats{config.ROSFeats},
    callbackFuncRegex{regex{config.geOpts.callbackFuncRegex}},
    Context(Context) {}


/**
 * Destructor.
 */
ParentWalker::~ParentWalker() {}


/**
 * Records a basic struct/union/class declaration in the TA model.
 * @param decl The declaration to add.
 */
void ParentWalker::recordCXXRecordDecl(const CXXRecordDecl *decl, RexNode::NodeType type) {
    if ((langFeats.cClass && type == RexNode::CLASS) ||
        (langFeats.cStruct && type == RexNode::STRUCT) ||
        (langFeats.cUnion && type == RexNode::UNION))
    {

        // Generates some fields.
        string id = generateID(decl);
        string name = generateName(decl);

        // Creates the node.
        if (!graph.hasNode(id)) {
            RexNode *node = new RexNode(id, type, isInMainFile(decl));
            node->addSingleAttribute(TASchemeAttribute::LABEL, name);
            // Resolves the filename.
            recordNamedDeclLocation(decl, node);
            addNodeToGraph(node);
        }


        // Get the parent.
        addParentRelationship(decl, id);
    }
}

/**
 * Records a basic enum declaration to the TA model.
 * @param decl The declaration to add.
 */
void ParentWalker::recordEnumDecl(const EnumDecl* decl){
    if (langFeats.cEnum){
        //Generates some fields.
        string id = generateID(decl);
        string name = generateName(decl);

        //Creates the node.
        RexNode* node = new RexNode(id, RexNode::ENUM, isInMainFile(decl));
        node->addSingleAttribute(TASchemeAttribute::LABEL, name);
        node->addSingleAttribute(TASchemeAttribute::IS_CONTROL_FLOW, "0");
        recordNamedDeclLocation(decl, node);
        addNodeToGraph(node);

        //Get the parent.
        addParentRelationship(decl, id);
    }
}

/**
 * Records a basic field declaration to the TA model.
 * @param decl The declaration to add.
 */
void ParentWalker::recordFieldDecl(const FieldDecl *decl) {
    if (langFeats.cVariable)
    {
        // Generates some fields.
        string id = generateID(decl);
        string name = generateName(decl);

        // Creates the node.
        RexNode *node = specializedVariableNode(decl, isInMainFile(decl));
        node->addSingleAttribute(TASchemeAttribute::LABEL, name);
        node->addSingleAttribute(TASchemeAttribute::IS_CONTROL_FLOW, "0");
        recordNamedDeclLocation(decl, node);
        addNodeToGraph(node);
        // Get the parent.
        addParentRelationship(decl, id);
    }
}

/**
 * Records a basic variable declaration to the TA model.
 * @param decl The declaration to add.
 */
void ParentWalker::recordVarDecl(const VarDecl *decl) {
    if (langFeats.cVariable){
        // Generates some fields.
        string id = generateID(decl);
        string name = generateName(decl);
        // Creates the node.
        RexNode *node = specializedVariableNode(decl, isInMainFile(decl));
        node->addSingleAttribute(TASchemeAttribute::LABEL, name);
        node->addSingleAttribute(TASchemeAttribute::IS_CONTROL_FLOW, "0");
        if (isa<ParmVarDecl>(decl)) {
            node->addSingleAttribute(TASchemeAttribute::IS_PARAM, "1");
            recordNamedDeclLocation(decl, node);
            //node->addSingleAttribute(TASchemeAttribute::FILENAME_DEFINITION, 
            //    generateFileName(decl->getDefinition()));
        } else {
            node->addSingleAttribute(TASchemeAttribute::IS_PARAM, "0");
            recordNamedDeclLocation(decl, node);
        }
        
        addNodeToGraph(node);
        // Get the parent.
        addParentRelationship(decl, id);

        CXXRecordDecl* classType = decl->getCanonicalDecl()->getType().getTypePtr()->getAsCXXRecordDecl();
        if(classType)
        {
            // Record OBJ relation
            addOrUpdateEdge(id, generateID(classType), RexEdge::OBJ);
        }
    }
}

/**
 * Records a basic function declaration to the TA model.
 * @param decl The declaration to add.
 */
void ParentWalker::recordFunctionDecl(const FunctionDecl *decl) {
    if (langFeats.cFunction){
        // Generatesthe fields.
        string id = generateID(decl);
        string name = generateName(decl);        

        // Creates the node.
        RexNode *node = new RexNode(id, RexNode::FUNCTION, isInMainFile(decl));
        node->addSingleAttribute(TASchemeAttribute::LABEL, name);

        // Record whether the function is a callback.
        string isCallback = regex_match(decl->getNameAsString(), callbackFuncRegex) ? "1" : "0";
        node->addSingleAttribute(TASchemeAttribute::ROS_IS_CALLBACK, isCallback);

        // Don't add filename if we cannot determine where body is
        recordNamedDeclLocation(decl, node, decl->doesThisDeclarationHaveABody());
        
        
        addNodeToGraph(node);

        // Get the parent.
        addParentRelationship(decl, id);

    }
}

void ParentWalker::recordParamPassing(const CallExpr *expr) {
    // The call f(x) would have the following AST:
    //
    // CallExpr 0x55a881605780 <line:13:3, col:6> 'void'
    // |-ImplicitCastExpr 0x55a881605768 <col:3> 'void (*)(int)'
    // <FunctionToPointerDecay> | `-DeclRefExpr 0x55a881605718 <col:3> 'void
    // (int)' lvalue Function 0x55a8816052f0 'f' 'void (int)'
    // `-ImplicitCastExpr 0x55a8816057b0 <col:5> 'int' <LValueToRValue>
    //     `-DeclRefExpr 0x55a8816056f0 <col:5> 'int' lvalue Var 0x55a8816055e0
    //     'x' 'int'

    const FunctionDecl *directCallee = expr->getDirectCallee();

    if (directCallee == nullptr) {
        // Ignore non-direct function call, i.e. function pointer, lambda, etc.
        return;
    }

    if (directCallee->isVariadic()) {
        // Ignore variadic functions
        return;
    }


    int numParams = directCallee->getNumParams();
    vector<string> argsPassed;
    int i = 0;
    for (const Expr *arg : expr->arguments()) {
        // For CXXOperatorCallExpr (which we don't support anyway), we get
        // exactly 1 argument, even though the number of parameters is zero...
        // This if statement is meant to protect us from that and future proof
        // for any other inconsistent decisions that clang makes in the future.
        if (i >= numParams) {
            break;
        }

        // Ignore any implicit things clang adds on top
        arg = arg->IgnoreImplicit();

        string memObjID;

        // Only parameters that are plain variables are supported
        if (const auto *declref = dyn_cast<DeclRefExpr>(arg)) {
            memObjID = generateID(declref->getDecl());
        } else if (const auto *memRef = dyn_cast<MemberExpr>(arg)) {
            memObjID = generateID(memRef);
        }

        if (!memObjID.empty()) {
            // Get the function parameter corresponding to this argument
            const ParmVarDecl *paramDecl = directCallee->getParamDecl(i);
            // Add an edge that indicates that the argument writes to the parameter
            addEdgeToGraph(memObjID, generateID(paramDecl), RexEdge::VAR_WRITES);
            argsPassed.push_back(memObjID);
        }

        i++;
    }

	if (const auto *methodCall = dyn_cast<clang::CXXMemberCallExpr>(expr)) {
        Expr* obj = methodCall->getImplicitObjectArgument();
        obj = obj->IgnoreCasts();


        string memberFuncID = generateID(directCallee);
        string memberObjID;
        if(const MemberExpr* memberObj = dyn_cast<MemberExpr>(obj))
        {
            memberObjID =  generateID(memberObj);
        }
        else if(const DeclRefExpr* memberObj = dyn_cast<DeclRefExpr>(obj))
        {
            memberObjID =  generateID(memberObj->getDecl());
        }

        if (!memberObjID.empty())
        {
            for(string argPassed : argsPassed)
            {
                addOrUpdateEdge(argPassed, memberObjID, RexEdge::VAR_WRITES);
            }
        }
    }
}



/**
 * Records a basic call expression.
 * @param expr The expression to record.
 */
void ParentWalker::recordCallExpr(const CallExpr *expr, bool isROSFunction) {
    if (langFeats.cFunction){

        auto callee = expr->getCalleeDecl();
        if (callee == nullptr)
            return;
        
        auto cDecl = dyn_cast<FunctionDecl>(callee);
        if (cDecl == nullptr)
            return;

        // Gets the parent expression.
        auto parDecl = getParentFunction(expr);
        if (parDecl == nullptr)
            return;

        cDecl = cDecl->getCanonicalDecl();
        parDecl = parDecl->getCanonicalDecl();

        if (isInSystemHeader(cDecl)) {
            curCallFunc = parDecl;
        }

        string calleeID = generateID(cDecl);
        RexNode *calleeNode = graph.findNode(calleeID);
        // Ensure that `calleeNode` exists
        if (calleeNode == nullptr) {
            recordFunctionDecl(cDecl);
            calleeNode = graph.findNode(calleeID); 
            assert(calleeNode);
        }

        /**cout << calleeID << endl;
        bool hasNDef = (cDecl->getDefinition() == nullptr);
        if (!hasNDef) {
            cout << "We have a body: " << calleeID << endl; 
        }**/

        // Gets the ID for the parent.
        string callerID = generateID(parDecl);
        RexNode *callerNode = graph.findNode(callerID);
        // Change made here
        if (callerNode == nullptr) {
            recordFunctionDecl(parDecl);
        }

        // Check if the call is conditional
        recordASTControl(expr, calleeNode);

        addOrUpdateEdge(callerID, calleeID, RexEdge::CALLS);
        /**if (hasNDef)
            callEdge->addSingleAttribute(TASchemeAttribute::NO_DEF, "1");**/

        if ((canBuildCFG()) && (!isROSFunction)) {
            if (cfgModel->hasStmt(expr)) {
                std::string callerCFGID = cfgModel->getCFGNodeID(expr);
                std::string calleeCFGID = generateCFGEntryBlockID(cDecl);

                // Associate `call` with the CFG block it happens in
                addOrUpdateEdge(calleeID, calleeCFGID, RexEdge::C_DESTINATION);
                addOrUpdateEdge(callerID, callerCFGID, RexEdge::C_SOURCE);
                cfgModel->markCFGNodeContainsFact(expr); 
                
                RexEdge *invokeEdge =
                addEdgeIfNotExist(callerCFGID, calleeCFGID, RexEdge::NEXT_CFG_BLOCK);
                invokeEdge->addSingleAttribute(TASchemeAttribute::CFG_INVOKE, "1");

                // Add edge from exit block of function to the call's block
                RexEdge *retEdge = 
                addEdgeIfNotExist(generateCFGExitBlockID(cDecl), callerCFGID, RexEdge::NEXT_CFG_BLOCK);
                retEdge->addSingleAttribute(TASchemeAttribute::CFG_RETURN, "1");
                
                
                /**if (hasNDef) {
                    retEdge->addSingleAttribute(TASchemeAttribute::NO_DEF, "1");
                    invokeEdge->addSingleAttribute(TASchemeAttribute::NO_DEF, "1");
                }**/         
            } else {
                cerr << "Rex Warning: Could not find a mapping from call expression to CFG block. ";
                expr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
                cerr << endl;
            }
        }

    }
}

/**
 * Records a return statement.
 * @param retStmt The return statement to record.
 */
void ParentWalker::recordReturnStmt(const ReturnStmt *retStmt) {
    if (canBuildCFG()) {
        if (cfgModel->hasStmt(retStmt)) {
            cfgModel->markCFGNodeContainsFact(retStmt);
        } else {
            cerr << "Rex Warning: Could not find a mapping from call expression to CFG block. ";
            retStmt->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
            cerr << endl;
        }
    }
}

/**ƒ
 * Records whether a statement is part of a control structure, creating either
 * a varInfFunc or varInfluence relationship.
 * @param baseStmt The statement to process. MUST NOT BE NULL.
 * @param astItem The node to add as the target of the relationship. MUST NOT BE NULL.
 */
void ParentWalker::recordASTControl(const Stmt *baseStmt, const RexNode *astItem) {
    // First, we need to determine if this is part of some control structure,
    // and if so, extract the variables used in the condition.
    const Stmt* controlStmt = nullptr;
    vector <std::string> vars;
    bool inCond;
    const Expr* condition;


    // get id of expression for comparison against condition
    int64_t baseExprID = baseStmt->getID(*Context);

    // Get the parent.
    auto parent = Context->getParents(*baseStmt);
    while (!parent.empty()) {

        // Cast to some control structures.
        auto curIfStmt = parent[0].get<IfStmt>();
        if (curIfStmt && isa<IfStmt>(curIfStmt)) {
            //cout << "IF" << endl;
            inCond = addVars(curIfStmt, baseExprID, &vars);
            controlStmt = curIfStmt;
            condition = curIfStmt->getCond();
        }
        auto curForStmt = parent[0].get<ForStmt>();
        if (curForStmt && isa<ForStmt>(curForStmt)) {
            //cout << "For" << endl;
            inCond = addVars(curForStmt, baseExprID, &vars);
            controlStmt = curForStmt;
            condition = curForStmt->getCond();
        }
        auto curWhileStmt = parent[0].get<WhileStmt>();
        if (curWhileStmt && isa<WhileStmt>(curWhileStmt)) {
            //cout << "While" << endl;
            inCond = addVars(curWhileStmt, baseExprID, &vars);
            controlStmt = curWhileStmt;
            condition = curWhileStmt->getCond();
        }
        auto curDoStmt = parent[0].get<DoStmt>();
        if (curDoStmt && isa<DoStmt>(curDoStmt)) {
            //cout << "Do" << endl;
            inCond = addVars(curDoStmt, baseExprID, &vars);
            controlStmt = curDoStmt;
            condition = curDoStmt->getCond();
        }
        auto curSwitchStmt = parent[0].get<SwitchStmt>();
        if (curSwitchStmt && isa<SwitchStmt>(curSwitchStmt)) {
            //cout << "Switch" << endl;
            inCond = addVars(curSwitchStmt, baseExprID, &vars);
            controlStmt = curSwitchStmt;
            condition = curSwitchStmt->getCond();
        }

        //cout << "Done find vars" << endl;
        parent = Context->getParents(parent[0]);

        // ignore affected variables when:
        //     - no control statement is found
        //     - the base statement exist in the same condition as the current decision structure 
        if ((!controlStmt) || (inCond)) {
            vars.clear();
            //cout << "Nothing" << endl;
            continue;
        }

        if (condition == nullptr) {
            vars.clear();
            //cout << "Nothing" << endl;
            continue;
        }

        // create varInfFunc/varInfluence relationship based on all variables found in condition
        for (const string varID : vars) {

            RexEdge::EdgeType infEdgeType;
            RexEdge::EdgeType cfgInfEdgeSrc;
            RexEdge::EdgeType cfgInfEdgeDest;

            if (astItem->getType() == RexNode::FUNCTION) {
                infEdgeType = RexEdge::VAR_INFLUENCE_FUNC;
                cfgInfEdgeSrc = RexEdge::VIF_SOURCE;
                cfgInfEdgeDest = RexEdge::VIF_DESTINATION;
            } else {
                infEdgeType = RexEdge::VAR_INFLUENCE;
                cfgInfEdgeSrc = RexEdge::VI_SOURCE;
                cfgInfEdgeDest = RexEdge::VI_DESTINATION;
            }

            addEdgeIfNotExist(varID, astItem->getID(), infEdgeType); 

            std::string vifSrcCfgID;

            if (canBuildCFG()) {
                // if the source is a return node, then get the exiting block of the called function
                if (isReturnID(varID))
                    vifSrcCfgID = varID.substr(0, varID.size()-RETURN_SUFFIX.size()) + ":CFG:0";
                // if the source is a variable, then get the CFG where the condition of the decision
                // structure is located 
                else if (cfgModel->hasStmt(condition)) {
                    vifSrcCfgID = cfgModel->getCFGNodeID(condition);
                    cfgModel->markCFGNodeContainsFact(condition);
                } else {
                    //unable to find mapping
                    cerr << "Rex Warning: Could not find a mapping from statement to CFG block for varInfFunc src facts. ";
                    condition->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
                    cerr << endl;
                    break; // do not create varInfFunc fact  and cfg linkage if not found 
                }
            }

            if (canBuildCFG()){
                if ((!vifSrcCfgID.empty()) && (cfgModel->hasStmt(baseStmt))) {
                    const std::string &vifDesCfgID = cfgModel->getCFGNodeID(baseStmt);

                    // Link `varInfFunc` or `varInfluence` fact to their corresponding CFG block
                    // the variable to the condition CFG block, the function call to the CFG it resides in
                    addOrUpdateEdge(varID, vifSrcCfgID, cfgInfEdgeSrc);
                    addOrUpdateEdge(astItem->getID(), vifDesCfgID, cfgInfEdgeDest);
                } else {
                    cerr << "Rex Warning: Could not find a mapping from statement to CFG block for varInfFunc facts. ";
                    baseStmt->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
                    cerr << endl;
                    continue;
                }
            }

        }

        vars.clear();
        
    }

}

/**
 * Records whether a variable is used in a control flow statement.
 * @param expr The expression to analyze.
 */
void ParentWalker::recordControlFlow(const DeclRefExpr *expr) {
    // Get the decl responsible.
    const ValueDecl *decl = expr->getDecl();
    recordControlFlow(decl);
}

/**
 * Records whether a variable is used in a control flow statement.
 * @param expr The expression to analyze.
 */
void ParentWalker::recordControlFlow(const MemberExpr *expr) {
    const ValueDecl *decl = expr->getMemberDecl();
    recordControlFlow(decl);
}

/**
 * Records whether a variable is used in a control flow statement.
 * @param decl The value declaration to analyze.
 */
void ParentWalker::recordControlFlow(const ValueDecl *decl) {
    // Get the ID and find it.
    string declID = generateID(decl);
    RexNode *refNode = graph.findNode(declID);
    if (!refNode) {
        return;
    }
    // Adds 1 to the control flag attr.
    refNode->addSingleAttribute(TASchemeAttribute::IS_CONTROL_FLOW, "1");
}

/**
 * Records the parent function in a statement.
 * @param statement The statement.
 * @param baseItem The baseItem of the parent.
 */
void ParentWalker::recordParentFunction(
    const Stmt *statement, RexNode *baseItem) {
    // Gets the parent function.
    const FunctionDecl *decl = getParentFunction(statement);
    // This was segfaulting when running Rex in batch script.
    // Pretty much a band-aid, since I don't know what's at the root
    // (Obviously decl was nullptr)
    if(!decl)
		return;

    //addOrUpdateEdge(generateID(decl), baseItem->getID(), RexEdge::CALLS);

    RexEdge *edge = graph.findEdge(generateID(decl), baseItem->getID(), RexEdge::CALLS);
    if (edge) {
        updateEdge(edge);
        return;
    }
    addEdgeToGraph(generateID(decl), baseItem->getID(), RexEdge::CALLS);


}

/**
 * Adds all variables in the condition of an if statement to the vars vector.
 * @param stmt The statement.
 * @param baseExprID The AST ID of the statement or its parent variable.
 * @param varsPtr The pointer to the vars vector that stores all variable id affecting the statement.
 * @return Boolean value denoting whether the statement occurs inside the condition of the if statement.
 */
bool ParentWalker::addVars(const IfStmt *stmt, int64_t baseExprID, std::vector<std::string> *varsPtr) {
    return addVars({stmt->getCond()}, baseExprID, varsPtr);
}



/**
 * Adds all variables in the condition of an for loop to the vars vector.
 * @param stmt The statement.
 * @param baseExprID The AST ID of the statement or its parent variable.
 * @param varsPtr The pointer to the vars vector that stores all variable ids affecting the statement.
 * @return Boolean value denoting whether the statement occurs inside the condition of the for loop.
 */
bool ParentWalker::addVars(const ForStmt *stmt, int64_t baseExprID, std::vector<std::string> *varsPtr) {
    return addVars({stmt->getInit(), stmt->getCond(), stmt->getInc()}, baseExprID, varsPtr);
}

/**
 * Adds all variables in the condition of an while loop to the vars vector.
 * @param stmt The statement.
 * @param baseExprID The AST ID of the statement or its parent variable.
 * @param varsPtr The pointer to the vars vector that stores all variable ids affecting the statement.
 * @return Boolean value denoting whether the statement occurs inside the condition of the while loop.
 */
bool ParentWalker::addVars(const WhileStmt *stmt, int64_t baseExprID, std::vector<std::string> *varsPtr) {
    return addVars({stmt->getCond()}, baseExprID, varsPtr);
}

/**
 * Adds all variables in the condition of a do loop to the vars vector.
 * @param stmt The statement.
 * @param baseExprID The AST ID of the statement or its parent variable.
 * @param varsPtr The pointer to the vars vector that stores all variable ids affecting the statement.
 * @return Boolean value denoting whether the statement occurs inside the condition of the do loop.
 */
bool ParentWalker::addVars(const DoStmt *stmt, int64_t baseExprID, std::vector<std::string> *varsPtr) {
    return addVars({stmt->getCond()}, baseExprID, varsPtr);
}

/**
 * Adds all variables in the condition of a switch statement to the vars vector.
 * @param stmt The statement.
 * @param baseExprID The AST ID of the statement or its parent variable.
 * @param varsPtr The pointer to the vars vector that stores all variable ids affecting the statement.
 * @return Boolean value denoting whether the statement occurs inside the condition of the switch statement.
 */
bool ParentWalker::addVars(const SwitchStmt *stmt, int64_t baseExprID, std::vector<std::string> *varsPtr) {
    return addVars({stmt->getCond()}, baseExprID, varsPtr);
}

/**
 * Adds all variables in the condition of a statement to the vars vector. This is the helper
 * method.
 * @param stmt The statement (conditions of the decision structure).
 * @param baseExprID The AST ID of the statement or its parent variable.
 * @param varsPtr The pointer to the vars vector that stores all variable ids affecting the statement.
 * @return Boolean value denoting whether the statement occurs inside the condition of the decision structure.
 */
bool ParentWalker::addVars(std::set<const Stmt*> stmts, int64_t baseExprID, 
    std::vector<std::string> *varsPtr) {
    
    int inCond = false;
    if (stmts.empty()) 
        return inCond;

    // loop through all condition statements
    for (const Stmt *stmt : stmts) {
        
        if (stmt == nullptr)
            continue;

        // check if statement is a part of the decision structure condition
        // if it is stop looking
        int64_t conditionID = stmt->getID(*Context);
        if (conditionID == baseExprID) {
            varsPtr->clear();
            return true;
        }

        // Gets the children of this statement.
        for (const auto &child : stmt->children()) {

            if(child)
            {
                if (isa<DeclRefExpr>(child)) {
                    auto dref = dyn_cast<DeclRefExpr>(child);
                    auto decl = dref->getDecl();
                    if ((!isa<FunctionDecl>(decl))) {
                        varsPtr->push_back(generateID(dyn_cast<NamedDecl>(decl)));
                    } else {
                        // record return variable that happens in condition of decision structure 
                        if (!isInSystemHeader(decl))
                            varsPtr->push_back(generateRetID(dyn_cast<FunctionDecl>(decl)));
                    }
                } else {
                    // check if it is member expression
                    if (isa<MemberExpr>(child)) {
                        auto mexpr = dyn_cast<MemberExpr>(child);
                        auto decl = mexpr->getMemberDecl();
                        if (!isa<FunctionDecl>(decl)) {
                            varsPtr->push_back(generateID(dyn_cast<NamedDecl>(decl)));
                        } else {
                            // record return variable that happens in condition of decision structure 
                            if (!isInSystemHeader(decl))
                                varsPtr->push_back(generateRetID(dyn_cast<FunctionDecl>(decl)));
                        }
                    }
                    // add to varsPtr if a parent variable is found
                    else if (isa<CXXMemberCallExpr>(child)) {

                        auto mcexpr = dyn_cast<CXXMemberCallExpr>(child);
                        NamedDecl *parentDecl = getParentVariable(mcexpr);
                        auto calleeDecl = mcexpr->getCalleeDecl();

                        if ((parentDecl != nullptr)) {
                            varsPtr->push_back(generateID(dyn_cast<NamedDecl>(parentDecl)));
                        }
                        // record return variable that happens in condition of decision structure 
                        if (isa<FunctionDecl>(calleeDecl)) {
                            if (!isInSystemHeader(calleeDecl))
                                varsPtr->push_back(generateRetID(dyn_cast<FunctionDecl>(calleeDecl)));
                        }
                        

                    }

                    // Gets the children. (a hackish way to also track pointers)
                    inCond = inCond || addVars({child}, baseExprID, varsPtr);
                    if (inCond) {
                        varsPtr->clear();
                        break;
                    } 
                }
            }
        }
        
        if (inCond) 
            break;

    }

    return inCond;
}

// debug code
// print expr
string printExpr(const Expr *expr, const ASTContext *Context) {
    std::string expr_string;
    llvm::raw_string_ostream stream(expr_string);
    expr->printPretty(stream, nullptr, Context->getPrintingPolicy());
    stream.flush();
    return expr_string;
}

/**
 * Gets the item that was assigned based on an operator call.
 * @param parent The parent object.
 * @return The decl that is the assignee.
 */
const NamedDecl *ParentWalker::getAssignee(const CXXOperatorCallExpr *parent) {
    // Gets the lhs of the expression.
    int num = parent->getNumArgs();
    if (num != 2)
        return nullptr;
    const Expr *lhs = parent->getArg(0);

    // Gets the base expression type.
    const MemberExpr *lhsCast = dyn_cast<MemberExpr>(lhs);
    if (!lhsCast)
        return nullptr;

    // Next, gets the referenced item.
    const NamedDecl *decl =
        dyn_cast<NamedDecl>(lhsCast->getReferencedDeclOfCallee());
    return decl;
}

/**
 * Gets the assignment statement.
 * @param parent The parent object.
 * @return The assign statement part of this operator.
 */
const MemberExpr *ParentWalker::getAssignStmt(
    const CXXOperatorCallExpr *parent) {
    int num = parent->getNumArgs();
    if (num != 2)
        return nullptr;
    const Expr *rhs = parent->getArg(1);

    // Digs down into all the temporary expressions.
    const MaterializeTemporaryExpr *tempExpr1 =
        dyn_cast<MaterializeTemporaryExpr>(rhs);
    const CXXMemberCallExpr *tempExpr2 =
        dyn_cast<CXXMemberCallExpr>(dyn_cast<CXXBindTemporaryExpr>(
            dyn_cast<ImplicitCastExpr>(tempExpr1->GetTemporaryExpr())
                ->getSubExpr())
                                        ->getSubExpr());
    const MemberExpr *lastItem = dyn_cast<MemberExpr>(tempExpr2->getCallee());
    return lastItem;
}

/**
 * Gets the parent class of a decl.
 * @param decl The decl to find the parent class.
 * @return The class.
 */
const CXXRecordDecl *ParentWalker::getParentClass(const NamedDecl *decl) {
    bool getParent = true;

    // Get the parent.
    auto parent = Context->getParents(*decl);
    while (getParent) {
        // Check if it's empty.
        if (parent.empty()) {
            getParent = false;
            continue;
        }

        // Get the current decl as named.
        auto currentDecl = parent[0].get<NamedDecl>();
        if (currentDecl && isa<CXXRecordDecl>(currentDecl)) {
            return dyn_cast<CXXRecordDecl>(currentDecl);
        }

        parent = Context->getParents(parent[0]);
    }

    return nullptr;
}

/**
 * Checks whether an entity is a class.
 * @param ctor The class.
 * @param className The name of the class.
 * @return Whether the two items compare.
 */
bool ParentWalker::isClass(const CXXConstructExpr *ctor, string className) {
    // Get the underlying class.
    QualType type = ctor->getBestDynamicClassTypeExpr()->getType();
    if (type->isArrayType())
        return false;

    auto record = ctor->getBestDynamicClassType();
    if (record == nullptr)
        return false;

    // Check the qualified name.
    if (record->getQualifiedNameAsString() != className)
        return false;
    return true;
}

/**
 * Checks whether an entity is a function..
 * @param expr The call expression.
 * @param functionName The name of the function.
 * @return Whether the two items compare.
 */
bool ParentWalker::isFunction(const CallExpr *expr, string functionName) {
    // Gets the underlying callee.
    if (expr->getCalleeDecl() == nullptr)
        return false;
    auto callee = expr->getCalleeDecl()->getAsFunction();
    if (callee == nullptr)
        return false;

    // Checks the value of the callee.
    if (callee->getQualifiedNameAsString() != functionName)
        return false;
    return true;
}

/**
 * Gets the decl assigned to a class.
 * @param expr The constructor expression.
 * @return The decl that was assigned.
 */
const NamedDecl *ParentWalker::getParentAssign(const CXXConstructExpr *expr) {
    bool getParent = true;

    // Get the parent.
    auto parent = Context->getParents(*expr);
    while (getParent) {
        // Check if it's empty.
        if (parent.empty()) {
            getParent = false;
            continue;
        }

        // Get the current decl as named.
        auto currentDecl = parent[0].get<NamedDecl>();
        if (currentDecl && (isa<VarDecl>(currentDecl) ||
                               isa<FieldDecl>(currentDecl) ||
                               isa<ParmVarDecl>(currentDecl))) {
            return currentDecl;
        } else if (currentDecl && isa<FunctionDecl>(currentDecl)) {
            getParent = false;
            continue;
        }

        parent = Context->getParents(parent[0]);
    }

    return nullptr;
}

/**
 * Gets arguments of a call expression.
 * @param expr The expression to get arguments.
 * @return The vector of arguments.
 */
vector<string> ParentWalker::getArgs(const CallExpr *expr) {
    string nodeuffer;

    // Creates a vector.
    vector<string> args;

    // Get the arguments for the CallExpr.
    int numArgs = expr->getNumArgs();
    auto argList = expr->getArgs();
    for (int i = 0; i < numArgs; i++) {
        nodeuffer = "";
        auto curArg = argList[i];

        // Pushes the argument string into a vector.
        llvm::raw_string_ostream strStream(nodeuffer);
        curArg->printPretty(strStream, nullptr, Context->getPrintingPolicy());
        nodeuffer = strStream.str();
        args.push_back(nodeuffer);
    }

    return args;
}

/**
 * Gets the parent function to process.
 * @param baseFunc The base function.
 * @return The function declaration object.
 */
const FunctionDecl *ParentWalker::getParentFunction(const Stmt *baseFunc) {
    bool getParent = true;

    // Get the parent.
    auto parent = Context->getParents(*baseFunc);
    while (getParent) {
        // Check if it's empty.
        if (parent.empty()) {
            getParent = false;
            continue;
        }

        // Get the current decl as named.
        auto currentDecl = parent[0].get<NamedDecl>();
        if (currentDecl && isa<FunctionDecl>(currentDecl)) {
            return dyn_cast<FunctionDecl>(currentDecl);
        }

        parent = Context->getParents(parent[0]);
    }

    return nullptr;
}

/**
 * Extracts template arguments from member expressions.
 * E.g. expr<A, B>(...) would extract <<A,B>>
 * @param expr - The member expression to extract
 * @return A vector containing the extracted template types as strings
 */
vector<string> ParentWalker::extractTemplateType(MemberExpr* expr)
{
	vector<string> result;
	ArrayRef<TemplateArgumentLoc> tmplArgs = expr->template_arguments();
	for(auto argIter = tmplArgs.begin(); argIter != tmplArgs.end(); argIter++)
	{
		const TemplateArgument arg = (*argIter).getArgument();
		result.push_back(arg.getAsType().getAsString());
	}
    return result;
}

/**
 * Gets the parent variable of a call expression.
 * @param callExpr The call expression.
 * @return The parent variable.
 */
NamedDecl *ParentWalker::getParentVariable(const Expr *callExpr) {

    // Get the CXX Member Call.
    auto call = dyn_cast<CXXMemberCallExpr>(callExpr);
    if (call == nullptr)
        return nullptr;

    auto parentVar = call->getImplicitObjectArgument();
    if (parentVar == nullptr) {
        return nullptr;
    }

    auto refCalleeDecl = parentVar->getReferencedDeclOfCallee();
    if (refCalleeDecl == nullptr) {
        return nullptr;
    }

    return dyn_cast<NamedDecl>(refCalleeDecl);

}

void ParentWalker::recordFunctionVarUsage(const FunctionDecl *decl,
                                          const std::set<std::string>& lhs,
                                          const std::set<std::string>& rhs) {
    // if (decl == nullptr || !langFeaddAttributeaddAttributeats.cFunction || !langFeats.cVariable)
    //     return; --NOT WORKING

    if (decl == nullptr || !langFeats.cFunction || !langFeats.cVariable)
        return;

    assert(curDflowStmt);

    std::string cfgNodeID{};
    if ((canBuildCFG()) && (cfgModel->hasStmt(curDflowStmt))) {
        cfgNodeID = cfgModel->getCFGNodeID(curDflowStmt);
    }

//Extract line number here using SourceManager or GetContext
    int line_number = Context->getSourceManager().getSpellingLineNumber(curDflowStmt->getBeginLoc());
    llvm::errs() << "Extracted line number: " << line_number << "\n";  // Debugging statement
//or something of that sort

//d'apres curDflowStmt->getBeginLoc().print(llvm::errs(), Context->getSourceManager()); on L1723.

    string functionID = generateID(decl);
    for (const auto &var : lhs) {
        addOrUpdateEdge(functionID, var, RexEdge::WRITES);

        if (!cfgNodeID.empty()) {
            // Link `write` fact to CFG block
            addOrUpdateEdge(functionID, cfgNodeID, RexEdge::W_SOURCE);
            addOrUpdateEdge(var, cfgNodeID, RexEdge::W_DESTINATION);
        }

        for (auto rhsItem : rhs) {

            if (isReturnID(rhsItem)) {
		//Note it is the same as below - returns now have a line number too. 
//                auto edge = addOrUpdateEdge(rhsItem, var, RexEdge::RET_WRITES);
		  auto edge = addOrUpdateEdge(rhsItem, var, RexEdge::RW_DESTINATION);
                edge->RexEdge::addSingleAttribute("LINE_NUMBER", std::to_string(line_number)); //NEW!

            } else {
//                auto edge = addOrUpdateEdge(rhsItem, var, RexEdge::VAR_WRITES);
  //Incorrect on account of single varWrite fact possibly having multiple CFG blocks; amended to be the source specifically. 
		auto edge = addOrUpdateEdge(rhsItem, var, RexEdge::VW_SOURCE);              
                  //addAttribute of extracted line number here 
		edge->RexEdge::addSingleAttribute("LINE_NUMBER", std::to_string(line_number));                
            	llvm::errs() << "Added attribute LINE_NUMBER: " << line_number << " to edge between " << rhsItem << " and " << var << "\n";  // Debugging statement    
        }



            //edge->addMultiAttribute(TASchemeAttribute::CONTAINING_FUNCTIONS, functionID);
            if (canBuildCFG()) {
                if (!cfgNodeID.empty()) {
                    cfgModel->markCFGNodeContainsFact(curDflowStmt);
                    //edge->addMultiAttribute(TASchemeAttribute::CFG_BLOCKS, cfgNodeID);

                    // Link item to CFG block (source and destination edges)
                    // Differentiate return node write and normal variable write
                    if (isReturnID(rhsItem)) {    
                        string cfgSrcNodeID = rhsItem.substr(0, rhsItem.size()-RETURN_SUFFIX.size()) + ":CFG:0";
                        addOrUpdateEdge(rhsItem, cfgSrcNodeID, RexEdge::RW_SOURCE);
                        addOrUpdateEdge(var, cfgNodeID, RexEdge::RW_DESTINATION);
                    } else {
                        addOrUpdateEdge(rhsItem, cfgNodeID, RexEdge::VW_SOURCE);
                        addOrUpdateEdge(var, cfgNodeID, RexEdge::VW_DESTINATION);
                    }
                    
                } else {
                    cerr << "Rex Warning: Could not find a mapping from statement to CFG block for varWrite/retWrite facts. ";
                    curDflowStmt->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
                    cerr << endl;
                }
            }

        }
    }
}

void ParentWalker::recordParameterWrite(const FunctionDecl *calledFn,
                                        const std::string& param,
                                        const std::set<std::string>& rhs) {
    if (calledFn == nullptr || !langFeats.cFunction || !langFeats.cVariable)
        return;

    assert(curDflowStmt);

    //bool hasNDef = (calledFn->getDefinition() != nullptr);

    std::string cfgNodeID{};
    if (canBuildCFG()) {
        addOrUpdateEdge(param, generateCFGEntryBlockID(calledFn), RexEdge::PW_DESTINATION);
        if (cfgModel->hasStmt(curDflowStmt)) {
            cfgNodeID = cfgModel->getCFGNodeID(curDflowStmt);
        }
    }

    for (auto rhsItem : rhs) {
        // auto edge = addOrUpdateEdge(rhsItem, param, RexEdge::PAR_WRITES);
        addOrUpdateEdge(rhsItem, param, RexEdge::PAR_WRITES);
        //RexEdge *edge = addOrUpdateEdge(rhsItem, param, RexEdge::PAR_WRITES);
        /**if (hasNDef) 
            edge->addSingleAttribute(TASchemeAttribute::NO_DEF, "1");**/

        // only create the fact if path exist or 
        if (canBuildCFG()) {
            if (!cfgNodeID.empty()) {
                
                cfgModel->markCFGNodeContainsFact(curDflowStmt);

                if (isReturnID(rhsItem))
                    cfgNodeID = rhsItem.substr(0, rhsItem.size()-RETURN_SUFFIX.size()) + ":CFG:0";

                // Link item to CFG block
                addOrUpdateEdge(rhsItem, cfgNodeID, RexEdge::PW_SOURCE);
            } else {
                cerr << "Rex Warning: Could not find a mapping from statement to CFG block for parWrite. ";
                curDflowStmt->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
                cerr << endl;
            }
        }
    }
}

bool ParentWalker::isStatic(const NamedDecl *decl) {
    // If the decl is static (internal linkage), we can define the same name in
    // multiple compilation units without conflicts. That means that if we want
    // the ID to be unique, we need to prepend the filename (i.e. the name of
    // the compilation unit) so that the decl is never associated with
    // conflicting decls from some other compilation unit.
    //
    // Speaking of internal linkage, the clang docs have an interesting table of
    // all the different variants of the Linkage enum. It turns out that there
    // are ways to have what is "effectively" internal linkage without actually
    // being explicitly static.
    //
    // As of this writing, both InternalLinkage and UniqueExternalLinkage
    // effectively give you internal linkage.
    //
    // Read more: https://clang.llvm.org/doxygen/namespaceclang.html#a6cb4fc7755d8dccec57b672a2c83b5ce
    //const NamedDecl *namedDecl = dyn_cast<NamedDecl>(decl->getCanonicalDecl());
    Linkage linkage = decl->getLinkageAndVisibility().getLinkage();

    // To make things easier, we break things down into two categories:
    //
    // 1. `static` - these need not be linked because they can only be used
    //    within one compilation unit. These do however need the name of the
    //    compilation unit (i.e. the filename) added so that their ID is unique.
    // 2. `extern` - these need to be linked. We can do without the filename
    //    because the ID is already unique. It's even unique across multiple
    //    executables because we add the featureName at the end of this method.
    //    Not having the filename makes linking `extern int var` way easier.
    //
    // Everything is assumed to be `extern` by default, so we only add something
    // for `static` linkage.
    if (linkage == InternalLinkage || linkage == UniqueExternalLinkage) {
        return true;
    }
    return false;
}

bool ParentWalker::isExternal(const NamedDecl *decl) {
    // If the decl is static (internal linkage), we can define the same name in
    // multiple compilation units without conflicts. That means that if we want
    // the ID to be unique, we need to prepend the filename (i.e. the name of
    // the compilation unit) so that the decl is never associated with
    // conflicting decls from some other compilation unit.
    //
    // Speaking of internal linkage, the clang docs have an interesting table of
    // all the different variants of the Linkage enum. It turns out that there
    // are ways to have what is "effectively" internal linkage without actually
    // being explicitly static.
    //
    // As of this writing, both InternalLinkage and UniqueExternalLinkage
    // effectively give you internal linkage.
    //
    // Read more: https://clang.llvm.org/doxygen/namespaceclang.html#a6cb4fc7755d8dccec57b672a2c83b5ce
    //const NamedDecl *namedDecl = dyn_cast<NamedDecl>(decl->getCanonicalDecl());
    Linkage linkage = decl->getLinkageAndVisibility().getLinkage();

    // To make things easier, we break things down into two categories:
    //
    // 1. `static` - these need not be linked because they can only be used
    //    within one compilation unit. These do however need the name of the
    //    compilation unit (i.e. the filename) added so that their ID is unique.
    // 2. `extern` - these need to be linked. We can do without the filename
    //    because the ID is already unique. It's even unique across multiple
    //    executables because we add the featureName at the end of this method.
    //    Not having the filename makes linking `extern int var` way easier.
    //
    // Everything is assumed to be `extern` by default, so we only add something
    // for `static` linkage.
    cout << decl->getNameAsString() << "\t";

     if (linkage == NoLinkage ) {
        cout << "No Linkage" << endl;
    } else if (linkage == InternalLinkage  ) {
        cout << "Internal Linkage" << endl;
    } else if (linkage == UniqueExternalLinkage  ) {
        cout << "Unique External Linkage" << endl;
    } else if (linkage == ModuleLinkage  ) {
        cout << "Module External Linkage" << endl;
    } else {
        cout << "External Linkage" << endl;
        return true;
    }
    return false;
}

/**
 * Checks whether a statement is in the system header.
 * @param statement The statement.
 * @return Whether its in the system header.
 */
bool ParentWalker::isInSystemHeader(const Stmt *statement) {
    if (statement == nullptr)
        return false;

    // Get the system header.
    bool isIn;
    try {
        // Get the source manager.
        auto &manager = Context->getSourceManager();

        // Check if in header.
        isIn = isInSystemHeader(manager, statement->getBeginLoc());
    } catch (...) {
        return false;
    }

    return isIn;
}

/**
 * Checks whether a decl is in the system header.
 * @param decl The declaration.
 * @return Whether its in the system header.
 */
bool ParentWalker::isInSystemHeader(const Decl *decl) {
    if (decl == nullptr)
        return false;

    // Get the system header.
    bool isIn;
    try {
        // Get the source manager.
        auto &manager = Context->getSourceManager();

        // Check if in header.
        isIn = isInSystemHeader(manager, decl->getBeginLoc());
    } catch (...) {
        return false;
    }

    return isIn;
}

std::string ParentWalker::generateRetID(const clang::FunctionDecl *fdecl) {
    return generateID(fdecl) + "__ret!;;";
}

/**
 * Generates a unique ID based on a member expression.
 * @param member The member expression to generate the ID for.
 * @return A string of the ID.
 */
string ParentWalker::generateID(const MemberExpr *member) {
    Expr *lhs = member->getBase();
    // if member = `x.foo`  ===> x
    if (auto *declref = dyn_cast<DeclRefExpr>(lhs)) {
        return generateID(declref->getDecl());
    } else { // if member == (something else).foo  ===> foo
        return generateID(member->getMemberDecl());
    }
}

string ParentWalker::generateCFGBlockID(const clang::FunctionDecl *fdecl, const clang::CFGBlock* block, const clang::CFG* cfg)
{
    if (block == &(cfg->getEntry())) {
        return generateCFGEntryBlockID(fdecl);
    } else {
        return generateCFGInnerBlockID(fdecl, block);
    }
}
string ParentWalker::generateCFGEntryBlockID(const FunctionDecl *fdecl)
{
    return generateID(fdecl) + ":CFG:ENTRY";
}
string ParentWalker::generateCFGInnerBlockID(const FunctionDecl *fdecl, const CFGBlock* block)
{
    return generateID(fdecl, true, (block->getBlockID() != 0)) + ":CFG:" + to_string(block->getBlockID());
}
string ParentWalker::generateCFGExitBlockID(const FunctionDecl *fdecl)
{
    return generateID(fdecl, true) + ":CFG:0";
}


/**
 * Generates a unique ID based on a decl.
 * @param decl The decl to generate the ID for.
 * @param root An internal-use-only argument used to differentiate recursive
 *             calls from regular calls
 * @param isCFGBlock checks if this ID is used for generating CFG blocks
 * @return A string of the ID.
 */
string ParentWalker::generateID(const NamedDecl *decl, bool root, bool isInnerCFG) {
    // "root" defaults to true and indicates whether we are in a recursive
    // invocation of this function or not.
    //TODO: Internal-only parameters like this are a code smell. You can
    // remove the internal-only `root` parameter by defining a separate
    // `generateFullyQualifiedNameForID` function (choose a better name
    // please lol). The only place we use generateID recursively is to generate
    // the fully qualified name. You can move the majority of the logic in this
    // function to that new function. Make sure you keep the comments in this
    // function up to date.

    // Note that these IDs are very intentionally structured for the linker to
    // be able to parse. The `;` is used quite extensively to separate different
    // sections of the IDs. Be careful when modifying this code.

    // Some assumptions:
    // * The characters in the ID aren't limited to certain character ranges
    //   because we quote the IDs in the TA file. (Spaces, parenthesis, etc.)
    // * The `::` is just a separator, with no relation to C++. That is, we
    //   won't attempt to extract information from the stuff before and after
    //   `::`. This is important because we often do things like <func>::<var>
    //   and <func> is not guaranteed to be a valid C++ identifier (due to how
    //   static/internal linkage is handled).

    // Functions can have same names but are located in different files, and we 
    // need a way to differentiate from each other. The way achieve this and prevent
    // Rex from generating the same id for these two funcitons, their respective CFG 
    // nodes, and local variables, we are adding the id to the node id.
    // Algorithm for adding filename to generating ids:
    //  * Declaration is static, then the filename is where the definition of
    //        the function is located (usually the .c/.cpp file)
    //  * Declaration is non-static
    //      - Declaration is a function node, return node, CFG entry node, CFG exit node,
    //          or parameter node, then filename is where the declaration of the function
    //          is located (usually the .h/.hpp file)
    //      - Everything else including the inner CFG nodes and local variables declared
    //          within the function have filename where the definition of the function
    //          is located (usually the .c/.cpp file)

    // The ID scheme is also not foolproof. You can see that we output some
    // warnings below where we basically just "give up" and allow some IDs to
    // conflict because we can't get enough info from clang.

    // Anything can be forward declared as many times as you want. The canonical
    // decl sees through that and is supposed to always return a single decl
    // regardless of how many times an item is declared elsewhere.
    string n = decl->getNameAsString();
    if (decl->getCanonicalDecl() != nullptr) {
        n = dyn_cast<NamedDecl>(decl->getCanonicalDecl())->getNameAsString();
    }
    string name = n;

    if (root) {assignedFileName = false;}

    bool isStaticDecl = isStatic(decl);
    bool isFunctionOrMethod = (isa<FunctionDecl>(decl) || isa<CXXMethodDecl>(decl));

    const NamedDecl *canDecl = dyn_cast<NamedDecl>(decl->getCanonicalDecl());
    string filepath = generateFileName(canDecl);
    if ((isInnerCFG)) {   
        filepath = generateFileName(decl);
    } 

    // Adds function parameters to account for function overloading
    if (isFunctionOrMethod) {

        const FunctionDecl *cur = decl->getAsFunction();

        if ((isStaticDecl)) {
            if (cur->getDefinition()) {
                decl = dyn_cast<NamedDecl>(cur->getDefinition());
            }
        } 
        
        // Don't need return type because return types can't be overloaded
        name += '(';
        
        // Add parameter types to name
        for (const ParmVarDecl *parm : cur->parameters()) {
            // This code will result in a trailing comma on every parameter. >:D
            name += parm->getType().getAsString() + ',';
        }
        name += ')';
    }

    // Find the first parent that is a NamedDecl (if any)
    // True if we found a NamedDecl
    bool found = false;
    auto parent = Context->getParents(*canDecl);
    string parentName;
    while (!parent.empty()) {
        // Get the parent decl as named.
        const NamedDecl *parentDecl = parent[0].get<NamedDecl>();

        if (parentDecl) {
            parentName = generateID(parentDecl, false, !isa<ParmVarDecl>(decl));
            if (!parentName.empty())
                name = parentName + "::" + name;
            found = true;
            break;
        }

        parent = Context->getParents(parent[0]);
    }

    const DeclContext *parentContext = nullptr;

    // If no parent was found, check if this is inside a function/method/block
    string funcName;
    if (!found) {

        // Get the function/method/block this was declared inside
        parentContext = canDecl->getParentFunctionOrMethod();

        if (parentContext) {
            if (const auto *parentDecl = dyn_cast<NamedDecl>(parentContext)) {
                funcName = generateID(parentDecl, false, !isa<ParmVarDecl>(decl));
                if (!funcName.empty()) {
                    name = funcName + "::" + name;
                }
            } 
        } 
    } 

    string staticFilename;

    // Getting static filename for static declarations
    if (isStaticDecl) {
        string filename = generateFileName(decl);
        if (filename.empty()) {
            cerr << "Rex Warning: Unable to determine filename for symbol with internal linkage. ";
            cerr << "Node ID may conflict. Name: " << name << endl;
        } else {
            //TODO: We really should use a path relative to the project
            // directory. This will become important if we need to support
            // linking .tao files coming from multiple computers. Having the
            // path in the ID won't work if two users put the project in a
            // different location from each other. The nodes won't be linked.
            staticFilename = filename;
        }
    }

    // Adding filename to node ids
    if ((isFunctionOrMethod) || (!assignedFileName && root)) {
        assignedFileName = true;
        std::vector<std::string> strs;
        boost::split(strs, filepath, boost::is_any_of("/"));
        name = strs.back() + "::" + name;
    }
    
    if (!root) {
        //HACK: This code honestly isn't great. I'm not sure what the right
        // abstractions are anymore. We need `;static;` here or else decls
        // inside static items won't have unique IDs. However the `;static;`
        // part isn't generated until RexDeclID's output operator runs. I guess
        // the answer is to move that `;static;` handling code to the fully
        // qualified name? Maybe it is actually part of that and not a separate
        // section of the ID.
        if (!staticFilename.empty()) { // if it is a staic declaration
            name += ";static;" + staticFilename;
        }
        return name;
    }

    // Prepend the prefix at the end so it actually ends up at the start of the
    // ID. There are several other prepends that happen before this point so
    // that is why this is last.
    //
    // Add an ID-type "decl" so the linker can figure out what to do with
    // this ID immediately as soon as it reads it
    // Also add the feature name so IDs don't conflict when analysing
    // multiple features (executables) together
    // By prepending the featureName, we will even get a unique ID for every
    // main() function since there can be at most one entry point in every
    // executable.

    stringstream ss;
    ss << RexDeclID(featureName, name, staticFilename, ROSFeats.appendFeatureName);
    return ss.str();
}

string ParentWalker::generateName(const NamedDecl *decl) {
    bool getParent = true;
    auto parents = Context->getParents(*decl);
    const NamedDecl *parentDecl;
    string name = decl->getNameAsString();

    while (getParent) {
        // Check if it's empty.
        if (parents.empty()) {
            getParent = false;
            continue;
        }

        // Get the current decl as named.
        parentDecl = parents[0].get<NamedDecl>();
        if (parentDecl) {
            name = parentDecl->getNameAsString() + "::" + name;
            parents = Context->getParents(*parentDecl);
            // recurse = true;
            // getParent = false;
            continue;
        }

        parents = Context->getParents(parents[0]);
    }

    fs::path filePath(generateFileName(decl));
    int MAX_DEPTH = 3;
    int currDepth = MAX_DEPTH;
    for (auto it = filePath.rbegin(); it != filePath.rend() && currDepth > 0;
         it++, currDepth--) {
        if (currDepth == MAX_DEPTH) {
            // Filename
            name = (*it).string() + ":" + name;
        } else {
            // Directories
            name = (*it).string() + "/" + name;
        }
    }
    return name;
}

string ParentWalker::getPartialFileName(string filename, const NamedDecl *decl) {
    if ((curCallFunc != nullptr) && (isInSystemHeader(decl))) {
        filename = generateFileName(curCallFunc);
    }
    size_t foundFile = filename.find_last_of("/");
    string partialFilename = filename;
    if (foundFile) 
        partialFilename = filename.substr(foundFile+1) + ";";
    else {
        partialFilename = "";
    }
    return partialFilename;
}

/**
 * Gets the filename of the decl.
 * @param decl The declaration.
 * @return The filename the declaration is in.
 */
string ParentWalker::generateFileName(const NamedDecl *decl) {

    // Gets the file name.
    SourceManager &sourceManager = Context->getSourceManager();
    string filename = sourceManager.getFilename(decl->getLocation());

    if (filename.empty()) {

        // in cases we cannot locate file with the regular method, here is the alternative
        // Example case: Test function of GTest 
        string location = decl->getBeginLoc().printToString(Context->getSourceManager());
        size_t foundFile = location.find_first_of(":");
        if (foundFile) {
            filename = location.substr(0, foundFile);
        } else {
            return filename;
        }

    }
    filename = fs::absolute(filename).string();

    // The filenames returned by clang can have `..` path elements in them. The `fs::absolute` call
    // does not resolve these, it just guarantees that the filename is indeed an absolute path.
    // Using fs::canonical would resolve all the dots, but it is a very slow operation.
    // Compromise by manually resolving any dots.
    auto smallstr = SmallString<256>(StringRef(filename));
    // if remove_dot_dot==false, will only resolve single dots (.), not not double dots (..)
    llvm::sys::path::remove_dots(smallstr, /*remove_dot_dot=*/true);
    return smallstr.str();
}

/**
 * Gets the location (filename) of the named decl.
 * @param decl - The named declaration.
 * @param node - The Graph Node representating the named decl
 */
void ParentWalker::recordNamedDeclLocation(const NamedDecl *decl, RexNode* node, bool hasBody) {

    // Generates the filename.
    string baseFN = generateFileName(decl);
    string canBaseFN = generateFileName(dyn_cast<NamedDecl>(decl->getCanonicalDecl()));
    if (!baseFN.empty()) {
        if (hasBody)
            node->addSingleAttribute(TASchemeAttribute::FILENAME, baseFN);
        node->addSingleAttribute(TASchemeAttribute::FILENAME_DECLARE, canBaseFN);
    }
}

/**
 * Checks whether an item is in a system header file.
 * @param manager The source manager.
 * @param loc The source location.
 * @return Whether its in a system header file.
 */
bool ParentWalker::isInSystemHeader(
    const SourceManager &manager, SourceLocation loc) {
    if (manager.isMacroBodyExpansion(loc) || manager.isMacroArgExpansion(loc))
        // use the location of the macro itself, so we can filter out system macros
        // (i.e., ROS logging macros which generate a LOT of pollution)
        loc = manager.getSpellingLoc(loc);

    // Check if we have a valid location.
    if (loc.isInvalid()) {
        return false;
    }

    if (manager.isInSystemHeader(loc))
        return true;

    // Now, check to see if we have a ROS library.
    string libLoc = loc.printToString(manager);
    libLoc = libLoc.substr(0, libLoc.find(":"));
    return ignored.shouldIgnore(libLoc);
}

bool ParentWalker::isInMainFile(const Stmt *statement) {
    const SourceManager &mgr = Context->getSourceManager();
    return mgr.isInMainFile(statement->getBeginLoc());
}

bool ParentWalker::isInMainFile(const Decl *decl) {
    decl = decl->getCanonicalDecl();

    // A function in the main file with no body is a forward declaration and
    // thus should not actually be considered declared in the main function.
    // The declaration of its body is elsewhere.
    if (isa<FunctionDecl>(decl) && !decl->hasBody()) {
        return false;
    }

    //TODO: I couldn't figure out how to get a ParmVarDecl's FunctionDecl. None
    //of the usual methods work. Context->getParents(*param) gives you a TypeLoc
    //for the parents. getDeclContext returns a TranslationUnitDecl. If you can
    //figure out how to do this, the pseudo code of what you need to do is
    //below:
    //
    // A parameter of a function that isn't in the main file shouldn't be
    // considered either
    if (const auto *param = dyn_cast<ParmVarDecl>(decl)) {
        auto parent = Context->getParents(*param);
        while (!parent.empty()) {
            // Get the parent decl as named.
            const NamedDecl *parentDecl = parent[0].get<NamedDecl>();

            if (parentDecl) {
                if (isa<FunctionDecl>(parentDecl) || isa<CXXMethodDecl>(parentDecl)) {
                    if (!isInMainFile(parentDecl->getAsFunction())) {
                        return false;
                    }
                }
            }

            parent = Context->getParents(parent[0]);

        }


        /**const auto *parent = ???; // <-- HELP
        const FunctionDecl *func = cast_me_somehow(parent);
        if (!isInMainFile(func)) {
            return false;
        }**/
    }

    const SourceManager &mgr = Context->getSourceManager();
    return !mgr.isWrittenInBuiltinFile(decl->getLocation());
}

/**
 * Check if a declaration reference expression is used in an if statement.
 * @param declRef The declaration reference expression.
 * @return Whether the variable is used.
 */
bool ParentWalker::usedInIfStatement(const Expr *declRef) {
    // Iterate through and find if we have an if statement parent.
    bool getParent = true;
    auto parent = Context->getParents(*declRef);
    auto previous = Context->getParents(*declRef);

    while (getParent) {
        // Check if it's empty.
        if (parent.empty()) {
            getParent = false;
            continue;
        }

        // Get the current parent as an if statement.
        auto ifStmt = parent[0].get<IfStmt>();
        if (ifStmt) {
            auto conditionExpression = previous[0].get<Expr>();
            if (findExpression(conditionExpression))
                return true;
            return false;
        }

        auto switchStmt = parent[0].get<SwitchStmt>();
        if (switchStmt) {
            auto conditionExpression = previous[0].get<Expr>();
            if (findExpression(conditionExpression))
                return true;
            return false;
        }

        previous = parent;
        parent = Context->getParents(parent[0]);
    }

    return false;
}

/**
 * Check if a declaration reference expression is used in a loop.
 * @param declRef The declaration reference expression.
 * @return Whether the variable is used.
 */
bool ParentWalker::usedInLoop(const Expr *declRef) {
    // Iterate through and find if we have an if statement parent.
    bool getParent = true;
    auto parent = Context->getParents(*declRef);
    auto previous = Context->getParents(*declRef);

    while (getParent) {
        // Check if it's empty.
        if (parent.empty()) {
            getParent = false;
            continue;
        }

        // Get the current parent as an if statement.
        auto whileStmt = parent[0].get<WhileStmt>();
        if (whileStmt) {
            auto conditionExpression = previous[0].get<Expr>();
            if (findExpression(conditionExpression))
                return true;
            return false;
        }

        auto forStmt = parent[0].get<ForStmt>();
        if (forStmt) {
            auto conditionExpression = previous[0].get<Expr>();
            if (findExpression(conditionExpression))
                return true;
            return false;
        }

        previous = parent;
        parent = Context->getParents(parent[0]);
    }

    return false;
}

/**
 * Finds an expression based on stored expressions.
 * @param expression The expression to find.
 * @return Whether the expression was found.
 */
bool ParentWalker::findExpression(const Expr *expression) {
    for (Expr *cur : parentExpression) {
        if (cur == expression)
            return true;
    }

    return false;
}

/**
 * Adds the parent relationship.
 * @param baseDecl The base declaration.
 * @param baseID The base ID.
 */
void ParentWalker::addParentRelationship(
    const NamedDecl *baseDecl, string baseID) {
    bool getParent = true;
    auto currentDecl = baseDecl;

    // Get the parent.
    auto parent = Context->getParents(*currentDecl);
    while (getParent) {
        // Check if it's empty.
        if (parent.empty()) {
            getParent = false;
            continue;
        }

        // Get the current decl as named.
        currentDecl = parent[0].get<NamedDecl>();
        if (currentDecl) {
            // Get the parent as a NamedDecl.
            string parentID = "";

            // Now, check to see what our current ID is.
            if (isa<FunctionDecl>(currentDecl) ||
                isa<CXXRecordDecl>(currentDecl) ||
                isa<CXXMethodDecl>(currentDecl) || isa<VarDecl>(currentDecl) ||
                isa<FieldDecl>(currentDecl)) {
    
                parentID = generateID(currentDecl);

                addOrUpdateEdge(parentID, baseID, RexEdge::CONTAINS);
                return;
            }
        }

        parent = Context->getParents(parent[0]);
    }
}

// virtual method for specializing a VARIABLE node to something else depending
// on its type. Gives child walkers a chance to decide what this node's type
// should be.
RexNode *ParentWalker::specializedVariableNode(const FieldDecl *decl, bool shouldKeep) {
    string id = generateID(decl);
    return new RexNode(id, RexNode::VARIABLE, shouldKeep);
}
RexNode *ParentWalker::specializedVariableNode(const VarDecl *decl, bool shouldKeep) {
    string id = generateID(decl);
    return new RexNode(id, RexNode::VARIABLE, shouldKeep);
}

bool ParentWalker::canBuildCFG() const {
    return buildCFG && curCFG != nullptr;
}

// Variability Aware - but also these functions are core now.
void ParentWalker::addEdgeToGraph(RexEdge *e) {
    graph.addEdge(e);
}
RexEdge *ParentWalker::addEdgeToGraph(const string& srcID, const string& dstID, RexEdge::EdgeType type) {
    RexNode* srcNode = graph.findNode(srcID);
    RexNode* dstNode = graph.findNode(dstID);

    RexEdge *edge = nullptr;
    if (!srcNode && !dstNode) {
        edge = new RexEdge(srcID, dstID, type);
    } else if (srcNode && !dstNode) {
        edge = new RexEdge(srcNode, dstID, type);
    } else if (!srcNode && dstNode) {
        edge = new RexEdge(srcID, dstNode, type);
    } else {
        edge = new RexEdge(srcNode, dstNode, type);
    }
    addEdgeToGraph(edge);
    return edge;
}
void ParentWalker::addNodeToGraph(RexNode *n) {
    graph.addNode(n);
}

void ParentWalker::updateEdge(RexEdge *e) {
    // Purposefully a no-op - though should consider moving condScope
    // up to this class.

    // Avoid compiler warning.
    (void)e;
    return;
}

RexEdge * ParentWalker::addEdgeIfNotExist(const string& srcID, const string& dstID, RexEdge::EdgeType type) {
    RexEdge *edge = graph.findEdge(srcID, dstID, type);
    if (edge)
        return edge;
    return addEdgeToGraph(srcID, dstID, type);
}

RexEdge *ParentWalker::addOrUpdateEdge(const string& srcID, const string& dstID, RexEdge::EdgeType type) {
    RexEdge *edge = graph.findEdge(srcID, dstID, type);
    if (!edge) {
        return addEdgeToGraph(srcID, dstID, type);
    }
    updateEdge(edge);
    return edge;
}

bool ParentWalker::isReturnID(const std::string& strID) {
    return strID.size() >= RETURN_SUFFIX.size() && 0 == strID.compare(strID.size()-RETURN_SUFFIX.size(), 
    RETURN_SUFFIX.size(), RETURN_SUFFIX);
}

//////////////////////////////
// CFGModel

ParentWalker::CFGModel::CFGModel(ParentWalker& walker)
    : walker(walker) {}
ParentWalker::CFGModel::~CFGModel() {
    for (auto p : clangMap) {
        delete p.second;
    }
}

void ParentWalker::CFGModel::mapASTtoCFG() {
    assert(walker.curCFG != nullptr);

    // Iterate over all basic blocks in the CFG
    for (const CFGBlock* cfgblock : *walker.curCFG) {
        BasicBlock *curBlock = new BasicBlock(cfgblock);
        clangMap[cfgblock] = curBlock;

        if (cfgblock == &(walker.curCFG->getEntry())) {
            entryBlock = curBlock;
        }

        for (const CFGElement& elem: *cfgblock) {
            // The CFGStmt type captures the AST from which the CFG was generated.
            // CFGElement subtypes not deriving from CFGStmt simply provide extra information, and
            // most are controlled by the CFG build options (off by default)
            if (elem.getKind() == CFGElement::Statement) {
                auto stmt = elem.castAs<CFGStmt>().getStmt();
                // In the CFG, function calls are typically hoisted out of
                // expressions they're nested in, and then re-used in the actual expression it's used in:
                // For example:
                //
                // -----------------
                // void foo() {
                //    ...
                //    func(g(i), h(j));
                //    ...
                // }
                // ---- becomes ----
                // [B#n]
                // ...
                // x: g(i)
                // y: h(j)
                // z: func([B#n.x], [B#n.y])
                // ...
                // -----------------
                //
                // However, we map statements recursively to their CFG block, since for a given AST node in the
                // CFG, a fact may be generated for its child instead of the node itself. So in the above example,
                // B#n.y would get mapped first since it was hoisted out of B#n.z, but then when we map B#n.z we would
                // re-encounter B#n.y. Therefore, only map a statement the first time it is encountered, as this prevents
                // duplicate mapping while also allowing the mappings to be split-up according to their actual order
                // of execution.
                if (stmtMap.count(stmt) == 0 && !walker.isInSystemHeader(stmt)) {
                    mapStmt(stmt, curBlock);
                }
                
                /*/ Map function declarations to call expressions that call them
                if (auto call = dyn_cast<CallExpr>(stmt)) {
                    const FunctionDecl *decl = getNonVariadicDirectCallee(call);
                    if (!decl) continue;

                    if (callMap.count(decl) == 0) {
                        callMap[decl] = {};
                    }

                    callMap[decl].push_back(call);
                }
                */
            }
        }
        if (auto terminator = cfgblock->getTerminator().getStmt()) {

            // varWrites that happen inside branch expressions tend to get hoisted out of the branching expression.
            // However, rangefor loops are complicated because the for-loops get converted into a bunch of compiler-generated
            // code with a lot of complicated pieces. The varWrite that Rex extracts (see the RangeFor dataflow logic) isn't actually
            // part of this stuff. So to capture the rangefor varWrite, manually map to the rangefor AST node in the terminator,
            // which is what the Rex fact extractor uses.
            //
            // Also, shouldn't need to check if `stmtMap` contains it already, since the branching constructs ASTs
            // would never get hoisted (i.e., in a `while (goo())`, `goo()` gets hoisted out, but the `while (...)`
            // AST node doesn't), so any particular terminator would only ever occur once in the CFG.
            if ((isa<ForStmt>(terminator) || isa<CXXForRangeStmt>(terminator) || isa<WhileStmt>(terminator)) 
                && !walker.isInSystemHeader(terminator)) {
                mapStmt(terminator, curBlock);
            }

            // We also want to associate if-conditions with the block for the if-statement itself
            if (auto* ifStmt = dyn_cast<IfStmt>(terminator)) {
                mapStmt(ifStmt, curBlock);
            }
        }
    }
}

bool ParentWalker::CFGModel::hasStmt(const clang::Stmt* stmt) const {

    return stmtMap.count(stmt) != 0;
}

std::string ParentWalker::CFGModel::getCFGNodeID(const clang::Stmt* stmt) {

    assert(stmtMap.count(stmt) != 0);
    return stmtMap[stmt]->getCFGNodeID(walker);
}

void ParentWalker::CFGModel::markCFGNodeContainsFact(const clang::Stmt* stmt) {
    assert(stmtMap.count(stmt) != 0);
    BasicBlock *block = stmtMap[stmt];

    block->markCFGNodeContainsFact();
}

void ParentWalker::CFGModel::createAndLinkTAGraph() const {
    // Add each basic block's contents to the graph
    for (auto b: clangMap) {
        auto* block = b.second;
        if (block->shouldKeep())
            block->createAndLinkRexCFGNodes(walker);
    }
    // Basic blocks are connected to each other -- link each tail/head to the tail/head of its neighbors.
    // Must be done separately from above, sine this links the RexNodes created above.
    for (auto p: clangMap) {
        auto* block = p.second;
        if (block->shouldKeep()) {
            for (auto c : block->getEffectiveChildren(clangMap)) {
                walker.addEdgeIfNotExist(block->getTail()->getID(),
                                         c->getHead()->getID(),
                                         RexEdge::NEXT_CFG_BLOCK);
            }
        }
    }
}

void ParentWalker::CFGModel::mapStmtToEntry(const clang::Stmt* stmt) {
    mapStmt(stmt, entryBlock);
}

void ParentWalker::CFGModel::mapStmt(const clang::Stmt* stmt, BasicBlock* block) {
    if (stmt->getStmtClass() == Stmt::DeclStmtClass) {
        // The CFG splits up all DeclStmts containing multiple declarations into "synthetic"
        // DeclStmts, and omits the original single DeclStmt found in the AST.
        //
        // So if we map the synthetic stmts to CFG blocks instead of using the *actual* DeclStmt,
        // then when Rex tries to look up the CFG mapping when making facts for these decls, it will not get anything.
        //
        // void foo() {
        //   int a = x, y = b; // a -varWrite-> x and y -varWrite-> b both belong to the same AST DeclStmt node.
        //                     // But in the CFG, the `int a = x` and `int y = b` are split up into new nodes not actually
        //                     // present in the AST
        // }
        //
        // Fortunately, the Clang CFG keeps a mapping of synthetic DeclStmt --> original DeclStmt, so map the original
        // to the CFG instead of the synthetic versions.
        //
        // Note: This cause multiple facts to be mapped to the same CFG node:
        //  `int x = a.foo(i), y = b.foo(x);`, the varWrites hare are all mapped to the same CFG node.
        // However, this case should be quite rare, and there really shouldn't be any dataflow occurring *across*
        // the decls: `int x = a.foo(i), y = x + 1;` // this is just weird
        for (auto synthetic_stmt: walker.curCFG->synthetic_stmts()) {
            if (synthetic_stmt.first == stmt) {
                // use the node actually found in the AST, as opposed to the "synthetic" one
                // created by the CFG
                stmt = synthetic_stmt.second;
                if (stmtMap.count(stmt) != 0)
                    // since there are multiple synthetic decls per single DeclStmt, only perform the mapping once.
                    return;
                break;
            }
        }
    }

    /**std::string expr_string;
    llvm::raw_string_ostream stream(expr_string);
    stmt->printPretty(stream, nullptr, walker.Context->getPrintingPolicy());
    stream.flush();
    cout << expr_string << endl;
    cout << block->getCFGNodeID(walker) << endl;
    //int64_t baseExprID = stmt->getID(*walker.Context);
    //cout << baseExprID << endl;
    //cout << endl;**/

    stmtMap[stmt] = block;

    // Map the child nodes since facts may be added for one of them instead of for the parent.
    for (auto child: stmt->children()) {
        // Children may sometimes be null (i.e., missing `if` statement initializer)
        if (child &&
                stmtMap.count(child) == 0) // don't re-map statements that have been hoisted (see CFGBlock::mapASTtoCFG)
        {
            mapStmt(child, block);
        }
    }
}


///////////////////////
// CFGModel::BasicBlock

ParentWalker::CFGModel::BasicBlock::BasicBlock(const clang::CFGBlock* cfgBlock)
    : block_(cfgBlock) {
        auto* cfg = cfgBlock->getParent(); // get the CFG this block belongs to;
        if (cfgBlock == &(cfg->getEntry()) // Alternatively, could compare the block IDs.
               || cfgBlock == &(cfg->getExit()))

            // The entry and exit blocks are used for some relations,
            // so they should always be kept
            shouldKeep_ = true;
}

void ParentWalker::CFGModel::BasicBlock::markCFGNodeContainsFact() {
    shouldKeep_ = true;
}

std::string ParentWalker::CFGModel::BasicBlock::getCFGNodeID(ParentWalker& walker) {

    return walker.generateCFGBlockID(walker.curFunc, block_, walker.curCFG.get());
}

bool ParentWalker::CFGModel::BasicBlock::shouldKeep() const { return shouldKeep_; }

set<ParentWalker::CFGModel::BasicBlock*> ParentWalker::CFGModel::BasicBlock::getEffectiveChildren(const ClangMap& mapping) const {
    set<BasicBlock*> alreadyVisited_BlocksThatShouldNotBeKept;
    return doGetEffectiveChildren(mapping, alreadyVisited_BlocksThatShouldNotBeKept);
}
set<ParentWalker::CFGModel::BasicBlock*> ParentWalker::CFGModel::
    BasicBlock::doGetEffectiveChildren(const ClangMap& mapping,
            std::set<BasicBlock*>& alreadyVisited_BlocksThatShouldNotBeKept) const
{
    // The return type is a set because by looking "through" blocks that should not be kept, there can be duplicates.
    set<BasicBlock*> ret;
    // Iterate over the successors of the Clang basic block that this BasicBlock is modelling
    for (auto child: block_->succs()) {
        if (!child) continue; // optimized-out successors are null
        assert(mapping.count(child) != 0); // if a clang block hasn't been mapped then something is really wrong...
        auto* childBlock = mapping.at(child);

        if (childBlock->shouldKeep()) {
            ret.insert(childBlock);
        }
        // The CFG may contain cycles (i.e., `for` statement).
        // If such a cycle exists among blocks that are not to be kept, the recursion will not terminate, so don't
        // re-visit a block that should not be kept. In such cases, the cycle's net set of "effective children" is empty.
        else if (alreadyVisited_BlocksThatShouldNotBeKept.count(childBlock) == 0) {
            alreadyVisited_BlocksThatShouldNotBeKept.insert(childBlock);
            auto rec = childBlock->doGetEffectiveChildren(mapping, alreadyVisited_BlocksThatShouldNotBeKept);
            ret.insert(rec.begin(), rec.end());
        }
    }
    return ret;
}

void ParentWalker::CFGModel::BasicBlock::createAndLinkRexCFGNodes(ParentWalker& walker) {
    // A block may be created even if it contains no statements (i.e., the ENTRY block)
    head = tail = new RexNode(walker.generateCFGBlockID(walker.curFunc, block_, walker.curCFG.get()),
                                RexNode::CFG_BLOCK, true /* shouldKeep */);
    //bool isExit = walker.isStatic(walker.curFunc) || ((block_->getBlockID() != 0) && (block_ != &(walker.curCFG.get()->getEntry())));
    walker.recordNamedDeclLocation(walker.curFunc, head); // add filename
    //head->addSingleAttribute(TASchemeAttribute::FILENAME_DEFINITION, 
                    //walker.generateFileName(walker.curFunc));
    walker.addEdgeToGraph(walker.generateID(walker.curFunc), head->getID(), RexEdge::CONTAINS);
    walker.addNodeToGraph(head);
}

