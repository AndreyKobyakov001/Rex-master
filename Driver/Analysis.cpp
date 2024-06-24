#include "Analysis.h"
#include "RexArgs.h"

#include <iostream> // for cerr, endl
#include <algorithm> // for find
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp> 

using namespace std;
namespace fs = boost::filesystem;

// Attempt to add a source file, returns true if this succeeds.
//
// This can fail if the file is not a supported source file.
bool Analysis::addSourceFile(const fs::path &path, const fs::path &outputPath, int argc, const char **argv) {
    // From: http://labor-liber.org/en/gnu-linux/development/index.php?diapo=extensions
    // And: https://en.wikibooks.org/wiki/C%2B%2B_Programming/Programming_Languages/C%2B%2B/Code/File_Organization
    //
    // Header files are considered source files by Rex because they can
    // contain code for the implementations of classes/methods/functions/etc.
    // Rex was changed to only consider facts from files it is told to analyse.
    // That means that we need to include headers or else we will miss critical
    // facts for class declarations, member variables, inline functions, etc.
    static vector<fs::path> cpp_ext{".cc", ".cp", ".cxx", ".cpp", ".c++", ".C", ".c"};
        //".h", ".hh", ".hxx", ".hpp", ".h++", ".H"};
	
	
    const fs::path ext = path.extension();
    fs::path separator("/");
    string sep = separator.make_preferred().string();
    if (std::find(cpp_ext.begin(), cpp_ext.end(), ext) != cpp_ext.end()) {
        Job job;
        job.sourcePath = fs::canonical(path);
        string objName = job.sourcePath.parent_path().string();
        boost::replace_all(objName, sep, ";");

        // capture all configuration options and store to configString
        std::string configString = "";
        for (int i = 1; i < argc; i++)
        {
            // added to only capture options instead of files
            if (argv[i][0] == '-') {
                configString = (configString + argv[i]);
            }
        }

        // replace all "-" with empty string
        boost::replace_all(configString,"-","");

        // We can't just replace the extension with "tao" because headers and
        // source files often have the same stem.
        // added if statement in case where no option is selected
        if (configString == "") {
            objName = objName + ";" + path.filename().string() + ".tao";
        }
        else {
            objName = configString + "-(" + objName + ";" + path.filename().string() + ").tao";
        }
        
        job.objectFilePath = outputPath;
        job.objectFilePath /= objName;
        // ClangTool will change the current working directory to whatever it
        // wants during its analysis. Thus, it is absolutely imperative that
        // this path be absolute. If this is a relative path, the file will end
        // up wherever clang decides the current directory should be.
        job.objectFilePath = fs::absolute(job.objectFilePath);
        jobs.push_back(job);
        return true;
    }
    else if(ext == ".tao")
    {
		extraObjectFiles.push_back(path);
		return true;
	}
    return false;
}

// Determine the files to analyse based on the file extensions of each of the
// given files. Recursively adds any provided directories. For directories,
// only source files are added to avoid having facts in subdirectories added
// unintentionally. That means that .tao files must always be explicitly specified.
Analysis::Analysis(const vector<fs::path> &files, const fs::path &outputPath, int argc, const char **argv) {
	
	fs::path root(outputPath);
	root /= "objectFiles";
	if(!fs::exists(root))
	{
       fs::create_directories(root);     
    }
	
    for (const fs::path &currentPath : files) {
        if (fs::is_directory(currentPath)) {
            // Insert only the source files from the directory
            for (const auto &entry : fs::recursive_directory_iterator(currentPath)) {
                const fs::path entry_path = entry.path();
                if (fs::is_regular_file(entry_path)) {
                    addSourceFile(entry_path, root, argc, argv);
                }
            }
            sourceDirs.emplace(currentPath);
            continue;
        }

        if (currentPath.extension() == ".tao") {
            extraObjectFiles.push_back(currentPath);
            continue;
        }

        if (addSourceFile(currentPath, root, argc, argv)) {
            sourceDirs.emplace(currentPath.parent_path());
        } else {
            cerr << "Rex Ignoring: " << currentPath << " (unsupported file extension)" << endl;
        }
    }
}

// Returns the analysis jobs that need to be performed
Analysis::Job &Analysis::getJob(unsigned int index) {
    return jobs[index];
}

unsigned int Analysis::getNumJobs() const{
	return jobs.size();
}

bool Analysis::hasJobs() const{
	return !jobs.empty();
}

// Returns the list of directories that contain any of the source files being
// analysed. This is derived from the command line arguments:
// 1. For paths to files, this is the parent directory of each file
// 2. For paths to directories, this is the directory itself
const unordered_set<fs::path> &Analysis::getSourceDirectories() const {
    return sourceDirs;
}

// Returns the list of all object files that will be produced by jobs, including
// any additional object files that were also specified
vector<fs::path> Analysis::getAllObjectFiles() const {
    vector<fs::path> objFiles;
    for (const Analysis::Job &job : jobs) {
		if(job.status == Job::SUCCESS 
				|| job.status == Job::COMPLETE_WITH_ERROR)
		{
			objFiles.push_back(job.objectFilePath);
		}
		
    }

    objFiles.insert(objFiles.end(), extraObjectFiles.begin(), extraObjectFiles.end());
    return objFiles;
}

// Returns true if this analysis has extra object files that should be included
bool Analysis::hasExtraObjectFiles() const {
    return !extraObjectFiles.empty();
}

//TODO: Write a summary of extraction run
void Analysis::writeDiagnostics(const fs::path &outputPath) {
    (void) outputPath;
}

