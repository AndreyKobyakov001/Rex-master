set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

cmake_minimum_required(VERSION 3.3 FATAL_ERROR)
project(Rex)


find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

find_package(Boost 1.60 COMPONENTS system filesystem serialization regex program_options stacktrace_backtrace stacktrace_addr2line REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

message(STATUS "Found Boost LIBS ${Boost_LIBRARY_DIR}")

# Sets up LLVM.
if(DEFINED ENV{CLANG_VER})
	set(CLANG_VER $ENV{CLANG_VER})
else()
	message( FATAL_ERROR "The CLANG_VER environment variable is not set. Please set it before compiling Rex." )
endif()
if(DEFINED ENV{LLVM_PATH})
	set(LLVM_PATH $ENV{LLVM_PATH})
else()
	message( FATAL_ERROR "The LLVM_PATH environment variable is not set. Please set it before compiling Rex." )
endif()


link_directories(${LLVM_PATH}/lib)
include_directories(${LLVM_PATH}/include)

# set(CLANG_INCLUDE_DIR ${LLVM_PATH}/lib/clang/${CLANG_VER}/include)
set(CLANG_INCLUDE_DIR "/home/andrey/Clang-Build/lib64/clang/8.0.0/include")  # Update with the correct path
# set(REX_INCLUDE_DIR "/home/andrey/Rex-master/include")

add_definitions(
    -D__STDC_LIMIT_MACROS
    -D__STDC_CONSTANT_MACROS
    -DBOOST_STACKTRACE_LINK
    -DBOOST_STACKTRACE_USE_ADDR2LINE
    -DBOOST_STACKTRACE_USE_BACKTRACE
)
add_definitions(
	-DCLANG_INC_DIR="${CLANG_INCLUDE_DIR}"
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -lstdc++ -std=c++17 -fPIC -g -rdynamic -Wextra -Wall -Wno-unused-variable -Wno-unused-parameter -Wno-comment -Wno-reorder")

set(SOURCE_FILES
	Graph/TAGraph.cpp
	Graph/TAGraph.h
	Graph/RexNode.cpp
	Graph/RexNode.h
	Graph/RexEdge.cpp
	Graph/RexEdge.h

	Driver/RexMaster.cpp
  Driver/RexArgs.h
  Driver/RexArgs.cpp
  Driver/Analysis.h
  Driver/Analysis.cpp
  Driver/ToolRunner.h
  Driver/ToolRunner.cpp
  Driver/IgnoreMatcher.h
  Driver/IgnoreMatcher.cpp
  Driver/ExtractionConfig.h
  Driver/ExtractionConfig.cpp

	Walker/RexID.cpp
	Walker/RexID.h
	Walker/ParentWalker.cpp
	Walker/ParentWalker.h
	Walker/ROSWalker.cpp
	Walker/ROSWalker.h
	Walker/BaselineWalker.h
	Walker/BaselineWalker.cpp
	Walker/ROSConsumer.cpp
	Walker/ROSConsumer.h
    Walker/CondScope.cpp
    Walker/CondScope.h
    Walker/VarWalker.h
    Walker/VarWalker.cpp

	Linker/Linker.h
	Linker/Linker.cpp
	Linker/LinkAttrs.h
	Linker/TAObjectWriter.h
	Linker/TAObjectWriter.cpp
	Linker/TAObjectFile.h
	Linker/TAObjectFile.cpp
	Linker/TAWriter.h
	Linker/TAWriter.cpp
	Linker/TASchema.h
	Linker/TASchema.cpp
	Linker/TASchemeAttribute.h
	Linker/TASchemeAttribute.cpp
	Linker/CSVWriter.h
	Linker/CSVWriter.cpp
	Linker/CypherWriter.h
	Linker/CypherWriter.cpp

	JSON/jsoncpp.cpp
	JSON/json-forwards.h
	JSON/json.h

	XML/rapidxml.hpp
	XML/rapidxml_iterators.hpp
	XML/rapidxml_print.hpp
	XML/rapidxml_utils.hpp
)

add_executable(Rex ${SOURCE_FILES})

file(GENERATE OUTPUT "extraction.conf" CONTENT "/* Rex Configuration File */
{
  /* Extraction configuration options */
  \"langFeatures\": { \"cClass\": true,
		    \"cStruct\": true,
		    \"cUnion\": true,
		    \"cEnum\": true,
		    \"cFunction\": true,
		    \"cVariable\": true },
  \"variability\": { \"globalsOnly\": false,
		   \"ints\": false,
		   \"enums\": false,
		   \"bools\": false,
		   \"chars\": false,
		   \"nameMatch\": \"\" },
  \"guidedExtraction\": { \"callbackFuncRegex\": \"MngPTCS.*\" },
  \"ROSFeatures\": { \"appendFeatureName\": false }

}")


target_link_libraries(Rex
        clangFrontend
        clangSerialization
        clangDriver
        clangParse
        clangSema
        clangAnalysis
        clangAST
        clangASTMatchers
        clangBasic
        clangEdit
        clangLex
        clangTooling
        )

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs all)

# Link against LLVM libraries
target_link_libraries(Rex ${llvm_libs})

include(FindCurses)
target_link_libraries(Rex
        pthread
        z
        dl
        crypto
        backtrace
        ${CURSES_LIBRARIES}
        )

target_link_libraries(Rex
        ${Boost_LIBRARIES}
        )

# Run cppcheck - sudo apt-get install cppcheck
# Use `./run-cppcheck.sh` to run this
find_program(CPPCHECK NAMES cppcheck)
if (CPPCHECK)
    add_custom_target(
        cppcheck
        COMMAND ${CPPCHECK}
        "--enable=all"
        "--inconclusive"
        "--force"
        "--inline-suppr"
        "--suppress=missingIncludeSystem" # https://stackoverflow.com/a/19779624/551904
        "--verbose"
        "--quiet"
        "-D__STDC_LIMIT_MACROS"
        "-D__STDC_CONSTANT_MACROS"
        "--suppressions-list=${CMAKE_SOURCE_DIR}/CppCheckSuppressions.txt"
        "--xml-version=2"
        "--report-progress"
        ${CMAKE_SOURCE_DIR}
    )
endif()

# Run cppclean - pip install --upgrade cppclean
# Use `make cppclean` to run this and find unused #include's (among other things)
find_program(CPPCLEAN NAMES cppclean)
if (CPPCLEAN)
    add_custom_target(
        cppclean
        COMMAND ${CPPCLEAN}
        "-I${CLANG_INCLUDE_DIR}"
        "-I${LLVM_PATH}"
        "--exclude" "CMakeFiles"
        "--exclude" "include"
        "--exclude" "JSON"
        "--exclude" "XML"
        "--exclude" "Tools"
        "--exclude" "tests"
        "--quiet"
        ${CMAKE_SOURCE_DIR}
    )
endif()

# Run include-what-you-use
# Use `make iwyu` to run this
# Both include-what-you-use and iwyu_tool.py must be on your PATH
#
# This tool attempts to help us only include what we use. This can help prevent
# subtle bugs and unintended dependencies between compilation units.
# https://stackoverflow.com/a/31086619/551904
find_package(PythonInterp)
find_program(IWYU_TOOL_PATH NAMES iwyu_tool.py)
if (IWYU_TOOL_PATH AND PYTHONINTERP_FOUND)
	message(STATUS "Found include-what-you-use: ${IWYU_TOOL_PATH}")
	add_custom_target(
		iwyu
		COMMAND "${PYTHON_EXECUTABLE}"
		"${IWYU_TOOL_PATH}"
		"-p${CMAKE_BINARY_DIR}"
		"-j5"

		"--"
		"-Xiwyu"
		# Don't want generated comments getting truncated
		"--max_line_length=10000"

		COMMENT "Running include-what-you-use tool"
		VERBATIM
	)
else()
	# If you get this message, go install the tool: https://include-what-you-use.org
	message(WARNING "Could not find the program include-what-you-use.")
endif()

add_custom_command(TARGET Rex PRE_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${LLVM_PATH}/lib/clang/${CLANG_VER}/include ${CMAKE_SOURCE_DIR}/include)
add_custom_command(TARGET Rex POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/include $<TARGET_FILE_DIR:Rex>/include)
