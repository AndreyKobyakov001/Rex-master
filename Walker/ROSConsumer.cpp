/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ROSConsumer.cpp
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

#include "ROSConsumer.h"

#include <clang/Frontend/CompilerInstance.h>
#include <iostream>

using namespace clang;
using namespace clang::tooling;
using namespace std;

WalkerConfig::WalkerConfig(TAGraph &graph, const IgnoreMatcher &ignored,
                            const string &featureName, bool clangOnly, bool buildCFG,
                            VariabilityOptions vOpts, LanguageFeatureOptions lFeats,
                            GuidedExtractionOptions geOpts, ROSFeatureOptions ROSFeats):
    graph{graph}, ignored{ignored}, featureName{featureName}, clangOnly{clangOnly},
    buildCFG{buildCFG}, vOpts{vOpts}, lFeats{lFeats}, geOpts{geOpts}, ROSFeats{ROSFeats} {}


/**
 * Creates a ROS consumer.
 * @param context The AST context.
 */
ROSConsumer::ROSConsumer(const WalkerConfig &config, ASTContext *context)
{
    walker = createWalker(config, context);
}

ROSConsumer::~ROSConsumer()
{
    delete walker;
}

/**
 * Handles the AST context's translation unit. Tells Clang to traverse AST.
 * @param context The AST context.
 */
void ROSConsumer::HandleTranslationUnit(ASTContext &context) {
    walker->TraverseDecl(context.getTranslationUnitDecl());
}

// TODO: Create alternative walkers based on what is available
// in the config and potentially other sources. This should
// make implementing the selective schema easier I hope
BaselineWalker* ROSConsumer::createWalker(const WalkerConfig &config, ASTContext *context)
{
    if(config.clangOnly) {
        return new BaselineWalker();
    }
    if (config.vOpts.isOn()) {
        return new VarWalker(config, context);
    }
    return new ROSWalker(config, context);
}

ROSAction::ROSAction(const WalkerConfig &config): config{config} {}

/**
 * Creates an AST consumer to "eat" the AST.
 * @param Compiler The compiler instance to process.
 * @param InFile The input file. (Currently unused because we just need the AST)
 * @return A pointer to the AST consumer.
 */
std::unique_ptr<ASTConsumer> ROSAction::CreateASTConsumer(
    CompilerInstance &Compiler, StringRef /* InFile */) {
	// Hide all output
	//Compiler.getDiagnostics().setClient(new IgnoringDiagConsumer());
    return std::unique_ptr<ASTConsumer>(new ROSConsumer(config, &Compiler.getASTContext()));
}

ROSFrontendActionFactory::ROSFrontendActionFactory(const WalkerConfig &config): config{config} {}

FrontendAction *ROSFrontendActionFactory::create() {
    return new ROSAction(config);
}
