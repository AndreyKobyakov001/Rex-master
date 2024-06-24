#pragma once

#include <boost/filesystem.hpp> // for path
#include <vector> // for vector
#include <unordered_set> // for unordered_set

// Need this code to store fs::path in unordered_set
// Source: https://lists.boost.org/boost-users/2012/12/76835.php
#include <boost/functional/hash.hpp>
namespace std {
    template<> struct hash<boost::filesystem::path> {
        size_t operator()(const boost::filesystem::path& p) const {
            return boost::filesystem::hash_value(p);
        }
    };
}

// A sorted version of the input files that separates what we need to analyze
// from the files we only need to link at the end.
class Analysis {
    bool addSourceFile(const boost::filesystem::path &path, const boost::filesystem::path &outputPath, int argc, const char **argv);

  public:
    // All of the configuration for each job we want to run.
    //
    // Will be passed to each walker.
    struct Job {
		
		enum Status
		{
			SUCCESS,
			COMPLETE_WITH_ERROR,
			FAIL,
			NOT_PROCESSED
		};
		
        boost::filesystem::path sourcePath;
        boost::filesystem::path objectFilePath;
        Status status;
        
        Job() : status{NOT_PROCESSED} {}
    };

    explicit Analysis(const std::vector<boost::filesystem::path> &files, const boost::filesystem::path &outputPath, int argc, const char **argv);

    Job &getJob(unsigned int index);
    unsigned int getNumJobs() const;
    bool hasJobs() const;
    const std::unordered_set<boost::filesystem::path> &getSourceDirectories() const;
    std::vector<boost::filesystem::path> getAllObjectFiles() const;
    bool hasExtraObjectFiles() const;
    void writeDiagnostics(const boost::filesystem::path &outputPath);

  private:
    std::vector<Job> jobs;
    std::vector<boost::filesystem::path> extraObjectFiles;
    std::unordered_set<boost::filesystem::path> sourceDirs;
};
