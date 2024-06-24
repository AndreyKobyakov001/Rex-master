#!/bin/bash

echo "Creating temp files..."

INTERACTIVE_INPUT=./RexInput.txt
TEMP_STDOUT=$(mktemp /tmp/RexTestStdout.XXXXXX)
TEMP_STDERR=$(mktemp /tmp/RexTestStderr.XXXXXX)

if [ -z "$REX" ]
then
    echo "REX environment variable is undefined. Assuming Rex is found in PATH."
    REX_EXECUTABLE=Rex
else
    echo "REX environment variable found."
    REX_EXECUTABLE="$REX"
fi

# Clean up previous output (if any)
rm -f output.ta

command="$REX_EXECUTABLE < $INTERACTIVE_INPUT > $TEMP_STDOUT 2> $TEMP_STDERR"

# Running test...
echo $command
eval $command

exit_code=$?
echo "exit_code=$exit_code"

if [ $exit_code -eq 0 ]
then
    echo "PASSED"
else
    echo "FAILED!!!"
fi

# Backing-up test result...
TESTS_RESULT=tests_result
mkdir -p $TESTS_RESULT

timestamp=$(date +%s-%N) # Get timestamp in nanoseconds
RESULT=$TESTS_RESULT/$timestamp
mkdir $RESULT

cp $TEMP_STDOUT $RESULT/stdout
cp $TEMP_STDERR $RESULT/stderr
# This might not be needed but the interactive commands could be changed overtime
cp $INTERACTIVE_INPUT $RESULT/stdin

echo "Result backed-up to $RESULT"

# Cleaning up temp files...
rm -f $TEMP_STDERR
rm -f $TEMP_STDOUT
