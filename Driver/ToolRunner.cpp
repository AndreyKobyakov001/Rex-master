#include "ToolRunner.h"

#include <vector>
#include <iostream>

#include <boost/filesystem/fstream.hpp> // fs::ofstream
#include <clang/Tooling/CommonOptionsParser.h>
#include "llvm/Support/CommandLine.h"
#include <clang/Tooling/Tooling.h>

#include "../Graph/TAGraph.h"
#include "../Linker/TAObjectWriter.h"
#include "../Walker/ROSConsumer.h"
#include "RexArgs.h"

using namespace std;
using namespace clang::tooling;
using namespace llvm::cl;
namespace fs = boost::filesystem;

// Retrieves the ROS_PACKAGE_NAME from a list of compilation commands. Expects
// that there will only be a single command in the given vector. If there was
// more than one, we might have multiple candidates for the package name.
static string extractPackageName(const vector<CompileCommand> &compCommands) {
    static string PACKAGE_NAME_ARG_START = "-DROS_PACKAGE_NAME=\"";
    static string PACKAGE_NAME_ARG_END = "\"";

	// Found a case in autonomoose where this occurs (anm_lidar_matcher)
    //~ if (compCommands.size() != 1) {
        //~ throw logic_error("Expected only a single compile command for each file");
    //~ }

    const CompileCommand &cmd = compCommands[0];

    string packageName = "";
    for (const string &arg : cmd.CommandLine) {
        // If this is the right argument, its value will be between these indexes
        unsigned int argValueStart = PACKAGE_NAME_ARG_START.size();
        unsigned int argValueSize = arg.size() - argValueStart - PACKAGE_NAME_ARG_END.size();

        // Check if the string actually matches what we want
        string argStart = arg.substr(0, argValueStart);
        string argEnd = arg.substr(argValueStart + argValueSize,
            PACKAGE_NAME_ARG_END.size());

        if (argStart == PACKAGE_NAME_ARG_START && argEnd == PACKAGE_NAME_ARG_END) {
            packageName = arg.substr(argValueStart, argValueSize);
            break;
        }
    }

    if (packageName.empty()) {
        cerr << "Rex Warning: Unable to find package name for file: '" << cmd.Filename << "'" << endl;
        //cerr << "    No compile command was found in compile_commands.json." << endl;
        packageName = "unknownFeature";
    }

    return packageName;
}

ToolRunner::ToolRunner(const Analysis::Job &job, const IgnoreMatcher &ignored, const RexArgs &args):
    job{job}, ignored{ignored}, args{args} {}

// Run the given analysis job through clang
//
// Returns true if clang finished without any errors
int ToolRunner::run() const {

	
	// If object file already exists, need to compare the time stamp of the tao file and the cpp file
	if(boost::filesystem::exists(job.objectFilePath))
    {
        // Capture the last_write_time of the tao file
        std::time_t tao_time = boost::filesystem::last_write_time(job.objectFilePath.string());

        // Capture the last_write_time of the cpp file
        const char* argv[] = {job.sourcePath.c_str()};
        std::time_t cpp_time = boost::filesystem::last_write_time(argv[0]);

        // Compare the last_write_time of tao file and cpp file
        // If tao_time > cpp_time: after the tao file has been generated, the cpp file has not been modified, then display the "already exists" message
        // If cpp_time > tao_time: after the tao file has been generated, the cpp file also has been updated, then need to update the tao file as well
        if (tao_time > cpp_time) {
            cout << job.objectFilePath.string() << " already exists" << endl;
		    return 0;
        }
	}
    // CLANG_INC_DIR is a preprocessor variable passed in CMakeLists.txt.
    // Taking advantage of C++ concatenating adjacent string literals.
    ArgumentsAdjuster arg1 = getInsertArgumentAdjuster("-I" CLANG_INC_DIR);
    // It turns out that clang uses some GCC headers? (e.g. omp.h)
    // To see which version of gcc clang has selected for you, run `clang -v`
    // "Selected GCC installation" will tell you which directory to put here.
    ArgumentsAdjuster arg2 = getInsertArgumentAdjuster("-I/usr/lib/gcc/x86_64-linux-gnu/7.5.0/include");
    // Suppresses all warnings (but not errors)
    ArgumentsAdjuster arg3 = getInsertArgumentAdjuster("-w");
    // Ensures ClangTool will process file regardless of number of encountered errors
    ArgumentsAdjuster arg4 = getInsertArgumentAdjuster("-ferror-limit=0");

    // Ensures (Qt5 in Conda works)
    ArgumentsAdjuster arg5 = getInsertArgumentAdjuster("-fPIC");

    string error = "Unable to find compilation database";
    auto autoCompDb = CompilationDatabase::autoDetectFromSource(
        job.sourcePath.string(),
        error
    );
	vector<string> sourcePathList{job.sourcePath.string()};

	// We need to account for source files that do not have an accompanying
	// compilation database. After reading through Clang API and source code,
	// this seems like the best way to do it, i.e. by moking a command line
	// invokation of our tool e.g. ./Rex srcFile --
	int argc = 3;
	const char* argv[] = {args.getProg().c_str(), job.sourcePath.c_str(), "--"};
	// Clang uses "--" to parse additional compilation flags. Without it,
	// this line will return a nullptr. See https://clang.llvm.org/doxygen/classclang_1_1tooling_1_1FixedCompilationDatabase.html
	// for more details
	unique_ptr<FixedCompilationDatabase> fixedCompDb = FixedCompilationDatabase::loadFromCommandLine(argc, argv, error);
	CompilationDatabase* compDb = fixedCompDb.get();
	if(autoCompDb)
	{
		cout << "Auto Detected a compilation database!" << endl;
		compDb = autoCompDb.get();
	}
	else
	{
		cout << "Could not auto detect a compilation database!" << endl;
		cout << "Using a default-generated compilation database!" << endl;
	}


    ClangTool tool(*compDb, sourcePathList);
    // The order of these argument adjusters is important! We need CLANG_INC_DIR
    // to be added before the GCC directory or everything will go wrong.
    tool.appendArgumentsAdjuster(arg1);
    tool.appendArgumentsAdjuster(arg2);
    tool.appendArgumentsAdjuster(arg3);
    tool.appendArgumentsAdjuster(arg4);
    tool.appendArgumentsAdjuster(arg5);

    for (const std::string clangFlags : args.getClangFlags()) {
        const string clangFlagStr = "-" + clangFlags;
        ArgumentsAdjuster clangFlag = getInsertArgumentAdjuster(clangFlagStr.c_str());
        tool.appendArgumentsAdjuster(clangFlag);
    }

    // Adding header file path to Clang Tool
    //cout << "Source file: " << job.sourcePath.string() << endl;
    //cout << "Header files: " << endl;
    for (const fs::path &headerPath : args.getHeaderPaths()) {
        const string headerPathString = "-I" + headerPath.string();
        //cout << headerPathString  << endl;
        ArgumentsAdjuster argHeader = getInsertArgumentAdjuster(headerPathString.c_str());
        tool.appendArgumentsAdjuster(argHeader);
    }
    //cout << endl;

    // The "feature" or "component" is extracted from the compilation database.
    // We use the ROS_PACKAGE_NAME preprocessor directive passed to each file in
    // autonomoose. In the future, we may want to get this from a configuration
    // file instead so we aren't coupled to anything ROS-specific.
    vector<CompileCommand> compCommands = compDb->getCompileCommands(job.sourcePath.string());
    string featureName = extractPackageName(compCommands);

    // Need to pass a graph from here because there is no way to retrieve
    // anything once ClangTool has run (as far as we know)
    TAGraph graph;
    WalkerConfig config(graph, ignored, featureName, args.isBareBones(), args.shouldBuildCFG(), args.getVariabilityOptions(), args.getLanguageFeaturesOptions(), args.getGuidedExtractionOptions(), args.getROSFeaturesOptions());
    ROSFrontendActionFactory factory(config);
    int code = tool.run(&factory);

	// Store the generated graph in out special object file format
	TAObjectWriter objFileContents(graph);
	fs::ofstream objFile(job.objectFilePath);
    objFile.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);
    if (!objFile.is_open()) {
        throw new runtime_error("Failed to open obj file: " + job.objectFilePath.string());
    }
    objFile << objFileContents;
    cout << "Wrote " << job.sourcePath.string() << " to " << job.objectFilePath.string() << endl;
    return code;
}
