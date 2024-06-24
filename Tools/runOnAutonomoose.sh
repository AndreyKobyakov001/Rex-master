#!/bin/bash

# To pass the `--cfg=true` option to Rex, use the `-c` flag

while getopts "c" option
do
    case "${option}"
        in
        c) CFG='--cfg=true';;
    esac
done

shift $((OPTIND - 1))


# Basic struicture: /..../autonomoose_core
# E.g ~/catkin_ws/src/autonomoose/rospackages/autonomoose_core
# CORPUS_SRC_DIR=$1
CORPUS_SRC_DIR=/mnt/c/Users/dimit/Documents/ura2/catkin_ws/src/autonomoose/rospackages/autonomoose_core

# Location of Rex executable
# Must not end in /Rex
# REX_BUILD_LOC=`readlink -f $3`
REX_BUILD_LOC=/mnt/c/Users/dimit/Documents/ura2/Rex-Build

# OUTPUT_LOC=$2
OUTPUT_LOC=`readlink -f $1`

if [ ! -e $OUTPUT_LOC ]
then
	mkdir -p $OUTPUT_LOC
fi

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
        if [ "$module" = "anm_route_network" ]; then
            srcFolder=$srcDir
            srcDir="$srcFolder/RouteGraph $srcFolder/RouteNetwork $srcFolder/*.cpp"
        elif [ "$module" = "route_publisher" ]; then
            srcFolder=$srcDir
            srcDir="$srcFolder/AnmNavigator $srcFolder/tinyspline $srcFolder/*.cpp"
        fi

        if [ -n "$srcDir" ] && [ -n "$includeDir" ]
        then
            echo "Running Rex on $module!: $CFG $includeDir $srcDir [$file_n/$total_files]"
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


