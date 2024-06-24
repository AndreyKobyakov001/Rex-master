#include "IgnoreMatcher.h"

#include <fstream>

#include <boost/algorithm/string.hpp>

using namespace std;
namespace fs = boost::filesystem;
namespace ba = boost::algorithm;

IgnoreMatcher::IgnoreMatcher() {}

// Adds a source directory. These should NOT be ignored during traversal.
void IgnoreMatcher::addSourceDir(const fs::path &sourceDir) {
    if (!pattern.empty()) {
        pattern += '|';
    }

    // only match if this path is the prefix of the path being checked
    pattern += '^';
    // prefix matching only works if we use the absolute path
    if (sourceDir.is_absolute()) {
        pattern += sourceDir.string();
    } else {
        pattern += fs::canonical(sourceDir).string();
    }

    // Reload the matcher with the latest pattern
    matcher = regex(pattern);
}

bool IgnoreMatcher::shouldIgnore(const string &path) const {
    // Ignore anything that isn't within the pattern of source directories
    return !regex_search(path, matcher);
}
