////////////////////////////////////////////////////////////////////////////////////////////////////////
// ROSWalker.cpp
//
// Created By: Bryan J Muscedere
// Date: 07/04/17.
//
// Walks through Clang's AST using the full analysis
// mode methodology. This is achieved using the parent
// walker class to help obtain information about each
// AST node.
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

#include "ROSWalker.h"

#include <fstream>
#include <iostream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <clang/AST/Mangle.h>
#include <clang/Analysis/CFG.h>

#include "../Graph/RexNode.h"
#include "ROSConsumer.h"
#include "CondScope.h"
#include "RexID.h"
using namespace llvm;
using namespace clang;
using namespace std;

string printExpr2(const Expr *expr, const ASTContext *Context) {
    std::string expr_string;
    llvm::raw_string_ostream stream(expr_string);
    expr->printPretty(stream, nullptr, Context->getPrintingPolicy());
    stream.flush();
    return expr_string;
}

// debug code
// stmt is the statement you wish to print
string stmtToString(const Stmt *stmt){
    clang::LangOptions lo;
    string out_str;
    llvm::raw_string_ostream outstream(out_str);
    stmt->printPretty(outstream, NULL, PrintingPolicy(lo));
    return out_str;
}


const clang::Expr* simplifyExpr(const Expr* expr) {
    const Expr *simplifiedExpr;

    if (expr == nullptr) {
        return nullptr;
    }

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

clang::Expr* simplifyExpr(Expr* expr) {
    Expr *simplifiedExpr;

    if (expr == nullptr) {
        return nullptr;
    }

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

/**
 * Constructor
 * @param Context AST Context
 */
ROSWalker::ROSWalker(const WalkerConfig &config, ASTContext *Context) :
        ParentWalker{config, Context} {}

/**
 * Destructor
 */
ROSWalker::~ROSWalker() {}

/**
 * Visits statements for ROS components.
 * @param statement The statement to visit.
 * @return Whether we should continue.
 */
bool ROSWalker::VisitStmt(Stmt *statement) {
    if (isInSystemHeader(statement))
        return true;

    // extract topic names
    extractTopicVariable(statement);

    // First, handle ROS messages.
    handleROSstmt(statement);
    
    // Deal with call expressions.
    if (CallExpr *callExpr = dyn_cast<CallExpr>(statement)) {
        // Deal with function calls.
        bool isROSFunction = isAdvertise(callExpr) || isPublish(callExpr) || isSubscribe(callExpr) || isTimer(callExpr);
        recordCallExpr(callExpr, isROSFunction);
    }

    if (ReturnStmt *retStmt = dyn_cast<ReturnStmt>(statement)) {

        // filter out implicit returns
        if ((curFunc != nullptr) && (retStmt->getRetValue() != nullptr) &&
            (!curFunc->getReturnType().getTypePtr()->isVoidType())) {
            recordReturnStmt(retStmt);
            RexNode *retNode = new RexNode(generateRetID(curFunc), RexNode::RETURN, isInMainFile(getParentFunction(statement)));
            recordNamedDeclLocation(curFunc, retNode); //add filename to return node
            addNodeToGraph(retNode);
        }
    }

    // Deal with uses of control flow.
    if (IfStmt *ifStmt = dyn_cast<IfStmt>(statement)) {
        parentExpression.push_back(ifStmt->getCond());
    } else if (SwitchStmt *switchStmt = dyn_cast<SwitchStmt>(statement)) {
        parentExpression.push_back(switchStmt->getCond());
    } else if (ForStmt *forStmt = dyn_cast<ForStmt>(statement)) {
        parentExpression.push_back(forStmt->getCond());
    } else if (WhileStmt *whileStmt = dyn_cast<WhileStmt>(statement)) {
        parentExpression.push_back(whileStmt->getCond());
    }

    addWriteRelationships(statement, getParentFunction(statement));

    return true;
}

/**
 * Visits variable declarations.
 * @param decl The variable declaration to visit.
 * @return Whether we should continue.
 */
bool ROSWalker::VisitVarDecl(VarDecl *decl) {
    if (isInSystemHeader(decl))
        return true;

    // Record the variable declaration.
    recordVarDecl(decl);

    // Record ROS entity
    if (Expr* expr = decl->getInit()) {
        recordROSEntity(generateID(decl), expr, dyn_cast<Decl>(decl));
    }

    return true;
}

/**
 * Visits field declarations.
 * @param decl The variable declaration to visit.
 * @return Whether we should continue.
 */
bool ROSWalker::VisitFieldDecl(FieldDecl *decl) {
    if (isInSystemHeader(decl))
        return true;

    // Record the field declaration.
    recordFieldDecl(decl);

    // Record ROS entity
    if (Expr* expr = decl->getInClassInitializer()) {
        recordROSEntity(generateID(decl), expr, dyn_cast<Decl>(decl));
    }


    return true;
}

/**
 * Visits enum declarations.
 * @param decl The enum declaration to visit.
 * @return Whether we should continue.
 */
bool ROSWalker::VisitEnumDecl(EnumDecl* decl){
    if (isInSystemHeader(decl)) return true;

    //Record the enum declaration.
    recordEnumDecl(decl);

    return true;
}

/**
 * Visits function declarations.
 * @param decl The function declaration to visit.
 * @return Whether we should continue.
 */
bool ROSWalker::VisitFunctionDecl(FunctionDecl *decl) {

    if (isInSystemHeader(decl))
        return true;

    curFunc = decl;
    // Record the function declaration.
    recordFunctionDecl(decl);
    auto functionID = generateID(decl);

    // record location of function definition
    if (decl->doesThisDeclarationHaveABody()) {
        RexNode *functionNode = graph.findNode(functionID);
        recordNamedDeclLocation(decl, functionNode);
        
        for (const ParmVarDecl *param : decl->parameters()) {
            RexNode *paramNode = graph.findNode(generateID(param));
            if (paramNode == nullptr) {
                paramNode = new RexNode(generateID(param), RexNode::VARIABLE, true);
                addNodeToGraph(paramNode);
            }
            recordNamedDeclLocation(decl, paramNode);
        }
    }

    // CFG generation
    if (buildCFG && decl->doesThisDeclarationHaveABody()) {

        // Build the CFG
        auto stmt = decl->getBody();         

        curCFG = CFG::buildCFG(decl, stmt, Context, CFG::BuildOptions() /* no non-default options needed */ );
        if (!curCFG) {
            cerr << "Rex Warning: Could not build CFG for function. ";
            stmt->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
            cerr << endl;
            return true;
        }
        
        // used for debugging
        //curCFG->dump(LangOptions(), true);

        addEdgeToGraph(functionID, generateCFGEntryBlockID(decl), RexEdge::FUNCTION_CFG_LINK);

        cfgModel = std::make_unique<CFGModel>(*this);
        cfgModel->mapASTtoCFG();

        // Deals with Constructor initialization list
        if (isa<CXXConstructorDecl>(decl)) {
            const CXXConstructorDecl *cDecl = dyn_cast<CXXConstructorDecl>(decl);
            for (auto iter : cDecl->inits()) {
                CXXCtorInitializer *ctorInit = dyn_cast<CXXCtorInitializer>(iter);
                Expr *init = ctorInit->getInit(); 
                cfgModel->mapStmtToEntry(init);
            }
            
        }
    }

    return true;
}

/**
 * Visits struct/union/class declarations.
 * @param decl The struct/union/class declaration to visit.
 * @return Whether we should continue.
 */
bool ROSWalker::VisitCXXRecordDecl(CXXRecordDecl *decl) {
    if (isInSystemHeader(decl))
        return true;

    // Check what type of structure it is.
    if (decl->isClass()) {
        recordCXXRecordDecl(decl, RexNode::CLASS);
    } else if (decl->isStruct()) {
        recordCXXRecordDecl(decl, RexNode::STRUCT);
    } else if (decl->isUnion()) {
        recordCXXRecordDecl(decl, RexNode::UNION);
    }

    return true;
}

/**
 * Visits member expressions.
 * @param decl The member expression to visit.
 * @return Whether we should continue.
 */
bool ROSWalker::VisitMemberExpr(MemberExpr *memExpr) {
    if (isInSystemHeader(memExpr))
        return true;

    // Check if we have an if-statement noted here.
    if (usedInIfStatement(memExpr) || usedInLoop(memExpr)) {
        recordControlFlow(memExpr);
    }

    return true;
}

/**
 * Visits declaration reference expressions.
 * @param decl The declaration reference to visit.
 * @return Whether we should continue.
 */
bool ROSWalker::VisitDeclRefExpr(DeclRefExpr *declRef) {
    if (isInSystemHeader(declRef))
        return true;

    // Check if we have an if-statement noted here.
    if (usedInIfStatement(declRef) || usedInLoop(declRef)) {
        recordControlFlow(declRef);
    }

    return true;
}

bool ROSWalker::TraverseDecl(clang::Decl *d)  {
    if (BaselineWalker::TraverseDecl(d)) {
        // TraverseDecl on a FunctionDecl also performs a Clang traversal
        // of the function's entire body. So at this point, all facts in
        // the function have been linked to the CFG and the CFG may be added to the TA graph
        if (d && isa<FunctionDecl>(d)) {
            
            if (canBuildCFG()) {
                cfgModel->createAndLinkTAGraph();
                cfgModel.reset();
                curCFG.reset();

            }
        }
        return true;
    }
    return false;
}

void ROSWalker::extractTopicVariable(Stmt* stmt) {
    // extract default values for ros::param assignments
    if (CallExpr *callExpr = dyn_cast<CallExpr>(stmt)) {
        if (isFunction(callExpr, PARAM_FUNC) && callExpr->getNumArgs() >= 3) {
            string topicName = extractTopicName(callExpr->getArg(2));

            string varDeclStr = "";
            if (const MemberExpr *memExpr = dyn_cast<MemberExpr>(callExpr->getArg(1))) {
                if (CXXThisExpr *thisExpr = dyn_cast<CXXThisExpr>(memExpr->getBase())) {
                    varDeclStr = generateID(memExpr->getMemberDecl());
                } 
            } else if (const DeclRefExpr* declref = dyn_cast<DeclRefExpr>(simplifyExpr(callExpr->getArg(1)))) {
                varDeclStr = generateID(declref->getDecl());
            }

            if ((topicName != "") && (varDeclStr != "")) {
                varMap[varDeclStr] = topicName;
            }
        } else if (callExpr->getNumArgs() == 2) {
            string topicName = extractTopicName(callExpr->getArg(1));

            string varDeclStr = "";
            if (const MemberExpr *memExpr = dyn_cast<MemberExpr>(callExpr->getArg(0))) {
                if (CXXThisExpr *thisExpr = dyn_cast<CXXThisExpr>(memExpr->getBase())) {
                    varDeclStr = generateID(memExpr->getMemberDecl());
                } 
            } else if (const DeclRefExpr* declref = dyn_cast<DeclRefExpr>(simplifyExpr(callExpr->getArg(0)))) {
                varDeclStr = generateID(declref->getDecl());
            }

            if ((topicName != "") && (varDeclStr != "")) {
                varMap[varDeclStr] = topicName;
            }
        }
    } else if (DeclStmt *declStmt = dyn_cast<DeclStmt>(stmt)) {
        for (auto it = declStmt->decl_begin(); it != declStmt->decl_end(); it++) {
            auto decl = dyn_cast<VarDecl>(*it);
            if (!decl) // does not declare a variable
                continue;

            Expr *initializer = decl->getInit();
            if (!initializer)
                continue;

            string declID = generateID(decl);
            string topicName = extractTopicName(initializer);
            if ((topicName != "") && (declID != "")) {
                varMap[declID] = topicName;
            }
        }
    } 
}

/**
 * Helper method for the two walkers that handles statements. This is done
 * to avoid repeats in both walkers.
 * @param statement The statement to analyze.
 */
void ROSWalker::handleROSstmt(Stmt *statement) {

    recordROSEntity(statement);
    recordROSEdge(statement);
    
}

/**
 * Checks if we're constructing a subscriber.
 * @param ctor The CXX constructor.
 * @return Whether its a subscriber.
 */
bool ROSWalker::isSubscriberObj(const CXXConstructExpr *ctor) {
    return isClass(ctor, SUBSCRIBER_CLASS);
}

/**
 * Checks if we're constructing a publisher.
 * @param ctor The CXX constructor.
 * @return Whether its a publisher.
 */
bool ROSWalker::isPublisherObj(const CXXConstructExpr *ctor) {
    return isClass(ctor, PUBLISHER_CLASS);
}

bool ROSWalker::isTimerObj(const CXXConstructExpr *ctor) {
    return isClass(ctor, TIMER_CLASS);
}

/**
 * Checks if we're making a publish call.
 * @param ctor The CXX constructor.
 * @return Whether its a publish call.
 */
bool ROSWalker::isPublish(const CallExpr *expr) {
    return isFunction(expr, PUBLISH_FUNCTION);
}

/**
 * Checks if we're making a subscribe call.
 * @param ctor The CXX constructor.
 * @return Whether its a subscribe call.
 */
bool ROSWalker::isSubscribe(const CallExpr *expr) {
    return isFunction(expr, SUBSCRIBE_FUNCTION);
}

/**
 * Checks if we're making an advertise call.
 * @param ctor The CXX constructor.
 * @return Whether its an advertise call.
 */
bool ROSWalker::isAdvertise(const CallExpr *expr) {
    return isFunction(expr, ADVERTISE_FUNCTION);
}

bool ROSWalker::isTimer(const CallExpr *expr) {
    return isFunction(expr, TIMER_FUNCTION);
}

RexNode* ROSWalker::recordCallback(const FunctionDecl *funcDecl, RexNode *topic, 
        const CallExpr *expr, bool isDef) {
    // Extract callback function
    RexNode *callbackFirstArg = nullptr;
    string callbackID = generateID(funcDecl);
    RexNode *callback = graph.findNode(callbackID);

    if (callback == nullptr) {
        recordFunctionDecl(funcDecl);
        callback = graph.findNode(callbackID); 
        assert(callback);
    }

    string callbackName = funcDecl->getQualifiedNameAsString();

    // Get the arguments of the function
    int numParams = funcDecl->getNumParams();
    if (numParams >= 1) {
        // Get the first argument of the callback declaration
        const ParmVarDecl *paramDecl = funcDecl->getParamDecl(0);
        string paramID = generateID(paramDecl);
        callbackFirstArg = graph.findNode(paramID);

        if (callbackFirstArg == nullptr) {
            recordVarDecl(dyn_cast<VarDecl>(paramDecl));
            callbackFirstArg = graph.findNode(paramID);
            assert(callbackFirstArg);
        }

    }

    if (!callback) {
        if (!isDef) {
            cerr << "Rex Warning: could not determine callback for call to subscribe. ";
            expr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
            cerr << endl;   
        }
        return nullptr;
    }

    if (!callbackFirstArg) {
        if (!isDef) {
            cerr << "Rex Warning: unsupported callback argument in call to subscribe. ";
            expr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
            cerr << endl;
        }
        return nullptr; // Unsupported call to subscribe
    }

    // mark function as callback
    callback->addSingleAttribute(TASchemeAttribute::ROS_IS_CALLBACK, "1");

    // Record that the callback's argument is the target of a publish. The word
    // "target" refers to the fact that publish(var) will write `var` to this
    // variable (i.e. publish will "target" this variable to write `var`).
    RexEdge *pubTarget = new RexEdge(topic, callbackFirstArg, RexEdge::PUBLISH_TARGET);
    addEdgeToGraph(pubTarget);

    if (canBuildCFG()) {
        // Record pubTarget CFG edges
        std::string pubTargetCFGID = generateCFGEntryBlockID(funcDecl);
        addOrUpdateEdge(topic->getID(), pubTargetCFGID, RexEdge::PT_SOURCE);
        addOrUpdateEdge(callbackFirstArg->getID(), pubTargetCFGID, RexEdge::PT_DESTINATION);

        // Record source CFG edge of subscriber - call -> callback function edge
        addOrUpdateEdge(callback->getID(), pubTargetCFGID, RexEdge::C_DESTINATION);
    }


    return callback;
}

/**
 * Records a subscriber object.
 * @param expr The expression with the subscriber.
 */
void ROSWalker::recordSubscribe(const CallExpr *expr) {
    // We only support the forms of subscribe with >= 3 arguments where the
    // first argument is a string literal, the second argument is a number, and
    // the third argument is a callback function (function pointer).
    // We also optionally match the type of the message to be sent across the
    // topic. This is usually passed as a type parameter.
    //
    // More concretely, we support at minimum:
    // * nodeHandle.subscribe("string-topic-name", 1000, callback);
    // * nodeHandle.subscribe(nodeHandle.param(key, std::string("string-topic-name")), 1000, callback)

    // Now, adds information to the destination node.
    //expr->dump(llvm::outs(), Context->getSourceManager());

    int numArgs = expr->getNumArgs();
    if (numArgs < 3) {
        cerr << "Rex Warning: Ignoring call to subscribe with less than 3 arguments. ";
        expr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
        cerr << endl;
        return; // Unsupported call to subscribe
    }

    const Expr *topicArg = expr->getArg(0);
    string topicName = extractTopicName(topicArg);
    if (topicName.empty()) {
        cerr << "Rex Warning: topic names that are not string literals are not supported. ";
        expr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
        cerr << endl;
        return; // Unsupported call to subscribe
    }

    /**const Expr *queueSizeArg = expr->getArg(1);
    string queueSize = extractQueueSize(queueSizeArg);
    if (queueSize.empty()) {
        cerr << "Rex Warning: queue sizes that are not integer literals are not supported. ";
        expr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
        cerr << endl;
        return; // Unsupported call to subscribe
    }**/

    // Get topic Nodes
    const NamedDecl *parentDecl = getParent<CallExpr>(expr);
    // Record the topic name if it is not present
    string topicID = static_cast<string>(RexTopicID(topicName));
    if (parentDecl) {
        string qualifiedPrefix = generateName(parentDecl) + "::subscribe::";
        recordTopic(topicID, qualifiedPrefix + topicName, topicArg);
    } else {
        recordTopic(topicID, topicName, topicArg);
    }
    RexNode *topic = graph.findNode(topicID);

    // Attempt to find the callback and first argument
    RexNode *callback = nullptr;
    RexNode *callbackDef = nullptr;
    string callbackName;
    //RexNode *callbackFirstArg = nullptr;
    //RexNode *callbackFirstArgDef = nullptr;
    const Expr *callbackArg = simplifyExpr(expr->getArg(2));
    // Unwrap any surrounding unary expression (e.g. the address operator `&`)
    if (const auto *unary = dyn_cast<UnaryOperator>(callbackArg)) {
        callbackArg = unary->getSubExpr();
    }

    // Only process callback if the argument is a variable
    const FunctionDecl *func;
    if (const auto *declref = dyn_cast<DeclRefExpr>(callbackArg)) {
        // Get the function declaration of the callback
        const ValueDecl *vardecl = declref->getDecl();
        if (const auto *funcdecl = dyn_cast<FunctionDecl>(vardecl)) {

            func = funcdecl;

             // attempts to get the definition of funcDecl
            // some declarations does not have parameter names
            if (const FunctionDecl *tempFunc = funcdecl->getDefinition()) {
                callbackDef = recordCallback(tempFunc, topic, expr, true);
            }

            callback = recordCallback(funcdecl, topic, expr, false);

        }
    }

    if (callback == nullptr) {
        return;
    }

    // Record subscribe edge
	RexNode* currentSubscriber = graph.findNode(subStack.top());
    RexEdge *topEdge = new RexEdge(topic, currentSubscriber, RexEdge::SUBSCRIBE);
    addEdgeToGraph(topEdge);

    if (canBuildCFG()) {
        std::string callbackCFGID = generateCFGEntryBlockID(func);
        addOrUpdateEdge(topic->getID(), callbackCFGID, RexEdge::SUB_SOURCE);
        addOrUpdateEdge(currentSubscriber->getID(), callbackCFGID, RexEdge::SUB_DESTINATION);

        if (callbackDef) {
            std::string callbackDefCFGID = generateCFGEntryBlockID(func);
            addOrUpdateEdge(topic->getID(), callbackDefCFGID, RexEdge::SUB_SOURCE);
            addOrUpdateEdge(currentSubscriber->getID(), callbackDefCFGID, 
                RexEdge::SUB_DESTINATION);
        }
    }

    // Record attributes for subscriber.
    //currentSubscriber->addSingleAttribute(TASchemeAttribute::ROS_TOPIC_BUFFER_SIZE, queueSize);
    currentSubscriber->addSingleAttribute(TASchemeAttribute::ROS_NUM_ATTRIBUTES, to_string(numArgs));
    currentSubscriber->addSingleAttribute(TASchemeAttribute::ROS_CALLBACK, callbackName);

    // Finally, adds in the callback function.
    //callback->addSingleAttribute(TASchemeAttribute::ROS_IS_CALLBACK, "1");
    RexEdge *callbackEdge = new RexEdge(currentSubscriber, callback, RexEdge::CALLS);
    addEdgeToGraph(callbackEdge);

    if (callbackDef) {
        RexEdge *callbackDefEdge = new RexEdge(currentSubscriber, callbackDef, RexEdge::CALLS);
        addEdgeToGraph(callbackDefEdge);
    }

    if (canBuildCFG()) {
        std::string pubTargetCFGID = generateCFGEntryBlockID(func);
        addOrUpdateEdge(currentSubscriber->getID(), pubTargetCFGID, RexEdge::C_SOURCE);
    }
}

/**
 * Records a publisher object.
 * @param expr The publisher object.
 */
void ROSWalker::recordPublish(const CallExpr *expr, string cfgID) {
    // Get the publisher object.
    auto parent = getParentVariable(expr);
    if (!parent)
        return;
    RexNode *parentVar = graph.findNode(generateID(parent));
    if (!parentVar)
        return;

    // Next, gets the topic it publishes to.
    const vector<RexEdge *> &destinations = graph.findEdgesBySrcID(parentVar->getID());
    RexNode *topic = nullptr;

    if (destinations.empty()) {
        return;
    }


    for (unsigned int i = 0; i < destinations.size(); i++) {
        auto item = destinations.at(i);

        // Narrows the item.
        if (item->getType() != RexEdge::ADVERTISE)
            continue;
        topic = item->getDestination();
    }

    if (!topic) {
        return;
    }

    // get parent function
    auto callingFunc = getParentFunction(expr);
    RexNode *funcNode = graph.findNode(generateID(callingFunc));

    // Now, generates the edge.
    RexEdge *publish = new RexEdge(parentVar, topic, RexEdge::PUBLISH);

    if (canBuildCFG() || !cfgID.empty()) {
        std::string publishCFGID = cfgID;
        if (cfgID.empty())
            publishCFGID = cfgModel->getCFGNodeID(expr);
        addOrUpdateEdge(parentVar->getID(), publishCFGID, RexEdge::PUB_SOURCE);
        addOrUpdateEdge(topic->getID(), publishCFGID, RexEdge::PUB_DESTINATION);
    }

    // Gets the publish data.
    auto args = getArgs(expr);
    string data = args.at(0);
    if (data.size() > PUB_MAX)
        data = data.substr(0, PUB_MAX);
    publish->addSingleAttribute(TASchemeAttribute::ROS_PUBLISHER_DATA, data);

    // Adds the result to the graph.
    addEdgeToGraph(publish);

    // Add an edge that communicates which variable was published
    const Expr *arg = expr->getArg(0);

    // Unwrap any surrounding implicit cast
    if (const auto *implcast = dyn_cast<ImplicitCastExpr>(arg)) {
        arg = implcast->getSubExpr();
    }
    if (!topic)
        return;

    arg = simplifyExpr(arg);

    // Unwrap any surrounding unary expression (e.g. the address operator `&`)
    if (const auto *unary = dyn_cast<UnaryOperator>(arg)) {
        arg = simplifyExpr(unary->getSubExpr());
    }
    

    string pubArgID;
    if (const auto *memexpr = dyn_cast<MemberExpr>(arg)) {
        pubArgID = generateID(memexpr);
    } else if (const auto *declref = dyn_cast<DeclRefExpr>(arg)) {
        // Get the original declaration of the variable that is referenced
        const ValueDecl *vardecl = declref->getDecl();
        pubArgID = generateID(vardecl);
    } else if (const NamedDecl* parentDecl = getParentVariable(arg)) {
        pubArgID = generateID(parentDecl);
    } else {
        //arg->dump(llvm::outs(), Context->getSourceManager());
        cerr << "Rex Warning: Unable to determine variable passed to publish. ";
        expr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
        cerr << endl;
        return;
    }

    // take care of cases where variable is declared in header file
    if (!graph.findNode(pubArgID)) {
        RexNode *pubArgNode = new RexNode(pubArgID, RexNode::VARIABLE, true);
        addNodeToGraph(pubArgNode);
    }
    
    RexEdge *pubVar = new RexEdge(pubArgID, topic, RexEdge::PUBLISH_VARIABLE);
    addEdgeToGraph(pubVar);

    addOrUpdateEdge(funcNode->getID(), parentVar->getID(), RexEdge::CALLS);

    if (canBuildCFG() || !cfgID.empty()) {
        std::string pubVarCFGID = cfgID;
        if (cfgID.empty())
            pubVarCFGID = cfgModel->getCFGNodeID(expr);
        addOrUpdateEdge(pubArgID, pubVarCFGID, RexEdge::PV_SOURCE);
        addOrUpdateEdge(topic->getID(), pubVarCFGID, RexEdge::PV_DESTINATION);

        // map CFG nodes for cFunction - call -> rosPublisher
        addOrUpdateEdge(funcNode->getID(), pubVarCFGID, RexEdge::C_SOURCE);
        addOrUpdateEdge(parentVar->getID(), pubVarCFGID, RexEdge::C_DESTINATION);
    }
}

/**
 * Records an advertise call.
 * @param expr The call with the advertise.
 */
void ROSWalker::recordAdvertise(const CallExpr *expr) {
    // We only support the forms of advertise with >= 2 arguments where the
    // first argument is a string literal and the second argument is a number.
    // We also optionally match the type of the message to be sent across the
    // topic. This is usually passed as a type parameter.
    //
    // More concretely, we support at minimum:
    // * nodeHandle.advertise("string-topic-name", 1000);
    // * nodeHandle.advertise<MessageType>("string-topic-name", 1000);

    int numArgs = expr->getNumArgs();
    if (numArgs < 2) {
        cerr << "Rex Warning: Ignoring call to advertise with less than 2 arguments. ";
        expr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
        cerr << endl;
        return; // Unsupported call to advertise
    }

    const Expr *topicArg = expr->getArg(0);
    string topicName = extractTopicName(topicArg);
    if (topicName.empty()) {
        cerr << "Rex Warning: topic names that are not string literals are not supported. ";
        expr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
        cerr << endl;
        return; // Unsupported call to advertise
    }

    const Expr *queueSizeArg = expr->getArg(1);
    /**string queueSize = extractQueueSize(queueSizeArg);
    if (queueSize.empty()) {
        cerr << "Rex Warning: queue sizes that are not integer literals are not supported. ";
        expr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
        cerr << endl;
        return; // Unsupported call to advertise
    }**/

    const NamedDecl *parentDecl = getParent<CallExpr>(expr);
    string topicID = static_cast<string>(RexTopicID(topicName));
    if (parentDecl) {
        string qualifiedPrefix = generateName(parentDecl) + "::advertise::";
        recordTopic(topicID, qualifiedPrefix + topicName, topicArg);
    } else {
        recordTopic(topicID, topicName, topicArg);
    }

	RexNode* currentPublisher = graph.findNode(pubStack.top());
    RexNode *topic = graph.findNode(static_cast<string>(RexTopicID(topicName)));
    RexEdge *topEdge = new RexEdge(currentPublisher, topic, RexEdge::ADVERTISE);
    addEdgeToGraph(topEdge);

    // Record specific attributes.
    //currentPublisher->addSingleAttribute(TASchemeAttribute::ROS_TOPIC_BUFFER_SIZE, queueSize);
    currentPublisher->addSingleAttribute(TASchemeAttribute::ROS_NUM_ATTRIBUTES, to_string(numArgs));
    currentPublisher->addSingleAttribute(TASchemeAttribute::ROS_PUBLISHER_TYPE, getPublisherType(expr));

    currentPublisher = nullptr;
}


/**
 * Gets the type of publisher from the advertise call
 * @param expr The expression of the publisher.
 * @return The type of publisher.
 */
string ROSWalker::getPublisherType(const CallExpr *expr) {
    string type;
    ArrayRef<Stmt*> subExprs = const_cast<CallExpr*>(expr)->getRawSubExprs();
    for(auto iter = subExprs.begin(); iter != subExprs.end(); iter++)
    {
		if(auto memberExpr = dyn_cast<MemberExpr>(*iter))
		{
			type = extractTemplateType(memberExpr)[0];
			break;
		}
	}
    return type;
}

/**
 * Records a ROS timer from a call expression.
 * @param expr The call expression to record.
 */
void ROSWalker::recordTimer(const CallExpr *expr) {

    int numArgs = expr->getNumArgs();
    if (numArgs < 4) {
        cerr << "Rex Warning: Ignoring call to createTimer with less than 4 arguments. ";
        expr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
        cerr << endl;
        return; // Unsupported call to createTimer
    }

    // First, get the arguments.
    auto timerArgs = getArgs(expr);

    // Gets the duration.
    string timerFreq = timerArgs[0];
    if (boost::starts_with(timerFreq, TIMER_PREFIX_1)) {
        boost::replace_all(timerFreq, TIMER_PREFIX_1, "");
        boost::replace_all(timerFreq, ")", "");
    } else if (boost::starts_with(timerFreq, TIMER_PREFIX_2)) {
        boost::replace_all(timerFreq, TIMER_PREFIX_2, "");
        boost::replace_all(timerFreq, ")", "");
    }

    // Checks if we have a one shot timer.
    string oneshot = timerArgs[3];
    if (oneshot == "" || oneshot == "false") {
        oneshot = "0";
    } else {
        oneshot = "1";
    }

	RexNode* currentTimer = graph.findNode(timerStack.top());
    // Adds specific attributes.
    currentTimer->addSingleAttribute(TASchemeAttribute::ROS_TIMER_DURATION, timerFreq);
    currentTimer->addSingleAttribute(TASchemeAttribute::ROS_TIMER_IS_ONESHOT, oneshot);

    // Attempt to find the callback
    RexNode *callback = nullptr;
    const Expr *callbackArg = expr->getArg(1);
    // Unwrap any surrounding unary expression (e.g. the address operator `&`)
    if (const auto *unary = dyn_cast<UnaryOperator>(callbackArg)) {
        callbackArg = unary->getSubExpr();
    }

    // Only process callback if the argument is a variable
    if (const auto *declref = dyn_cast<DeclRefExpr>(callbackArg)) {
        // Get the function declaration of the callback
        const ValueDecl *vardecl = declref->getDecl();
        if (const auto *funcdecl = dyn_cast<FunctionDecl>(vardecl)) {
            // Find the node for this callback
            string callbackID = generateID(funcdecl);
            callback = graph.findNode(callbackID);
        }
    }

    if (!callback) {
        cerr << "Rex Warning: could not determine callback for call to createTimer. ";
        expr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
        cerr << endl;
        return; // Unsupported call to createTimer
    }

    // Finally, adds in the callback function.
    RexEdge *callbackEdge = new RexEdge(currentTimer, callback, RexEdge::SET_TIME);
    addEdgeToGraph(callbackEdge);

    //TODO: Davood we need to deal with this or else this variable won't get
    // reset properly at any of the early returns above
    currentTimer = nullptr;
}


// Extracts the topic name from the Expr for the first argument to either
// advertise or subscribe.
//
// Returns "" if no supported form of the topic name was matched
string ROSWalker::extractTopicName(const Expr *topicArg) {
    // We only support string literals for this argument, but matching it is
    // actually quite complicated.
    //
    // The AST for just the string literal argument looks something like this:
    //
    // MaterializeTemporaryExpr 'const std::string':'const class std::__cxx11::basic_string<char>' lvalue
    // | `-CXXBindTemporaryExpr 'const std::string':'const class std::__cxx11::basic_string<char>'
    // |   `-CXXConstructExpr 'const std::string':'const class std::__cxx11::basic_string<char>' 'void (const char *, const class std::allocator<char> &)'
    // |     |-ImplicitCastExpr 'const char *' <ArrayToPointerDecay>
    // |     | `-StringLiteral 'const char [15]' lvalue "/vehicle_state"
    // |     `-CXXDefaultArgExpr 'const class std::allocator<char>':'const class std::allocator<char>' lvalue
    //
    // All we care about from this is the StringLiteral.

    // This is not the most elegant code, but it works...

    // handles special case in WISE-ADS that uses nh.param("key", std::string("topic-name"))
    topicArg = simplifyExpr(topicArg);
    if (const CXXMemberCallExpr *tempCE = dyn_cast<CXXMemberCallExpr>(topicArg)) {
        if (tempCE->getNumArgs() > 1) {
            topicArg = simplifyExpr(dyn_cast<Expr>(tempCE->getArg(1)));
        }
    } else if (const MemberExpr *tempCE = dyn_cast<MemberExpr>(topicArg)) {
        if (const CXXThisExpr *thisExpr = dyn_cast<CXXThisExpr>(tempCE->getBase())) {
            return varMap[generateID(tempCE->getMemberDecl())];
        }
    } else if (const DeclRefExpr* declref = dyn_cast<DeclRefExpr>(topicArg)) {
        return varMap[generateID(declref->getDecl())];
    } else if (const clang::StringLiteral *topicStrLit = dyn_cast<clang::StringLiteral>(topicArg)) {
        auto ret = topicStrLit->getString();
        if ((ret.str().length() > 0) && (ret[0] == '/'))
            return ret.substr(1);
        return ret;
    }

    /**const MaterializeTemporaryExpr *mattempexpr;
    if (!(mattempexpr = dyn_cast<MaterializeTemporaryExpr>(topicArg))) {
        return "";
    }

    const CXXBindTemporaryExpr *bindtempexpr;
    if (!(bindtempexpr = dyn_cast<CXXBindTemporaryExpr>(mattempexpr->GetTemporaryExpr()))) {
        return "";
    }**/

    
    const CXXConstructExpr *constructexpr;
    if (!(constructexpr = dyn_cast<CXXConstructExpr>(topicArg))) {
        return "";
    }
    if (constructexpr->getNumArgs() < 1) {
        return "";
    }
    //constructexpr->dump(llvm::outs(), Context->getSourceManager());

    const ImplicitCastExpr *implcast;
    const Expr *constArg = constructexpr->getArg(0);
    if (!(implcast = dyn_cast<ImplicitCastExpr>(constArg))) {
        //simplifyExpr(constructexpr->getArg(0))->dump(llvm::outs(), Context->getSourceManager());
        if (isa<CXXConstructExpr>(simplifyExpr(constArg))) {
            return extractTopicName(simplifyExpr(constArg));
        }
        return "";
    }


    const clang::StringLiteral *topicStrLit;
    if (!(topicStrLit = dyn_cast<clang::StringLiteral>(implcast->getSubExpr()))) {
        return "";
    }

    auto ret = topicStrLit->getString();
    if ((ret.str().length() > 0) && (ret[0] == '/'))
        return ret.substr(1);
    return ret;
}

// Extracts the queue size (as a string) from the Expr for the second argument
// to either advertise or subscribe.
//
// Returns "" if no supported form of the queue size was matched
string ROSWalker::extractQueueSize(const Expr *queueSizeArg) {
    // Unwrap any surrounding implicit cast
    if (const auto *implcast = dyn_cast<ImplicitCastExpr>(queueSizeArg)) {
        queueSizeArg = implcast->getSubExpr();
    }

    //queueSizeArg->dump(llvm::outs(), Context->getSourceManager());
    if (const auto *sizeLit = dyn_cast<IntegerLiteral>(queueSizeArg)) {
        string queueSize = "";
        llvm::raw_string_ostream strStream(queueSize);
        sizeLit->printPretty(strStream, nullptr, Context->getPrintingPolicy());
        return strStream.str();
    }
    return "";
}


/**
 * Records a topic.
 * @param name The name of the topic.
 */
void ROSWalker::recordTopic(string id, string name, const Expr *topicArg) {

    if (graph.hasNode(id)) {
        return;
    }

    // Create the node.
    RexNode *node = new RexNode(id, RexNode::TOPIC, isInMainFile(topicArg));
    node->addSingleAttribute(TASchemeAttribute::LABEL, name);
    addNodeToGraph(node);
}

/**
 * Record a ROS Entity/Node in the factbase if it doesn't already exist.
 * */
void ROSWalker::recordROSEntity(string declID, Expr *expr, Decl *decl) 
{
    string type = expr->getType().getAsString(Context->getPrintingPolicy());

    if (type == PUBLISHER_CLASS) {
        RexNode *assigneeNode = graph.findNode(declID);
        if(!assigneeNode) {
			assigneeNode = new RexNode(declID, RexNode::PUBLISHER, isInMainFile(decl));
			addNodeToGraph(assigneeNode);
		}
		pubStack.push(declID);
    } else if (type == SUBSCRIBER_CLASS) {
        RexNode *assigneeNode = graph.findNode(declID);
        if(!assigneeNode) {
			assigneeNode = new RexNode(declID, RexNode::SUBSCRIBER, isInMainFile(decl));
			addNodeToGraph(assigneeNode);
		}
		subStack.push(declID);
    } else if (type == TIMER_CLASS) {
        RexNode *assigneeNode = graph.findNode(declID);
        if(!assigneeNode) {
			assigneeNode = new RexNode(declID, RexNode::TIMER, isInMainFile(decl));
			addNodeToGraph(assigneeNode);
		}
		timerStack.push(declID);
    }
    

}

/**
 * Record a ROS Entity/Node in the factbase if it doesn't already exist.
 * */
void ROSWalker::recordROSEntity(Stmt* statement)
{

    // simplify expression
    Expr* expr = simplifyExpr(dyn_cast<Expr>(statement));
    if (expr == nullptr) {
        return;
    }
    
    if (CXXOperatorCallExpr* assignExpr = dyn_cast<CXXOperatorCallExpr>(expr)){
        recordNonStaticROSEntity(assignExpr, statement);
    } else if (CXXConstructExpr* constrExpr = dyn_cast<CXXConstructExpr>(expr)) {
        recordStaticROSEntity(constrExpr, statement);
    } 

}

/**
 * Records a non-static ROS entity/node in the factbase if it doesn't exist already.
 * Non-Static in this sense refers to  the ROS variable being non-static
 * */
void ROSWalker::recordNonStaticROSEntity(CXXOperatorCallExpr* assignExpr, Stmt* statement)
{
    string type = assignExpr->getType().getAsString(Context->getPrintingPolicy());

    if (assignExpr->getNumArgs() > 1) {
        const Expr *lhs = assignExpr->getArg(0);
        string type1 = lhs->getType().getAsString(Context->getPrintingPolicy());

        std::string prefix("std::map");
        if (!type1.compare(0, prefix.size(), prefix)) {
            return;
        }
    }

	if (type == PUBLISHER_CLASS){
		const NamedDecl* assignee = getAssignee(assignExpr);
		const MemberExpr* assignStmt = getAssignStmt(assignExpr);
		if (!assignee || !assignStmt) return;
		string assigneeID = generateID(assignee);
		RexNode *assigneeNode = graph.findNode(assigneeID);
		if(!assigneeNode)
		{
			assigneeNode = new RexNode(assigneeID, RexNode::PUBLISHER, isInMainFile(assignee));
			addNodeToGraph(assigneeNode);
		}
		// We nedd to maintain a stack of publishers (by their ID)
		// for recording advertise calls and associated control dependencies.
		// See ROSWalker::recordROSEdge for more details
        //cout << assigneeID << endl;
        //cout << stmtToString(statement) << endl;
		pubStack.push(assigneeID);
		// I'm not sure we need this here
		recordParentFunction(statement, assigneeNode);
	} else if (type == SUBSCRIBER_CLASS) {
		const NamedDecl *assignee = getAssignee(assignExpr);
		const MemberExpr *assignStmt = getAssignStmt(assignExpr);
		if (!assignee || !assignStmt) return;
		string assigneeID = generateID(assignee);
		RexNode *assigneeNode = graph.findNode(assigneeID);
		if(!assigneeNode)
		{
			assigneeNode = new RexNode(assigneeID, RexNode::SUBSCRIBER, isInMainFile(assignee));
			addNodeToGraph(assigneeNode);
		}
		// We nedd to maintain a stack of subscribers (by their ID)
		// for recording subscribe calls and associated control dependencies
		// See ROSWalker::recordROSEdge for more details
		subStack.push(assigneeID);
		// I'm not sure we need this here
		recordParentFunction(statement, assigneeNode);
	} else if (type == TIMER_CLASS) {
		const NamedDecl *assignee = getAssignee(assignExpr);
		const MemberExpr *assignStmt = getAssignStmt(assignExpr);
		if (!assignee || !assignStmt) return;
		string assigneeID = generateID(assignee);
		RexNode *assigneeNode = graph.findNode(assigneeID);
		if(!assigneeNode)
		{
			assigneeNode = new RexNode(assigneeID, RexNode::TIMER, isInMainFile(assignee));
			addNodeToGraph(assigneeNode);
		}
		// We nedd to maintain a stack of timers (by their ID)
		// for recording time calls and associated control dependencies
		// See ROSWalker::recordROSEdge for more details
		timerStack.push(assigneeID);
		// I'm not sure we need this here
		recordParentFunction(statement, assigneeNode);
	}

}
/**
 * Records a static ROS entity/node in the factbase if it doesn't exist already.
 * Static in this sense refers to the ROS variable being static
 * */
void ROSWalker::recordStaticROSEntity(CXXConstructExpr* constrExpr, Stmt* statement)
{
	if (isSubscriberObj(constrExpr)) {
		const NamedDecl* assignee = getParent<CXXConstructExpr>(constrExpr);
		string assigneeID = generateID(assignee);
		RexNode *assigneeNode = graph.findNode(assigneeID);
		if(!assigneeNode)
		{
			assigneeNode = new RexNode(assigneeID, RexNode::PUBLISHER, isInMainFile(assignee));
			addNodeToGraph(assigneeNode);
		}
		// We need to maintain a stack of subscribers (by their ID)
		// for recording subscribe calls and control dependencies
		// See ROSWalker::recordROSEdge for more details
		subStack.push(assigneeID);
		// I'm not sure we need this here
		recordParentFunction(statement, assigneeNode);

	} else if (isPublisherObj(constrExpr)) {
		const NamedDecl* assignee = getParent<CXXConstructExpr>(constrExpr);
		string assigneeID = generateID(assignee);
		RexNode *assigneeNode = graph.findNode(assigneeID);
		if(!assigneeNode)
		{
			assigneeNode = new RexNode(assigneeID, RexNode::PUBLISHER, isInMainFile(assignee));
			addNodeToGraph(assigneeNode);
		}
		// We need to maintain a stack of publishers (by their ID)
		// for recording advertise calls and associated control dependencies.
		// See ROSWalker::recordROSEdge for more details
		pubStack.push(assigneeID);
		// I'm not sure we need this here
		recordParentFunction(statement, assigneeNode);
	} else if (isTimerObj(constrExpr)){
		const NamedDecl* assignee = getParent<CXXConstructExpr>(constrExpr);
		string assigneeID = generateID(assignee);
		RexNode *assigneeNode = graph.findNode(assigneeID);
		if(!assigneeNode)
		{
			assigneeNode = new RexNode(assigneeID, RexNode::PUBLISHER, isInMainFile(assignee));
			addNodeToGraph(assigneeNode);
		}
		// We need to maintain a stack of timers (by their ID)
		// for recording timerr calls and associated control dependencies.
		// See ROSWalker::recordROSEdge for more details
		timerStack.push(assigneeID);
		// I'm not sure we need this here
		recordParentFunction(statement, assigneeNode);
	}

}

/**
 * Records either an advertise, publish, subscribe, or time
 * relation to the factbase. In addition, the varInfluence
 * relation for these calls are also recorded assuming the calls
 * are enclosed in a control structure
 * */
void ROSWalker::recordROSEdge(Stmt* statement)
{
	//This handles the publish/subscribe/advertise/time calls
    if (CallExpr* callExpr = dyn_cast<CallExpr>(statement)){
        if (isPublish(callExpr)) {
            // Given the structure of the ROS framework, we only need
            // the callExpr to extract the publisher, since publish() is
            // a member function for a publisher
            NamedDecl* pub_decl = getParentVariable(callExpr);
            if(pub_decl)
            {
                RexNode* publisher = graph.findNode(generateID(pub_decl));
                // This is in case we Visit a call to publish() before
                // we record the publisher
                if(!publisher)
                {
                    publisher = new RexNode(generateID(pub_decl), RexNode::PUBLISHER, isInMainFile(pub_decl));
                    addNodeToGraph(publisher);
                } else {
                    string cfgID = "";
                    if (canBuildCFG())
                        cfgID = cfgModel->getCFGNodeID(callExpr);
                    publishExprs[publisher->getID()].push_back(make_pair(callExpr,cfgID));
                }
                recordASTControl(statement, publisher);
                recordPublish(callExpr);
            }
            else
            {
                cerr << "Rex Warning: Ignoring call to publish";
                callExpr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
                cerr << endl;
            }

        } else if (isSubscribe(callExpr)) {
			// Given the structure of the ROS framework, we need to keep
			// track of a stack of subscribers (by their ID), since
			// the subscribe() call is a member function for a NodeHandle
			if (!subStack.empty()) {
                RexNode* subscriber = graph.findNode(subStack.top());
                recordASTControl(statement, subscriber);
                recordSubscribe(callExpr);
                subStack.pop();
            }
        } else if (isAdvertise(callExpr)) {
            // Given the structure of the ROS framework, we need to keep
            // track of a stack of publishers (by their ID), since
            // the advertise() call is a member function for a NodeHandle
            string pubName = "";
            if(!pubStack.empty())
            {
                pubName = pubStack.top();
                RexNode* publisher = graph.findNode(pubStack.top());
                recordASTControl(statement, publisher);
                recordAdvertise(callExpr);
                pubStack.pop();
            }
            else
            {
                cerr << "Rex Warning: Ignoring call to advertise";
                callExpr->getBeginLoc().print(llvm::errs(), Context->getSourceManager());
                cerr << endl;
            }

            if (publishExprs.count(pubName)) {
                for (auto &publishExpr : publishExprs[pubName]) {
                    recordPublish(publishExpr.first, publishExpr.second);
                }
                publishExprs.erase(pubName);
            }

        } else if (isTimer(callExpr)) {
            recordTimer(callExpr);
            timerStack.pop();
        }
    }
}

// ROS entities need to match ROS special types (publishers, subscribers, etc.)
RexNode *ROSWalker::chooseVariableNodeType(const ValueDecl *decl, bool shouldKeep) {
    const CXXRecordDecl *varType = decl->getType()->getAsCXXRecordDecl();
    string varTypeName = (varType) ? varType->getQualifiedNameAsString() : "";

    string id;
    RexNode::NodeType type;
    if (varTypeName == PUBLISHER_CLASS) {
        id = generateID(decl); //TODO: change this
        type = RexNode::PUBLISHER;
    } else if (varTypeName == SUBSCRIBER_CLASS) {
        id = generateID(decl); //TODO: change this
        type = RexNode::SUBSCRIBER;
    } else if (varTypeName == NODE_HANDLE_CLASS) {
        id = generateID(decl); //TODO: change this
        type = RexNode::NODE_HANDLE;
    } else if (varTypeName == TIMER_CLASS) {
        id = generateID(decl); //TODO: change this
        type = RexNode::TIMER;
    } else {
        id = generateID(decl); //TODO: change this
        type = RexNode::VARIABLE;
    }

    return new RexNode(id, type, shouldKeep);
}
RexNode *ROSWalker::specializedVariableNode(const FieldDecl *decl, bool shouldKeep) {
    return chooseVariableNodeType(decl, shouldKeep);
}
RexNode *ROSWalker::specializedVariableNode(const VarDecl *decl, bool shouldKeep) {
    return chooseVariableNodeType(decl, shouldKeep);
}

// Variability Aware but add edge/node functions are core now.
// This is base implementation with no variability.
void ROSWalker::addEdgeToGraph(RexEdge *e) {
    graph.addEdge(e);
}
RexEdge *ROSWalker::addEdgeToGraph(const std::string& srcID, const std::string& dstID, RexEdge::EdgeType type) {
    return ParentWalker::addEdgeToGraph(srcID, dstID, type);
}

void ROSWalker::addNodeToGraph(RexNode *n) {
    graph.addNode(n);
}

void ROSWalker::updateEdge(RexEdge *e) {
    (void)e;
}


