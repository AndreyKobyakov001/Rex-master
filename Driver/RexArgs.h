#pragma once

#include <string>
#include <vector>
#include <regex>
#include <boost/filesystem.hpp>
#include "ExtractionConfig.h"
// Represents the configuration extracted from the command line arguments
class RexArgs {
    unsigned int jobs;
    std::vector<boost::filesystem::path> inputPaths;
    std::vector<boost::filesystem::path> headerPaths;
    std::vector<std::string> clangFlags;
    boost::filesystem::path outputPath;
    boost::filesystem::path neo4jCypherPath;
    std::vector<boost::filesystem::path> neo4jCsvPaths;
	std::string prog;
	bool clangOnly;
    bool incremental; 
	bool buildCFG;
    
    // This enapsulates the configuration options that we support
    // for extraction, e.g. Variability Options
    ExtractionConfig config;
    RexArgs();
    RexArgs(const char* progPath);
    void validate() const;
  public:
    static RexArgs processArgs(int argc, const char **argv);
    enum ConfigType {
        VARIABILITY,
        LANG_FEATURES,
        ROS_FEATURES,
        GUIDED_EXTRACTION,
        COUNT
    };
    
    static std::string configToString(ConfigType type);
    static ConfigType stringToConfig(std::string type);
    
    static std::time_t readPreviousConfig(const boost::filesystem::path &objFile, ExtractionConfig &config);
    
    unsigned int getParallelJobs() const;
    const std::vector<boost::filesystem::path> &getInputPaths() const;
    const std::vector<boost::filesystem::path> &getHeaderPaths() const;
    const std::vector<std::string> &getClangFlags() const;
    const boost::filesystem::path &getOutputPath() const;
  const boost::filesystem::path &getNeo4jCypherPath() const;
  const std::vector<boost::filesystem::path> &getNeo4jCsvPaths() const;

    const std::string getProg() const;
    bool isBareBones() const;
    bool isIncremental() const;
    
    bool shouldBuildCFG() const;

    VariabilityOptions getVariabilityOptions() const;
    LanguageFeatureOptions getLanguageFeaturesOptions() const;
    ROSFeatureOptions getROSFeaturesOptions() const;
    GuidedExtractionOptions getGuidedExtractionOptions() const;
    
    const ExtractionConfig& getConfig() const { return config; }
};
