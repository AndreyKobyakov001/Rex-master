#pragma once

#include <string> // for string
#include <regex> // for regex

#include <boost/filesystem.hpp> // for path

class IgnoreMatcher {
    std::string pattern;
    std::regex matcher;
public:
    IgnoreMatcher();

    void addSourceDir(const boost::filesystem::path &sourceDir);

    bool shouldIgnore(const std::string &path) const;
};
