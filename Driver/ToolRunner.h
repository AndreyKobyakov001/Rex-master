#pragma once

#include "Analysis.h"
#include <string>

class IgnoreMatcher;
class RexArgs;
class TAGraph;

// Runs a single analysis job through clang.
class ToolRunner {
    const Analysis::Job &job;
    const IgnoreMatcher &ignored;
    const RexArgs &args;

  public:
    ToolRunner(const Analysis::Job &job, const IgnoreMatcher &ignored,  const RexArgs &args);

    int run() const;
};
