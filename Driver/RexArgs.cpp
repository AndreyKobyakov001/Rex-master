#include "RexArgs.h"

#include <cassert>   // assert
#include <cstdlib>   // exit
#include <exception> // exception
#include <iostream>  // cerr, endl
#include <thread>    // hardware_concurrency

#include <boost/program_options.hpp>
#include <boost/dll.hpp>

using namespace std;
namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace dll = boost::dll;

struct validation_error : public exception {
    string message;

    explicit validation_error(string message) : message{message} {}

    const char *what() const throw() override {
        return message.c_str();
    }
};
RexArgs::RexArgs() : jobs{1}, prog{"./Rex"}, incremental{false} {}
RexArgs::RexArgs(const char* progPath) : jobs{1}, prog{progPath} {}

static void printHelp(const char *program_name, const po::options_description &desc) {
    cerr << "Usage: " << program_name << " [OPTIONS] <source0> <source1> ... <sourceN>" << endl;
    cerr << endl;
    cerr << "Run the Rex fact extractor on the given source files/directories." << endl;
    cerr << "A .tao file will be produced in the current directory for each" << endl;
    cerr << "source file analyzed." << endl;
    cerr << endl;
    cerr << "If used with the `-o` flag, you can also pass in .tao files to" << endl;
    cerr << "be linked into the result." << endl;
    cerr << endl;

    cerr << desc;
}

void RexArgs::validate() const {
    if (jobs < 1) {
        throw validation_error("Cannot have 0 jobs");
    }

    if (inputPaths.empty()) {
        throw validation_error("Must provide at least one source file/directory");
    }
}
string RexArgs::configToString(RexArgs::ConfigType type)
{
    string result;
    switch(type)
    {
        case RexArgs::VARIABILITY:
            result = "variability";
            break;
        
        case RexArgs::LANG_FEATURES:
            result = "langFeatures";
            break;

        case RexArgs::GUIDED_EXTRACTION:
            result = "guidedExtraction";
            break;
        
        case RexArgs::ROS_FEATURES:
            result = "ROSFeatures";
            break;
        
        default:
            throw domain_error("Unknown config type");
    }
    return result;
}
RexArgs::ConfigType RexArgs::stringToConfig(string type)
{
    if(type == "variability")
    {
        return VARIABILITY;
    }
    else if (type == "langFeatures") {
        return LANG_FEATURES;
    }
    else if (type == "ROSFeatures") {
        return ROS_FEATURES;
    }
    else if (type == "guidedExtraction") {
        return GUIDED_EXTRACTION;
    }
    throw domain_error("Unknown config type");
}
// Read the configuration used to generate
// the object file "objFile"
//
// Returns the last time the corresponding source file
// was written to
time_t RexArgs::readPreviousConfig(const fs::path &objFile, ExtractionConfig &config)
{
    time_t lastModified;
    fs::ifstream obj(objFile);
    for(int i = 0; i < ConfigType::COUNT; i++)
    {
        obj >> config;
    }
    obj >> lastModified;
    return lastModified;
}
// Extracts the configuration for this program from the command line arguments.
//
// If the help flag is found or if the arguments cannot be parsed, this function
// will exit the program immediately.
RexArgs RexArgs::processArgs(int argc, const char **argv) {
    RexArgs args(argv[0]);
    int cpus = thread::hardware_concurrency() - 1;

    // The API of boost::program_options is not actually that great. There is
    // no way to do certain things like:
    //
    // * have a description for the positional argument
    //   (https://stackoverflow.com/a/39934380/551904)
    // * not represent positional arguments as an extra flag argument (see files)
    // * do validation (e.g. source files not empty)
    //
    // We work around all of this, but it isn't exactly pretty and can be
    // somewhat error prone.
    po::options_description desc("OPTIONS");
    auto add_opt = desc.add_options();
    VariabilityOptions *vOpts = new VariabilityOptions;
    fs::path configFilePath;
    add_opt("help,h", "Produce this help message");
    add_opt("jobs,j",
        po::value<unsigned int>(&args.jobs)->default_value(args.jobs)->implicit_value(cpus),
        "The number of concurrent jobs to run. If no value is provided for this "
        "argument, it will be determined automatically.");
     add_opt("output,o", po::value<fs::path>(&args.outputPath)->default_value("")->implicit_value("./out.ta"),
        "Name of the generated TA file (with file extension). Linking will "
        "be performed if this argument is provided. "
        "This option conflicts with --neo4j,-n and --neo4j-csv,-c.");
    add_opt("neo4j,n", po::value<fs::path>(&args.neo4jCypherPath)
            ->default_value("")
            ->implicit_value("./out.cypher"),
        "Name of the generated Neo4j Cypher MERGE statements file (with file extension). Linking will "
        "be performed if this argument is provided. "
        "This option conflicts with --output,-o and --neo4j-csv,-c.");
    add_opt("neo4j-csv,c", po::value<vector<fs::path>>(&args.neo4jCsvPaths)->multitoken()
            ->default_value(vector<fs::path>{}, "")
            ->implicit_value(vector<fs::path>{"./nodes.csv", "./edges.csv"}, "./nodes.csv ./edges.csv"),
            "Name of the 2 generated CSV files for Neo4j bulk import (with file extension). "
            "The first parameter is the CSV file for nodes. "
            "The second parameter is the CSV file for edges. "
            "Names cannot start with -(dash). "
            "Linking will be performed if this argument is provided. "
            "This option conflicts with --output,-o and --neo4j,-n.");
     add_opt("barebones,b", po::value<bool>(&args.clangOnly)->default_value(false)->implicit_value(true),
        "Flag for running a barebones version of Rex that only walks AST.");
     add_opt("incremental,i", po::value<bool>(&args.incremental)->default_value(false)->implicit_value(true),
        "Enable Incremental Linking");
     add_opt("configFile,F", po::value<fs::path>(&configFilePath)->default_value(""),
        "Configuration file path");
     add_opt("headerLocation,H", po::value<vector<fs::path>>(&args.headerPaths)->multitoken()
            ->default_value(vector<fs::path>{}, ""),
              "Other header file directories.");
    add_opt("clangFlags,f", po::value<vector<string>>(&args.clangFlags)->multitoken()
            ->default_value(vector<string>{}, ""),
              "Other clang flags.");
     add_opt("barebones,b", po::value<bool>(&args.clangOnly)->default_value(false)->implicit_value(true),
        "Flag for running a barebones version of Rex that only walks AST.");   
     add_opt("globalsOnly,G", po::value<bool>(&vOpts->globalsOnly)->default_value(false)->implicit_value(true),
             "Flag for considering only global variables to be configuration variables. "
              "This option conflicts with --nonglobalsOnly,-g.");
     add_opt("nonglobalsOnly,g", po::value<bool>(&vOpts->nonglobalsOnly)->default_value(false)->implicit_value(true),
             "Flag for considering only nonglobal variables to be configuration variables. "
              "This option conflicts with --globalsOnly,-G.");
     add_opt("ints,I", po::value<bool>(&vOpts->ints)->default_value(false)->implicit_value(true),
             "Flag for considering integers to be configuration variables.");
     add_opt("enums,E", po::value<bool>(&vOpts->enums)->default_value(false)->implicit_value(true),
             "Flag for considering enum variables to be configuration variables.");
     add_opt("bools,B", po::value<bool>(&vOpts->bools)->default_value(false)->implicit_value(true),
             "Flag for considering boolean variables to be configuration variables.");
     add_opt("chars,C", po::value<bool>(&vOpts->chars)->default_value(false)->implicit_value(true),
             "Flag for considering character variables to be configuration variables.");
     add_opt("nameMatch,N", po::value<string>(&vOpts->nameMatch)->default_value(""),
             "Flag for providing variable name match regex for what is a configuration variable");
     add_opt("variability_On,V", po::value<bool>(&vOpts->variability_On)->default_value(false)->implicit_value(false),
              "Flag for turning Variability on without setting other variability options. ");
    add_opt("cfg", po::value<bool>(&args.buildCFG)->default_value(false)->implicit_value(true),
             "Flag for building and including CFG information");
     add_opt("extract_All_PCs,A", po::value<bool>(&vOpts->extractAll)->default_value(false)->implicit_value(true),
              "Flag for choosing to extract all conditions as presence conditions, including function calls.");

    // HACK: Prevent positional options from showing up in help message
    // See: https://stackoverflow.com/a/39934380/551904
    po::options_description pos_desc;
    pos_desc.add_options()("source",
        po::value<vector<fs::path>>(&args.inputPaths),
        "The input files/directories to analyze");
    // Create a composite option_description just for parsing
    po::options_description full_desc;
    full_desc.add(desc).add(pos_desc);

    // Accept any number of positional arguments
    po::positional_options_description pos_opts;
    // Name here matches the other source argument declared above
    pos_opts.add("source", -1);

    // Parsed arguments will be read into this variable
    po::variables_map vm;
    try {
        auto parsed = po::command_line_parser(argc, argv).options(full_desc).positional(pos_opts).run();
        po::store(parsed, vm);
        // Trigger notifiers, if any
        po::notify(vm);

        // Check for help before we validate
        if (vm.count("help")) {
            printHelp(argv[0], desc);
            exit(0);
        }

        // Run validations
        args.validate();
    } catch (exception &e) {
        cerr << "Error while parsing command line arguments:" << endl;
        cerr << e.what() << endl
             << endl;
        printHelp(argv[0], desc);
        exit(1);
    }

    // Must check if this is empty or else we will take the absolute path of an
    // empty string and get the current working directory
    if (!args.outputPath.empty()) {
        // ClangTool will change the current working directory to whatever it wants
        // during its analysis. Sometimes (if the right data race occurs), it won't
        // reset the CWD back to what it was before. Thus, it is absolutely
        // imperative that this path be absolute. If this is a relative path, the
        // file will end up wherever clang decides the current directory should be.
        args.outputPath = fs::absolute(args.outputPath);
    }

    // get absolute path of the header file locations
    vector<boost::filesystem::path> absolutePaths = vector<boost::filesystem::path>();
    for (boost::filesystem::path relative : args.headerPaths) {
        //cout << relative.string() << endl;
        //cout << fs::absolute(relative).string() << endl;
        absolutePaths.push_back(fs::absolute(relative));
    }
    args.headerPaths = absolutePaths;

    //If the user does not specify the configFile path, look for a configFile at the folder where Rex is built 
    if (configFilePath.empty()) {
        cout << "No configuration file specified. Defaulting to Rex executable directory." << endl;
        configFilePath = dll::program_location().parent_path().append("extraction.conf");
    }
    cout << "Attempting to read configuration file located at: " + configFilePath.string() << endl;
    ifstream inFile{configFilePath.string()};
    
    // Transfer ownership of Configuration ptrs to an instance
    // of ExtractionConfig
    inFile >> args.config;
    // Temporary hack to get around the fact that the config file overwrites everything...
    delete &args.config.get(configToString(VARIABILITY));
    args.config.addOption(RexArgs::configToString(RexArgs::VARIABILITY), vOpts);
    return args;
}

// The number of parallel jobs to run, guaranteed to be greater than 0.
unsigned int RexArgs::getParallelJobs() const {
    assert(jobs > 0); // Check guarantee
    return jobs;
}

// The input files to process, guaranteed to be non-empty.
const std::vector<boost::filesystem::path> &RexArgs::getInputPaths() const {
    assert(!inputPaths.empty()); // Check guarantee
    return inputPaths;
}

// The input files to process, guaranteed to be non-empty.
const std::vector<boost::filesystem::path> &RexArgs::getHeaderPaths() const {
    return headerPaths;
}

// The input files to process, guaranteed to be non-empty.
const std::vector<std::string> &RexArgs::getClangFlags() const {
    return clangFlags;
}

// The destination path of the linked TA file. Empty if no path was provided.
const boost::filesystem::path &RexArgs::getOutputPath() const {
    return outputPath;
}

const string RexArgs::getProg() const{
    return prog;
}

bool RexArgs::isBareBones() const{
    return clangOnly;
}
bool RexArgs::isIncremental() const{
    return incremental;
}

bool RexArgs::shouldBuildCFG() const{
    return buildCFG;
}

VariabilityOptions RexArgs::getVariabilityOptions() const
{
    const ConfigOption &opt = config.get(configToString(VARIABILITY));
    return *dynamic_cast<const VariabilityOptions*>(&opt);
}

LanguageFeatureOptions RexArgs::getLanguageFeaturesOptions() const
{
    const ConfigOption &opt = config.get(configToString(LANG_FEATURES));
    return *dynamic_cast<const LanguageFeatureOptions*>(&opt);
}

ROSFeatureOptions RexArgs::getROSFeaturesOptions() const
{
    const ConfigOption &opt = config.get(configToString(ROS_FEATURES));
    return *dynamic_cast<const ROSFeatureOptions*>(&opt);
}

GuidedExtractionOptions RexArgs::getGuidedExtractionOptions() const
{
    const ConfigOption &opt = config.get(configToString(GUIDED_EXTRACTION));
    return *dynamic_cast<const GuidedExtractionOptions*>(&opt);
}

// The destination path of the generated Neo4j Cypher MERGE statements. Empty if no path was provided.
const boost::filesystem::path &RexArgs::getNeo4jCypherPath() const{
    return neo4jCypherPath;
}

// The destination paths of the generated CSV files for Neo4j bulk import. Empty if no path was provided.
const std::vector<boost::filesystem::path> &RexArgs::getNeo4jCsvPaths() const{
    return neo4jCsvPaths;
}
