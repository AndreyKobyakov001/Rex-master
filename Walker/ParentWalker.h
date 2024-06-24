/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ParentWalker.h
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

#ifndef REX_PARENTWALKER_H
#define REX_PARENTWALKER_H

#include <map>

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/Analysis/CFG.h>

#include "../Graph/TAGraph.h"
#include "BaselineWalker.h"
#include "../Driver/RexArgs.h"
#include "../Linker/TASchemeAttribute.h"


struct WalkerConfig;
class IgnoreMatcher;


class ParentWalker : public BaselineWalker {
public:
    // Constructor/Destructor
    ParentWalker(const WalkerConfig &config, clang::ASTContext *Context);
    virtual ~ParentWalker();

    // Variable access
    // A function can read or write to a variable. These are some data structures
    // Used to keep track of variable access.
    enum AccessMethod { NONE, BOTH, READ, WRITE };
    // Represents a mapping from a variable ID to the way it's accessed in a particular expression.
    class AccessMap {
            typedef std::map<std::string, AccessMethod> mapType;
        public:
            // Note: merges the method if `var` already exists.
            void insert(const std::string& var, AccessMethod method);
            // Iterators
            mapType::iterator begin() { return _map.begin(); }
            mapType::iterator end() { return _map.end(); }
            mapType::const_iterator begin() const { return _map.cbegin(); }
            mapType::const_iterator end() const { return _map.cend(); }

            // Return a single access method which encompasses both behaviors.
            // For instance, READ + WRITE = BOTH; NONE + WRITE = WRITE, ...
            static AccessMethod mergeAccessMethods(AccessMethod m1, AccessMethod m2);
            // Keep keys distinct to m1 and m2, and for shared keys, merge their access methods.
            static AccessMap merge(const AccessMap& m1, const AccessMap& m2);
        private:
            std::map<std::string, AccessMethod> _map;
    };

    // Do not allow parent walker to be copied since its state variables should
    // not be duplicated
    ParentWalker(const ParentWalker &) = delete;
    ParentWalker &operator=(const ParentWalker &) = delete;

    // Source location checking
    bool isInSystemHeader(const clang::Stmt *statement);
    bool isInSystemHeader(const clang::Decl *decl);
    bool isInMainFile(const clang::Stmt *statement);
    bool isInMainFile(const clang::Decl *decl);

	// C++ Detectors
    void recordFunctionDecl(const clang::FunctionDecl *decl);
    void recordCXXRecordDecl(const clang::CXXRecordDecl *decl, RexNode::NodeType type);;
    void recordVarDecl(const clang::VarDecl *decl);
    void recordFieldDecl(const clang::FieldDecl *decl);
    void recordEnumDecl(const clang::EnumDecl* decl);

    // Expr Recorders
    void recordCallExpr(const clang::CallExpr *expr, bool isROSFunction);
    void recordConstructExpr(const clang::CXXConstructExpr *ctor);
    void recordReturnStmt(const clang::ReturnStmt *retStmt);
    // Add `write` from `decl` to `lhs`, and add `varWrite` from each `rhs` to each `lhs`
    // Record CFG information if necessary
    void recordFunctionVarUsage(const clang::FunctionDecl *decl,
                                const std::set<std::string>& lhs,
                                const std::set<std::string>& rhs);
    // Add `parWrite` from each `rhs` to each `lhs`
    void recordParameterWrite(const clang::FunctionDecl *calledFn,
                              const std::string& param,
                              const std::set<std::string>& rhs);
    void recordControlFlow(const clang::DeclRefExpr *expr);
    void recordControlFlow(const clang::MemberExpr *expr);
    void recordControlFlow(const clang::ValueDecl *decl);
    void recordParentFunction(const clang::Stmt *statement, RexNode *baseItem);
    void recordASTControl(const clang::Stmt *baseStmt, const RexNode *rosItem);
    void recordParamPassing(const clang::CallExpr *expr);

	// Var Recorders
    bool addVars(const clang::IfStmt *stmt, int64_t baseExprID, std::vector<std::string> *vars);
    bool addVars(const clang::ForStmt *stmt, int64_t baseExprID, std::vector<std::string> *vars);
    bool addVars(const clang::WhileStmt *stmt, int64_t baseExprID, std::vector<std::string> *vars);
    bool addVars(const clang::DoStmt *stmt, int64_t baseExprID, std::vector<std::string> *vars);
    bool addVars(const clang::SwitchStmt *stmt, int64_t baseExprID, std::vector<std::string> *vars);
    bool addVars(std::set<const clang::Stmt*> stmts, int64_t baseExprID, std::vector<std::string> *vars);

    // Name Helper Functions
    std::string generateID(const clang::NamedDecl *decl, bool root = true, bool isInnerCFG = false);
    std::string generateID(const clang::MemberExpr *member);
    bool assignedFileName;
    // A CFG's entry block gets assigned a different ID than the other blocks.
    // This method can be used as a convenience to avoid explicitly checking if the block is
    // an entry or inner block.
    std::string generateCFGBlockID(const clang::FunctionDecl *fdecl, const clang::CFGBlock* block, const clang::CFG* cfg);
    // These three methods are for when the above information is already known.
    std::string generateCFGEntryBlockID(const clang::FunctionDecl *fdecl);
    std::string generateCFGInnerBlockID(const clang::FunctionDecl *fdecl, const clang::CFGBlock* block);
    std::string generateCFGExitBlockID(const clang::FunctionDecl *fdecl);
    // Return the ID for the node representing a function's return value
    std::string generateRetID(const clang::FunctionDecl *fdecl);
    std::string generateName(const clang::NamedDecl *decl);
    std::string generateFileName(const clang::NamedDecl *decl);
    std::string getPartialFileName(std::string filename, const clang::NamedDecl *decl);
    void recordNamedDeclLocation(const clang::NamedDecl *decl, RexNode* node, bool hasBody = true);

    // Helper for extracting template arguments from member expressions
    std::vector<std::string> extractTemplateType(clang::MemberExpr* expr);

    bool isStatic(const clang::NamedDecl* decl);
    bool isExternal(const clang::NamedDecl* decl);
    const clang::NamedDecl *funcNamedDecl;

    // System Headers - Helpers
    bool isInSystemHeader(const clang::SourceManager &manager, clang::SourceLocation loc);

    // Minimal Variables - Helpers
    const clang::NamedDecl *getAssignee(const clang::CXXOperatorCallExpr *parent);
    const clang::MemberExpr *getAssignStmt(const clang::CXXOperatorCallExpr *parent);
    const clang::CXXRecordDecl *getParentClass(const clang::NamedDecl *decl);

    // ROSHandlers - Helpers
    bool isClass(const clang::CXXConstructExpr *ctor, std::string className);
    bool isFunction(const clang::CallExpr *expr, std::string functionName);
    const clang::NamedDecl *getParentAssign(const clang::CXXConstructExpr *expr);
    std::vector<std::string> getArgs(const clang::CallExpr *expr);
    clang::NamedDecl *getParentVariable(const clang::Expr *callExpr);

    template <typename T>
    const clang::NamedDecl *getParent(const T *node)
    {
		auto parents = Context->getParents(*node);
		const clang::NamedDecl *parentDecl = parents[0].template get<clang::NamedDecl>();
		while (!parentDecl && !parents.empty()) {
			parents = Context->getParents(parents[0]);
			parentDecl = parents[0].template get<clang::NamedDecl>();
		}
		return parentDecl;
	};

	// Control Flow Detectors
    bool usedInIfStatement(const clang::Expr *declRef);
    bool usedInLoop(const clang::Expr *declRef);
    bool findExpression(const clang::Expr *expression);

    // Secondary Helper Functions
    void addParentRelationship(const clang::NamedDecl *baseDecl, std::string baseID);
    const clang::FunctionDecl *getParentFunction(const clang::Stmt *expr);

    virtual RexNode *specializedVariableNode(const clang::FieldDecl *decl, bool shouldKeep);
    virtual RexNode *specializedVariableNode(const clang::VarDecl *decl, bool shouldKeep);

protected:

    TAGraph &graph;
    const IgnoreMatcher &ignored;
    const std::string &featureName;
    const LanguageFeatureOptions langFeats;
    bool buildCFG;
    const ROSFeatureOptions ROSFeats;
    const std::regex callbackFuncRegex;
    clang::ASTContext *Context;

    std::vector<clang::Expr *> parentExpression;

    // Variability Aware - but also these functions must now always
    // be used when adding edges/nodes to the graph!
    virtual void addEdgeToGraph(RexEdge *);
    virtual RexEdge *addEdgeToGraph(const std::string& srcID, const std::string& dstID, RexEdge::EdgeType type);
    virtual void addNodeToGraph(RexNode *);
    virtual void updateEdge(RexEdge *);
    virtual RexEdge *addEdgeIfNotExist(const std::string& srcID, const std::string& dstID, RexEdge::EdgeType type);
    virtual RexEdge *addOrUpdateEdge(const std::string& srcID, const std::string& dstID, RexEdge::EdgeType type);

    void addWriteRelationships(const clang::Stmt* stmt, const clang::FunctionDecl* curFunc);

    //////////////////
    // CFG information

    // function ID --> CFG of the function
    // CFGs get generated per function when that function's definition is traversed by the walker.
    // To keep around the blocks longer than the scope of the traversal procedure,
    // take ownership of the generated CFGs in this map.
    std::unique_ptr<const clang::CFG> curCFG;
    const clang::FunctionDecl* curFunc;
    const clang::FunctionDecl* curCallFunc = nullptr;
    bool canBuildCFG() const;

    // The CFG building occurs in three steps, spanning the AST traversal of a single function.
    //
    // 1) the CFG must be related to the AST nodes. This is because Rex derives its relationships by walking
    // the AST. So to be able to link relationships to their occurrence in the CFG, there must be a mapping between the
    // two.
    // The mapping process also deals with decomposing the Clang CFG basic blocks down to the statement level.
    // This is because the order of facts is determined entirely by their locations within the Rex CFG nodes.
    // But if multiple facts are all within the same basic block, then it is impossible to tell which comes first,
    // and thus false positive paths, like the following, are impossible to detect:
    // c = b; // b -varWrite-> c
    // b = a; // a -varWrite-> b
    // False positive: a -varWrite-> b -varWrite-> c // can't actually happen!
    //
    // So we abandon the notion of "basic block", and instead just create a Rex CFG node per statement within the Clang
    // basic blocks.
    // Unfortunately, even this level of granularity is not enough, since there sometimes occur multiple facts
    // within one statement, and these can sometimes cause false positives as well:
    //  a = a + b; // b -varWrite-> a, a -varWrite-> a
    //  False positive: b -varWrite-> a -varWrite-> a
    //
    // Even something as common as a function call is affected:
    //
    //  void foo() {
    //     bar(i);
    //  }
    // [B1]
    //    1: bar(i)         // the facts `foo calls bar` and `i writes to bar::parameter` both get mapped to B1.1
    //    Preds (1): B2
    //    Succs (1): B0
    //
    // This is something that will have to be fixed in the future.
    //
    // 2) The amount of CFG nodes that actually end up containing facts is small compared to the amount of total
    // nodes created. So to minimize the number of CFG paths that can be taken during traversal when checking for reachability
    // and the overall number of CFG nodes in the graph, we first also remove "unused" nodes. So the second step
    // is "linking" the facts created by the Rex traversal to their mapped CFG node. This happens throughout the traversal,
    // as each fact is created.
    //
    // 3) Once the function has been fully traversed, the "unused" nodes are removed from the CFG model, and
    // the model is added to the TA graph.
    class CFGModel {
        class BasicBlock;
        typedef std::map<const clang::CFGBlock*, BasicBlock*> ClangMap;

        // Represents a single Clang basic block.
        //
        // A Clang basic block looks something like this:
        // [B2]
        //    1: foo(x);
        //    2: int var1 = var2;
        //    Preds (1): B3
        //    Succs (1): B1
        //
        // - The block's ID is 2
        // - the block has 2 statements, stmt 2.1 and stmt 2.2
        // - each statement is an AST node
        //
        // These blocks are linked within the CFG like this:
        //
        // [B1 (ENTRY)]
        //   Succs (1): B0
        //
        // [B0 (EXIT)]
        //   Preds (1): B1
        //
        //
        // [B4 (ENTRY)]
        //   Succs (1): B3
        //
        // [B1]
        //   1: x++
        //   Preds (1): B3
        //   Succs (1): B0
        //
        // [B2]
        //   1: goo()
        //   Preds (1): B3
        //   Succs (1): B0
        //
        // [B3]
        //   1: cond (ImplicitCastExpr, LValueToRValue, _Bool)
        //   T: if [B3.1]
        //   Preds (1): B4
        //   Succs (2): B2 B1
        //
        // [B0 (EXIT)]
        //   Preds (2): B1 B2
        //
        // Basic blocks are not actually kept around in the final TA graph, they are just containers of
        // CFGNode nodes which will eventually be made into RexNodes.
        // The individual nodes are all connected linearly to each other within a basic block, since... that's the definition
        // of a basic block. The tail and head node of each block is then connected to the head/tail node of its neighbors
        // (Preds, Succs).
        class BasicBlock {
            public:
                BasicBlock(const clang::CFGBlock* cfgBlock);

                // Map an AST node to the tail CFGNode
                void mapStmt(const clang::Stmt* stmt);
                // Record that this statement has been used to generate a Rex fact.
                // The corresponding CFGNode will now be marked to get a RexNode in the final TA graph.
                void markCFGNodeContainsFact();

                 // Get the RexNode ID corresponding to the current block's CFGNode. (the TA graph doesn't have to have been
                // built yet)
                std::string getCFGNodeID(ParentWalker& walker);

                // Return whether this block contains any CFGNode will be included in the TA graph.
                bool shouldKeep() const;

                // Get all of the successors to this basic block. If a successor should not be kept (`shouldKeep() == false`),
                // then return *its* effective children. This is done recursively.
                std::set<BasicBlock*> getEffectiveChildren(const ClangMap& mapping) const;

                // The head and tail are the first/last CFGNode within this block's sequence of CFGNodes,
                // where `CFGNode::shouldKeep == true`
                RexNode* getHead() { return head; }
                RexNode* getTail() { return tail; }

                // For each BasicBlock that should be kept, create a corresponding RexNode in the TA graph.
                // Link the RexNodes together linearly.
                void createAndLinkRexCFGNodes(ParentWalker& walker);
            private:

                std::set<BasicBlock*> doGetEffectiveChildren(const ClangMap& mapping,
                                                            std::set<BasicBlock*>& alreadyVisited_BlocksThatShouldNotBeKept) const;

                // The Clang basic block being modelled.
                const clang::CFGBlock* block_;

                // True if this block should be kept.
                bool shouldKeep_ = false;

                RexNode* head = nullptr;
                RexNode* tail = nullptr;
        };

        // Map an AST node and its children to the BasicBlock
        // as necessary.
        void mapStmt(const clang::Stmt* stmt, BasicBlock* block);

        // Map each Clang basic block to the model version
        ClangMap clangMap;
        // Map each AST node to a basic block.
        std::map<const clang::Stmt*, BasicBlock*> stmtMap;
        // Map each FunctionDecl to CallExprs that call them
        std::map<const clang::FunctionDecl*, std::vector<const clang::CallExpr*>> callMap;

        // Messy, but we frequently need access to some ParentWalker methods
        ParentWalker& walker;

        BasicBlock* entryBlock = nullptr;

    public:

        CFGModel(ParentWalker& walker);
        ~CFGModel();

        // Visit the current function's entire CFG (ParentWalker::curCFG), and map it to the CFGModel.
        // For every statement under every basic block, map the statement to a node the model.
        // This must be called before generating Rex facts in the function's body if those facts are
        // linked to some CFG node.
        void mapASTtoCFG();
        void mapStmtToEntry(const clang::Stmt* stmt);
        // Return whether the AST node `stmt` has been mapped to the CFG model.
        bool hasStmt(const clang::Stmt* stmt) const;
        // Get the RexNode ID corresponding to the stmt's CFGNode. (the TA graph doesn't have to have been
        // built yet)
        std::string getCFGNodeID(const clang::Stmt* stmt);

        // Record that this statement has been used to generate a Rex fact.
        // The corresponding CFGNode will now be marked to get a RexNode in the final TA graph.
        void markCFGNodeContainsFact(const clang::Stmt* stmt);

        // Take the model and convert it into an actual TA graph, using RexNodes and RexEdges.
        // This should be called
        void createAndLinkTAGraph() const;
    };
    std::unique_ptr<CFGModel> cfgModel = nullptr;
private:

    //////////////////////////
    // Tracking Dataflows
    //
    // Dataflow can occur in many different expressions within the AST. The goal of these functions is to abstract
    // out the pattern matching on these statements in a simple and convenient manner.
    //
    // For all statements where dataflow an occur, label the variables providing the data as "rvalues", and
    // the variables receiving the data as "lvalues". These don't correspond to C++ lvalues/rvalues so a better name may
    // be needed.
    //
    // To implement dataflow tracking for a particular AST statement/expression, 3 methods are needed:
    // 1. addWriteRelationships(*Stmt/*Expr, FunctionDecl curFunc);
    //      - this method extracts the rvalues and lvalues from the expression, and feeds them into `recordFunctionVarUsage`,
    //        which generates VAR_WRITES and WRITES relationships for these dataflows.
    //      - it is called by ROSWalker throughout the standard Clang traversal
    // 2. extractRValues(const clang::Expr*)
    //    - takes an expression and returns all variable IDs which can return data if this expression is used
    //      as the "rhs" of a dataflow
    //    - not implemented for Stmts because they can't return values
    // 3. extractLValues(const clang::Expr*)
    //    - takes an expression and returns all variable IDs which can receive data if this expression is used
    //      as the "lhs" of a dataflow
    //    - not implemented for Stmts because they can't return values
    //
    // These 3 methods are overridden per the AST node type being analyzed. However, they must ALSO be explicitly dispatched
    // within the generic `Expr*` / `Stmt*` version of the functions. Couldn't find a cleaner way to do this -- can't do
    // visitor pattern on these because they're clang classes. Perhaps some template magic can be used...
    //
    //
    // A dummy example is if we have an "SimpleAssignmentExpr" AST node which represents all `operator=` usages
    //
    //     addWriteRelationships(SimpleAssignmentExpr op, FunctionDecl curFunc) {
    //        lhs = extractLValues(op.getLHS());
    //        rhs = extractRValues(op.getRHS());
    //        recordFunctionVarUsage(curFunc, lhs, rhs);
    //     }
    //     extractRValues(SimpleAssignmentExpr op) {
    //        return extractRValues(op.rhs)
    //     }
    //     extractLValues(SimpleAssignmentExpr op) {
    //        return extractLValues(op.lhs)
    //     }
    //
    //  now, any arbitrary nesting of assignments will create the following dataflows:
    //    ((a = b) = (c = d)) = (e = f)
    //      - f -> a, e
    //      - d -> a, c
    //      - b -> a

    // Tracks the statement for which dataflow is being recorded. Used to link the dataflows to the CFG. Messy.
    const clang::Stmt* curDflowStmt = nullptr;

    // define return id pattern
    const std::string RETURN_SUFFIX = "__ret!;;";

    ///// Implementations for the various dataflows we support /////

    void addWriteRelationships(const clang::ReturnStmt* stmt, const clang::FunctionDecl* curFunc);
    void addWriteRelationships(const clang::DeclStmt* stmt, const clang::FunctionDecl* curFunc);
    void addWriteRelationships(const clang::CXXForRangeStmt* rangefor, const clang::FunctionDecl* curFunc);

    void addWriteRelationships(const clang::Expr* expr, const clang::FunctionDecl* curFunc);
    std::set<std::string> extractRValues(const clang::Expr* expr);
    std::set<std::string> extractLValues(const clang::Expr* expr);

    std::set<std::string> extractRValues(const clang::DeclRefExpr* declref);
    std::set<std::string> extractLValues(const clang::DeclRefExpr* declref);

    void createThisVariables(const clang::ValueDecl *valueDecl);
    std::set<std::string> extractRValues(const clang::MemberExpr* member);
    std::set<std::string> extractLValues(const clang::MemberExpr* member);

    void addWriteRelationships(const clang::CXXConstructExpr* ctor, const clang::FunctionDecl* curFunc);
    std::set<std::string> extractRValues(const clang::CXXConstructExpr* ctor);

    void addWriteRelationships(const clang::BinaryOperator* binop, const clang::FunctionDecl* curFunc);
    std::set<std::string> extractRValues(const clang::BinaryOperator* binop);
    std::set<std::string> extractLValues(const clang::BinaryOperator* binop);

    void addWriteRelationships_BinaryOperatorCommon(const clang::Expr* lhs,
                                                    const clang::Expr* rhs,
                                                    const clang::BinaryOperator::Opcode opcode,
                                                    const clang::FunctionDecl* curFunc);
    std::set<std::string> extractRValues_BinaryOperatorCommon(const clang::Expr* lhs,
                                                              const clang::Expr* rhs,
                                                              const clang::BinaryOperator::Opcode opcode);
    std::set<std::string> extractLValues_BinaryOperatorCommon(const clang::Expr* lhs,
                                                              const clang::BinaryOperator::Opcode opcode);

    void addWriteRelationships(const clang::UnaryOperator* unop, const clang::FunctionDecl* curFunc);
    std::set<std::string> extractRValues(const clang::UnaryOperator* unop);
    std::set<std::string> extractLValues(const clang::UnaryOperator* unop);


    void addWriteRelationships_UnaryOperatorCommon(const clang::Expr* arg,
                                                   const clang::UnaryOperator::Opcode opcode,
                                                   const clang::FunctionDecl* curFunc);
    std::set<std::string> extractRValues_UnaryOperatorCommon(const clang::Expr* arg);
    std::set<std::string> extractLValues_UnaryOperatorCommon(const clang::Expr* arg);

    void addWriteRelationships(const clang::ArraySubscriptExpr* arr, const clang::FunctionDecl* curFunc);
    std::set<std::string> extractRValues(const clang::ArraySubscriptExpr* arr);
    std::set<std::string> extractLValues(const clang::ArraySubscriptExpr* arr);

    void addWriteRelationships_SubscriptOperatorCommon(const clang::Expr* arrayObj, const clang::FunctionDecl* curFunc);
    std::set<std::string> extractRValues_SubscriptOperatorCommon(const clang::Expr* arrayObj);
    std::set<std::string> extractLValues_SubscriptOperatorCommon(const clang::Expr* arrayObj);


    void addWriteRelationships_OverloadedOperator(const clang::CXXOperatorCallExpr* opcall,
                                                  const clang::FunctionDecl* curFunc);
    std::set<std::string> extractRValues_OverloadedOperator(const clang::CXXOperatorCallExpr* opcall);
    std::set<std::string> extractLValues_OverloadedOperator(const clang::CXXOperatorCallExpr* opcall);

    void addWriteRelationships(const clang::CallExpr* call, const clang::FunctionDecl* curFunc);
    std::set<std::string> extractRValues(const clang::CallExpr* call);
    std::set<std::string> extractLValues(const clang::CallExpr* call);

    // Check if id belongs to a return node - Helper
    bool isReturnID(const std::string& strID); 

};

#endif // REX_PARENTWALKER_H
