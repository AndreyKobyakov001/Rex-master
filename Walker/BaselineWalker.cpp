
#include "BaselineWalker.h"

BaselineWalker::BaselineWalker() {}

BaselineWalker::~BaselineWalker() {}

// All of the following are meant to just return true. The reason they're written this way is to get rid of annoying warnings when building.

bool BaselineWalker::VisitStmt(clang::Stmt *statement)
{
    return statement || !statement;
}

bool BaselineWalker::VisitVarDecl(clang::VarDecl *decl)
{
	return decl || !decl;
}

bool BaselineWalker::VisitFieldDecl(clang::FieldDecl *decl)
{
	return decl || !decl;
}

bool BaselineWalker::VisitEnumDecl(clang::EnumDecl* decl)
{
	return decl || !decl;
}

bool BaselineWalker::VisitFunctionDecl(clang::FunctionDecl *decl)
{
	return decl || !decl;
}

bool BaselineWalker::VisitCXXRecordDecl(clang::CXXRecordDecl *decl)
{
	return decl || !decl;
}

bool BaselineWalker::VisitMemberExpr(clang::MemberExpr *memExpr)
{
	return memExpr || !memExpr;
}

bool BaselineWalker::VisitDeclRefExpr(clang::DeclRefExpr *declRef)
{
	return declRef || !declRef;
}


bool BaselineWalker::TraverseDecl(clang::Decl *d) {
    return RecursiveASTVisitor<BaselineWalker>::TraverseDecl(d);
}

bool BaselineWalker::TraverseStmt(clang::Stmt* s) {
     return RecursiveASTVisitor<BaselineWalker>::TraverseStmt(s);
}

bool BaselineWalker::TraverseIfStmt(clang::IfStmt* s) {
    return RecursiveASTVisitor<BaselineWalker>::TraverseIfStmt(s);
}

bool BaselineWalker::TraverseSwitchStmt(clang::SwitchStmt *s) {
    return RecursiveASTVisitor<BaselineWalker>::TraverseSwitchStmt(s);
}

bool BaselineWalker::TraverseCaseStmt(clang::CaseStmt *s) {
    return RecursiveASTVisitor<BaselineWalker>::TraverseCaseStmt(s);
}

bool BaselineWalker::TraverseWhileStmt(clang::WhileStmt *s) {
    return RecursiveASTVisitor<BaselineWalker>::TraverseWhileStmt(s);
}
bool BaselineWalker::TraverseForStmt(clang::ForStmt *s) {
    return RecursiveASTVisitor<BaselineWalker>::TraverseForStmt(s);
}

bool BaselineWalker::TraverseDoStmt(clang::DoStmt *s) {
    return RecursiveASTVisitor<BaselineWalker>::TraverseDoStmt(s);
}

bool BaselineWalker::TraverseChildren(clang::Stmt *s) {
    return s || !s;
}
