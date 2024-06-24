#!/bin/bash

# Basic structure: .../..../build
# E.g ~/catkin_ws/build/
CORPUS_BUILD_DIR=`readlink -f $1`
# Basic struicture: /..../autonomoose_core
# E.g ~/catkin_ws/src/autonomoose/rospackages/autonomoose_core/
CORPUS_SRC_DIR=$2
for file in `find $CORPUS_BUILD_DIR -name "compile_commands.json"`
do
	fullDir=`dirname "$file"`
	compDB=`basename "$file"`
	parentDir=`basename "$fullDir"`
	srcDir=`find $CORPUS_SRC_DIR -type d -name $parentDir -prune`
	if [ -d "$srcDir" ]
	then
		echo "copying $file to $srcDir/$compDB"
		cp $file $srcDir/$compDB
	else
		echo "$CORPUS_SRC_DIR/$parentDir not found!"
	fi	
done
