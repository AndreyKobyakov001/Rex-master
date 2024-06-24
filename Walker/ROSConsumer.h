/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ROSConsumer.h
//
// Created By: Bryan J Muscedere
// Date: 08/07/17.
//
// Sets up the AST walker components that
// Rex uses to walk through Clang's AST.
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

#ifndef REX_ROSCONSUMER_H
#define REX_ROSCONSUMER_H

#include "ROSWalker.h"
#include "VarWalker.h"
#include <string>
#include <vector>


#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>
#include "../Driver/RexArgs.h"


class IgnoreMatcher;

// All inputs and outputs passed to the walker
struct WalkerConfig {
    // The destination of the generated graph
    TAGraph &graph;
    // The paths to ignore
    const IgnoreMatcher &ignored;
    // The current "feature" that the walked file belongs to
    const std::string &featureName;
    //TODO: Enabled C++ features, other walker settings, etc.
    //Should help with selective schema
    
    //This is fo
    bool clangOnly;
    
    // Whether to build and include CFG information
    bool buildCFG;

    // Variability Aware
    VariabilityOptions vOpts;

    LanguageFeatureOptions lFeats;

    ROSFeatureOptions ROSFeats;

    GuidedExtractionOptions geOpts;

    WalkerConfig(TAGraph &graph, const IgnoreMatcher &ignored,
        const std::string &featureName, bool clangOnly = false, bool buildCFG = false,
        VariabilityOptions vOpts = VariabilityOptions{},
        LanguageFeatureOptions lFeats = LanguageFeatureOptions{},
        GuidedExtractionOptions geOpts = GuidedExtractionOptions{},
        ROSFeatureOptions ROSFeats = ROSFeatureOptions{});
};

class ROSConsumer : public clang::ASTConsumer {
    BaselineWalker* walker;

  public:
    // Constructor/Destructor
    ROSConsumer(const WalkerConfig &config, clang::ASTContext *context);
    virtual ~ROSConsumer();
    virtual void HandleTranslationUnit(clang::ASTContext &context) override;

  private:
    BaselineWalker* createWalker(const WalkerConfig &config, clang::ASTContext *context);
};

class ROSAction : public clang::ASTFrontendAction {
    const WalkerConfig &config;

  public:
    explicit ROSAction(const WalkerConfig &config);
    virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) override;
};

class ROSFrontendActionFactory : public clang::tooling::FrontendActionFactory {
    const WalkerConfig &config;

public:
    explicit ROSFrontendActionFactory(const WalkerConfig &graph);
    virtual clang::FrontendAction *create() override;
};

#endif // REX_ROSCONSUMER_H
