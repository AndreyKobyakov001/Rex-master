#!/bin/bash

# Relies on the edges.csv and nodes.csv files to be located under ~/neo4j/import/

docker run --name=importcsv --publish=7474:7474 --publish=7687:7687 --volume=$HOME/neo4j/data:/data --volume=$HOME/neo4j/logs:/logs --volume=$HOME/neo4j/plugins:/plugins --volume=$HOME/neo4j/conf:/conf --volume=$HOME/neo4j/import:/import neo4j:latest /bin/bash -c "neo4j-admin import --relationships=/import/edges.csv --nodes=/import/nodes.csv --delimiter='\t' --array-delimiter='-'"
# array-delimiter is used to separate items in list-like attributes (i.e., the "containingFunc" attribute for varWrite edges")
