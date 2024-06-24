/////////////////////////////////////////////////////////////////////////////////////////////////////////
// RexMaster.cpp
//
// Created By: Bryan J Muscedere
// Date: 13/05/17.
//
// Driver source code for the Rex program. Handles
// commands and passes it off to the RexHandler to
// translate it into a Rex action. Also handles
// errors gracefully.
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

#include <chrono>   // high_resolution_clock
#include <iostream> // cout
#include <mutex>    // mutex
#include <string>   // string, getline
#include <thread>   // thread
#include <vector>   // vector
#include <sys/wait.h> 

#include <boost/filesystem.hpp>

#include "Analysis.h"
#include "RexArgs.h"
#include "IgnoreMatcher.h"
#include "ToolRunner.h"
#include "ThrowsWithTrace.h"
#include "../Linker/Linker.h"
#include "../Graph/TAGraph.h"
#include "../Linker/TAWriter.h"
#include "../Linker/CSVWriter.h"
#include "../Linker/CypherWriter.h"

#include <semaphore.h>
#include <fcntl.h>

#include <chrono>
#include <ctime>  

#define FAIL_LOG_SEM_NAME "failLogSem"


using namespace std;
namespace fs = boost::filesystem;


// This is ugly, but I needed a way to access
// the current job for each process when it crashes.
// Since the child processes have their own memory,
// this job is unique to each process
Analysis::Job *currJob;

// This is ugly, but I couldn't think of another
// way to log failures that occured due to a
// signal being raised (e.g. SIGSEGV)
fs::ofstream *globFailLogWriter;
fs::path *globFailLogPath;

// extraction and linking logging
fs::ofstream *infoLogWriter;
fs::path *infoLogPath;

// This is intended for handling errors with
// extraction processes ONLY. Do not use this
// signal handler for anything else
void handleSignal(int signum)
{
    // Ignore warning for unused param.
    (void) signum;
	sem_t *failureLogMutex = sem_open(FAIL_LOG_SEM_NAME, 0);
	// Need to synchronize concurrent writes to log file
	sem_wait(failureLogMutex);
	globFailLogWriter->open(*globFailLogPath, ios::app);
	*globFailLogWriter << "Failure on " << currJob->sourcePath.string() << endl;
	*globFailLogWriter << boost::stacktrace::stacktrace() << endl;
	globFailLogWriter->close();
	sem_post(failureLogMutex);
	raise(SIGTERM);
}


// Run all of the given jobs in parallel.
//
// Returns true if all clang invocations succeeded without any errors
static bool runParallelJobs(const RexArgs &args, Analysis &analysis, const IgnoreMatcher &ignored) {
    // Goal: Achieve maximum concurrency by having a simple job "queue" that
    // allows threads to take the next job as soon as they are ready.
    //
    // An altnerative approach would be to split the jobs evenly among the
    // threads. This doesn't achieve the greatest possible speed because we
    // can't assume that analyzing every file will take the same amount of time.

	unsigned int nthreads = args.getParallelJobs();

    // State shared between threads protectex by a mutex
    mutex sharedState;
    unsigned int nextJob = 0;
    bool allSuccess = true;


	fs::path failLogPath(args.getOutputPath().parent_path());
	failLogPath /= "logs";
	failLogPath /= "failuresWithTraces.dump";
	fs::create_directories(failLogPath.parent_path());
	if(fs::exists(failLogPath))
	{
		fs::remove(failLogPath);
	}
	fs::ofstream failLogWriter;
	
	globFailLogWriter = &failLogWriter;
	globFailLogPath = &failLogPath;
	signal(SIGABRT, &handleSignal);
	signal(SIGSEGV, &handleSignal);
	
    vector<thread> threads;
    threads.reserve(nthreads);
    cout << "Instantiating " << nthreads << " workers" << endl;
    for (unsigned int i = 0; i < nthreads; i++) {
        cout << "Spawning worker thread #" << (i + 1) << endl;
        threads.push_back(thread(
            [&sharedState, &nextJob, &allSuccess, &analysis, &ignored, &args, &failLogPath, &failLogWriter]() {
                // true if the last job that was run was successful
                bool lastSuccess = true;
                while (true) {
                    sharedState.lock();

                    allSuccess = allSuccess && lastSuccess;
                    if (nextJob >= analysis.getNumJobs()) {
                        sharedState.unlock();
                        break;
                    }
                    unsigned int currentJob = nextJob;
                    nextJob++;
                    Analysis::Job &myJob = analysis.getJob(currentJob);
                    cout << "[" << (currentJob + 1) << "/" << analysis.getNumJobs() << "] ";
                    cout << "Analyzing " << myJob.sourcePath << endl;
                    currJob = &myJob;
                    sharedState.unlock();
                    
                    int childStatus;
					pid_t child_pid = fork();
                    //pid_t child_pid = 0;
					if(child_pid == 0)
					{
						// In child process
						ToolRunner jobRunner(myJob, ignored, args);
						int exitStatus;				
						try
						{
							exitStatus = jobRunner.run();
						}
						catch(const exception &e)
						{
							sem_t *failureLogMutex = sem_open("failLogSem", 0);
							sem_wait(failureLogMutex);
							failLogWriter.open(failLogPath, ios::app);
							failLogWriter << "Failure on " << myJob.sourcePath.string() << ": " << e.what() << endl;
							const boost::stacktrace::stacktrace* st = boost::get_error_info<traced>(e);
							if (st) 
							{
								failLogWriter << *st << endl;
							}
							else
							{
								failLogWriter << "Stack unavailable. Did you try throw_with_trace?" << endl;
							}
							failLogWriter.close();
							sem_post(failureLogMutex);
							raise(SIGTERM);
						}
						catch(...)
						{
							sem_t *failureLogMutex = sem_open("failLogSem", 0);
							sem_wait(failureLogMutex);
							failLogWriter.open(failLogPath, ios::app);
							failLogWriter << "Unknown Failure on " << myJob.sourcePath.string() << endl;
							failLogWriter.close();
							sem_post(failureLogMutex);
							raise(SIGTERM);
						}
						exit(exitStatus);
					}
					else
					{						
						// This suspends execution of the calling thread (and not process) until the
						// child process with id 'child_pid' has terminated. If the child process has already terminated, 
						// then the call returns immediately
						waitpid(child_pid, &childStatus, 0);
						// Child process exits gracefully
						if(WIFEXITED(childStatus))
						{
							// No Compilation Errors
							if(WEXITSTATUS(childStatus) == EXIT_SUCCESS)
							{
								myJob.status = Analysis::Job::Status::SUCCESS;
							}
							// Compilation Errors
							else
							{
							    myJob.status = Analysis::Job::Status::COMPLETE_WITH_ERROR;
							}
						}
						// Child process crashed or an exception was raised
						else
						{
							myJob.status = Analysis::Job::Status::FAIL;
							lastSuccess = false;
						}						
					
					}
                }
            }
        ));
    }

    for (unsigned int i = 0; i < threads.size(); i++) {
        threads[i].join();
        cout << "Stopping worker thread #" << (i + 1) << endl;
    }
    signal(SIGABRT, SIG_DFL);
	signal(SIGSEGV, SIG_DFL);
    return allSuccess;
}


int main(int argc, const char **argv) {
    using namespace std::chrono;
    
	sem_open(FAIL_LOG_SEM_NAME, O_CREAT, 0644, 1);
	
    RexArgs args = RexArgs::processArgs(argc, argv);
    const fs::path &outputPath = args.getOutputPath();
    const fs::path &neo4jCypherPath = args.getNeo4jCypherPath();
    vector<fs::path> neo4jCsvPaths = args.getNeo4jCsvPaths();

    bool outputTA = !outputPath.empty();
    bool outputCypher = !neo4jCypherPath.empty();
    bool outputCSVs = !neo4jCsvPaths.empty();

    int linkingOptions = 0;
    if (outputTA) { linkingOptions++; }
    if (outputCypher) { linkingOptions++; }
    if (outputCSVs) { linkingOptions++; }

    // conflicting options: only output one type of format
    if (linkingOptions > 1) {
      cerr << "Rex Error: " << "At most one output format could be selected." << endl;
      return 1;
    }

    bool performLinking = linkingOptions == 1;

    if (outputCSVs) {
      if (neo4jCsvPaths.size() > 2) {
        cerr << "Rex Error: " << "Invalid number of CSV filenames." << endl;
        return 1;
      }
      if (neo4jCsvPaths.size() == 1) {
        neo4jCsvPaths.push_back(fs::path{"edges.csv"});
      }
    }

    Analysis analysis(args.getInputPaths(), args.getOutputPath().parent_path(), argc, argv);
    if (!performLinking && analysis.hasExtraObjectFiles()) {
        cerr << "Rex Warning: Provided .tao files even though not linking (no output filename)" << endl;
    }

    // create information.log file in output path
    fs::path infoLogPath(args.getOutputPath().parent_path());
	infoLogPath /= "information.log";
	fs::ofstream infoLogWriter;
    infoLogWriter.open(infoLogPath, ios::app);

    // create csv log file
    fs::path infoCSVPath(args.getOutputPath().parent_path());
    infoCSVPath /= "information.csv";
    string header = "";
    if (!fs::exists(infoCSVPath)) {
        header = "command-line,extraction-time,linking-time\n";
    }
    fs::ofstream infoCSVWriter;
    infoCSVWriter.open(infoCSVPath, ios::app);
    infoCSVWriter << header;

    // log command line arguments to information.log file to differentiate the different calls
    for(int i=0;i<argc;i++) {
        infoLogWriter << argv[i] << " ";
        infoCSVWriter << argv[i] << " ";
    }
    infoLogWriter << endl;
    infoCSVWriter << ",";

    if (analysis.hasJobs()) {
        // Time how long extration takes
        high_resolution_clock::time_point start = high_resolution_clock::now();
        auto start_sys = std::chrono::system_clock::now();
        std::time_t start_time = std::chrono::system_clock::to_time_t(start_sys);
        infoLogWriter << "Starting Extraction: " << std::ctime(&start_time);

        IgnoreMatcher ignored;
        for (const fs::path &sourceDir : analysis.getSourceDirectories()) {
            ignored.addSourceDir(sourceDir);
        }

        if (!runParallelJobs(args, analysis, ignored)) {
            cout << "Rex Warning: Some files failed to compile. You may want to re-run after fixing the errors." << endl;
        }

        // print extraction time in command line
        high_resolution_clock::time_point end = high_resolution_clock::now();
        //auto duration = duration_cast<std::chrono::milliseconds>(end - start).count();
        std::chrono::duration<double, std::milli> duration = end - start;
        auto end_sys = std::chrono::system_clock::now();

        // log extraction time in information.log file directly within the build directory
        std::time_t end_time = std::chrono::system_clock::to_time_t(end_sys);
        cout << "Completed in " << duration.count() << " milliseconds" << endl;
        infoLogWriter << std::ctime(&end_time)  << "Completed in " << duration.count() << " milliseconds" << endl;
        infoCSVWriter << duration.count() << ",";
        infoLogWriter << endl;
    }

    if (performLinking) {
        // Time how long linking takes
        high_resolution_clock::time_point start = high_resolution_clock::now();
        auto start_sys = std::chrono::system_clock::now();
        std::time_t start_time = std::chrono::system_clock::to_time_t(start_sys);
        infoLogWriter << "Starting Linking: " << std::ctime(&start_time);

        const vector<fs::path> taoFiles = analysis.getAllObjectFiles();
        cout << "Linking " << taoFiles.size() << " object files..." << endl;

        if (outputTA) {
          fs::ofstream outputFile(outputPath);
          TAWriter taFile(outputFile);
          linkObjectFiles(taoFiles, taFile);
        }

        if (outputCSVs) {
          fs::ofstream nodesFile(neo4jCsvPaths[0]);
          fs::ofstream edgesFile(neo4jCsvPaths[1]);
          CSVWriter csvFiles(nodesFile, edgesFile);
          linkObjectFiles(taoFiles, csvFiles);
        }

        if (outputCypher) {
          fs::ofstream outputFile(neo4jCypherPath);
          CypherWriter cypherFile(outputFile);
          linkObjectFiles(taoFiles, cypherFile);
        }

        // print linking time in commandline
        high_resolution_clock::time_point end = high_resolution_clock::now();
        //auto duration = duration_cast<std::chrono::milliseconds>(end - start).count();
        std::chrono::duration<double, std::milli> duration = end - start;
        cout << "Finished linking in " << duration.count() << " milliseconds" << endl;
        
        // log linking time in information.log file directly within the build directory
        auto end_sys = std::chrono::system_clock::now();
        std::time_t end_time = std::chrono::system_clock::to_time_t(end_sys);
        infoLogWriter << std::ctime(&end_time) << "Finished linking in " << duration.count() << " milliseconds" << endl;
        infoLogWriter << endl;
        infoCSVWriter << duration.count() << endl;
    }

    infoLogWriter.close();
    infoCSVWriter.close();
    return 0;
}
