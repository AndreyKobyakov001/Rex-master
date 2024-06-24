#!/bin/bash

# Runs preprocess.cypher on the graph
# The graph is expected to be running in a docker container.

if [ $# -ne 3 ]; then
    echo "Usage: $0 <docker container name> <neo4 username> <neo4 password>"
    exit 1
fi

dbname=$1
neo4jname=$2
neo4jpassword=$3

docker cp preprocess.cypher $dbname:/preprocess.cypher
docker exec $dbname bash -c "cat /preprocess.cypher | cypher-shell -u $neo4jname -p $neo4jpassword"
