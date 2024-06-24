
#ifndef REX_BASELINEWALKER_H
#define REX_BASELINEWALKER_H

#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecursiveASTVisitor.h>

class BaselineWalker : public clang::RecursiveASTVisitor<BaselineWalker>{
  public:
    // Constructor/Destructor
    BaselineWalker();
    virtual ~BaselineWalker();

    // ASTWalker Functions
    virtual bool VisitStmt(clang::Stmt *statement);
    virtual bool VisitVarDecl(clang::VarDecl *decl);
    virtual bool VisitFieldDecl(clang::FieldDecl *decl);
    virtual bool VisitEnumDecl(clang::EnumDecl* decl);
    virtual bool VisitFunctionDecl(clang::FunctionDecl *decl);
    virtual bool VisitCXXRecordDecl(clang::CXXRecordDecl *decl);
    virtual bool VisitMemberExpr(clang::MemberExpr *memExpr);
    virtual bool VisitDeclRefExpr(clang::DeclRefExpr *declRef);
    
    virtual bool TraverseDecl(clang::Decl *d);
    virtual bool TraverseStmt(clang::Stmt* s);
    virtual bool TraverseIfStmt(clang::IfStmt* s);
    virtual bool TraverseSwitchStmt(clang::SwitchStmt *s);
    virtual bool TraverseCaseStmt(clang::CaseStmt *s);
    virtual bool TraverseWhileStmt(clang::WhileStmt *s);
    virtual bool TraverseForStmt(clang::ForStmt *s);
    virtual bool TraverseDoStmt(clang::DoStmt *s);
    // This isn't a Clang Traverse Stmt.
    virtual bool TraverseChildren(clang::Stmt *s);
};

#endif // REX_BASELINEWALKER_H
