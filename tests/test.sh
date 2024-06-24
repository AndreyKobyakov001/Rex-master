#!/bin/bash

while getopts "h" arg; do
    case $arg in
        h)
            echo "Requirement:"
            echo "Even though this depends on each individual project test script, we expect"
            echo "Rex executable to be available in PATH environment variable, or else the"
            echo "test will look in the REX environment variable."
            echo ""
            echo "Usage:"
            echo "./test.sh [Options...] [Test cases...]"
            echo "If no specific tests are specified then all tests will be run by default."
            echo ""
            echo "Examples:"
            echo "./test.sh # Test everything"
            echo "./test.sh test1 test2 # Test only test1 test2"
            echo "REX=/path/to/custom/Rex ./test.sh # Test everything with custom path to Rex"
            echo ""
            echo "Options:"
            echo "-h: Print this manual"
            echo ""
            echo "How to add more test?"
            echo "- Each test case is a project of its own, and each has its own customized"
            echo "test script, specified in test.sh within the directory."
            echo "- So to add a new project we simply create a new test project and create"
            echo "a new (or copy the existing) test script."
            echo "- Or to add new tests to existing project, we modify the test.sh in the"
            echo "project."

            exit
            ;;
    esac
done

args=("$@")

if [ ${#args[@]} -eq 0 ]
then
    # NOTE: this won't work if tests' names contain space
    tests=($(ls -d */ | cut -f1 -d'/'))
    echo "Running all ${#tests[@]} tests..."
else
    tests=("${args[@]}")
    echo "Running ${#tests[@]} individual tests..."
fi

SCRIPT=test.sh

counter=1
for t in "${tests[@]}"
do
    echo ""
    echo -n "#$counter: "
    counter=$[$counter + 1]

    # Still need to check since the user specified tests might not exist
    if [ -d "$t" ]; then
        cd $t

        if [ ! -f $SCRIPT ]; then
            echo "$SCRIPT in $t not found!"
        else
            echo "Running test $t..."

            ./$SCRIPT
        fi

        cd ..
    else
        echo "Test $t does not exist!"
    fi
done

