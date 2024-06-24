#!/usr/bin/env bash

set -e

echo "Analysing code..."
make cppcheck 2> result.xml

echo "Outputting report..."
cppcheck-htmlreport --title="Rex" --source-dir=$(readlink -f .) \
  --report-dir=cppcheck-report --file=result.xml
rm result.xml
