////////////////////////////////////////////////////////////////////////////////////////////////////////
// ROSWalker.h
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

#ifndef REX_ROSWALKER_H
#define REX_ROSWALKER_H

#include <stack>
#include <map>
#include <utility>

#include "ParentWalker.h"

class ROSWalker : public ParentWalker {
  public:
    // Constructor/Destructor
    ROSWalker(const WalkerConfig &config, clang::ASTContext *Context);
    virtual ~ROSWalker() override;

    // ASTWalker Functions
    bool VisitStmt(clang::Stmt *statement) override;
    bool VisitVarDecl(clang::VarDecl *decl) override;
    bool VisitFieldDecl(clang::FieldDecl *decl) override;
    bool VisitEnumDecl(clang::EnumDecl* decl) override;
    bool VisitFunctionDecl(clang::FunctionDecl *decl) override;
    bool VisitCXXRecordDecl(clang::CXXRecordDecl *decl) override;
    bool VisitMemberExpr(clang::MemberExpr *memExpr) override;
    bool VisitDeclRefExpr(clang::DeclRefExpr *declRef) override;

    bool TraverseDecl(clang::Decl *d) override;

    void handleROSstmt(clang::Stmt* statement);

    void extractTopicVariable(clang::Stmt* stmt);

	// ROS Handlers
    bool isSubscriberObj(const clang::CXXConstructExpr *ctor);
    bool isPublisherObj(const clang::CXXConstructExpr *ctor);
    bool isTimerObj(const clang::CXXConstructExpr *ctor);
    bool isPublish(const clang::CallExpr *expr);
    bool isSubscribe(const clang::CallExpr *expr);
    bool isAdvertise(const clang::CallExpr *expr);
    bool isTimer(const clang::CallExpr *expr);

    // ROS Helpers
    void recordStaticROSEntity(clang::CXXConstructExpr* constrExpr, clang::Stmt* statement);
    void recordNonStaticROSEntity(clang::CXXOperatorCallExpr* assignExpr, clang::Stmt* statement);
	void recordSubscribe(const clang::CallExpr *expr);
    void recordPublish(const clang::CallExpr *expr, std::string cfgID="");
    void recordAdvertise(const clang::CallExpr *expr);
    void recordTimer(const clang::CallExpr *expr);
    void recordTopic(std::string id, std::string name, const clang::Expr *topicArg);
    void recordROSEntity(clang::Stmt* stmt);
    void recordROSEntity(std::string declID, clang::Expr *expr, clang::Decl *decl);
    void recordROSEdge(clang::Stmt* stmt);

    RexNode *recordCallback(const clang::FunctionDecl *funcDecl, RexNode *topic, 
        const clang::CallExpr *expr, bool isDef);

    //std::string extractTopicName(const clang::Expr *topicArg);
	std::string extractTopicName(const clang::Expr *topicArg);
    std::string extractQueueSize(const clang::Expr *queueSizeArg);
    std::string getPublisherType(const clang::CallExpr *expr);

    RexNode *specializedVariableNode(const clang::FieldDecl *decl, bool shouldKeep) override;
    RexNode *specializedVariableNode(const clang::VarDecl *decl, bool shouldKeep) override;

    // CFG functions

    // Traverse each statement within the block, and for each encountered CallExpr `c`,
    // store the mapping `c` --> `block`. This lookup-table is useful when the principal AST traversal
    // records the CallExpr, allowing it to create relationships between CFG blocks.
    //
    // Return true if this block should have a corresponding RexNode created
    bool mapASTStatementsToCFGBlocks(const clang::CFGBlock* block);
    // Helper to above. Recursively traverse the AST rooted at `stmt`
    void mapASTStatementsToCFGBlocks(const clang::Stmt* stmt, const clang::CFGBlock* block);

private:
    // Helper method
    RexNode *chooseVariableNodeType(const clang::ValueDecl *decl, bool shouldKeep);

    // ROS Names
    const std::string PUBLISHER_CLASS = "ros::Publisher";
    const std::string SUBSCRIBER_CLASS = "ros::Subscriber";
    const std::string TIMER_CLASS = "ros::Timer";
    const std::string NODE_HANDLE_CLASS = "ros::NodeHandle";

    // ROS Names
    const std::string PUBLISH_FUNCTION = "ros::Publisher::publish";
    const std::string SUBSCRIBE_FUNCTION = "ros::NodeHandle::subscribe";
    const std::string ADVERTISE_FUNCTION = "ros::NodeHandle::advertise";
    const std::string TIMER_FUNCTION = "ros::NodeHandle::createTimer";
    const std::string TIMER_PREFIX_1 = "ros::Duration(";
    const std::string TIMER_PREFIX_2 = "Duration(";

    // extract topic names stored in variables
    const std::string PARAM_FUNC = "ros::NodeHandle::param";
    const std::string STR_FUNC = "std::string";

    // ROS Attributes
    const unsigned int PUB_MAX = 30;

    enum ROSType {
        ROS_NONE,
        PUB,
        SUB,
        TIMER
    };

    std::stack<std::string> pubStack;
    std::stack<std::string> subStack;
    std::stack<std::string> timerStack;
    std::map<std::string, std::string> varMap;
    std::map<std::string, std::vector<std::pair<const clang::CallExpr*, std::string>>> publishExprs;

    // Core functions. Must always be used when adding edge or node to graph, or updating a node or edge.
    virtual void addEdgeToGraph(RexEdge *) override;
    virtual RexEdge *addEdgeToGraph(const std::string& srcID, const std::string& dstID, RexEdge::EdgeType type) override;
    virtual void addNodeToGraph(RexNode *) override;
    virtual void updateEdge(RexEdge *) override;
};

#endif // REX_ROSWALKER_H
