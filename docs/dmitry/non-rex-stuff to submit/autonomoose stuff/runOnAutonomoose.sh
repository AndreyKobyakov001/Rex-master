#!/bin/bash

# This script is a modification of the existing `runOnAutonomoose.sh` script under Rex. It adds CFG support,
# allows more fine-grained control over which autonomoose modules to run on, and also prevents processing
# of all `test` files in autonomoose (the previous script caught most, but not all), as well the `example_project`

# Usage:
# ./runOnAutonomoose [-c] output_dir [autonomoose_target]
#
# All output goes under `output_dir`
#
# - autonomoose_target: if empty, runs on the entirety of autonomoose. Otherwise, runs on only this subfolder (i.e., `ref_ekf`)
# - option: -c: passes runs Rex with CFG-mode ON

while getopts "c" option
do
    case "${option}"
        in
        c) CFG='--cfg=true';;
    esac
done

shift $((OPTIND - 1))

OUTPUT_LOC=`readlink -f $1`

if [ ! -e $OUTPUT_LOC ]
then
	mkdir -p $OUTPUT_LOC
fi

CORPUS_SRC_DIR=/mnt/c/Users/dkobets/Documents/catkin_ws/src/autonomoose/rospackages/autonomoose_core

# Location of Rex executable
# Must not end in /Rex
REX_BUILD_LOC=/mnt/c/Users/dkobets/Documents/ura2/Rex-Build


OUTPUT_LOC=`readlink -f $OUTPUT_LOC`
LOG_LOC=$OUTPUT_LOC

# Where final ta files are stored (along with small grok script)
# Must be a directory
OUTPUT_LOC=`readlink -f $OUTPUT_LOC`

modulesToMatch=${2:-ALL}

> $LOG_LOC/anmRunLog.txt
> $OUTPUT_LOC/readTAs.ql
file_n=0
total_files=`ls -d $CORPUS_SRC_DIR/* | wc -l`
for file in `ls -d $CORPUS_SRC_DIR/*`
do
    file_n=$((file_n+1))
	srcDir=`find $file -type d -name src -prune | grep -v /test/`
	includeDir=`find $file -type d -name include -prune`
	module=`basename "$file"`

    if [ $modulesToMatch = "ALL" ] || [ $modulesToMatch = $module ]; then
        if [ "$module" = "example_package" ]; then # skip
            continue
        elif [ "$module" = "anm_route_network" ]; then # filter out hidden tests
            srcFolder=$srcDir
            srcDir="$srcFolder/RouteGraph $srcFolder/RouteNetwork $srcFolder/*.cpp"
        elif [ "$module" = "route_publisher" ]; then # filter out hidden tests
            srcFolder=$srcDir
            srcDir="$srcFolder/AnmNavigator $srcFolder/tinyspline $srcFolder/*.cpp"
        fi

        if [ -n "$srcDir" ] && [ -n "$includeDir" ]
        then
            echo "Running Rex on $module!: $CFG $includeDir $srcDir [$file_n/$total_files]" # the file counting is messed up
            $REX_BUILD_LOC/Rex $CFG $includeDir $srcDir -j -o $OUTPUT_LOC/$module.ta &>> $LOG_LOC/anmRunLog.txt
            status=$?
            echo "Rex exited with status $status on $module" >> $LOG_LOC/anmRunLog.txt
            if [ $status -eq 0 ]
            then
                :
                # echo "addta $module.ta" >> $OUTPUT_LOC/readTAs.ql
            else
                echo "$module is being ignored in analysis. Extraction did not complete successfully"
                # gdb --args $REX_BUILD_LOC/Rex $includeDir $srcDir -j -o $OUTPUT_LOC/$module.ta
                exit $status
            fi
        fi
    fi
done
