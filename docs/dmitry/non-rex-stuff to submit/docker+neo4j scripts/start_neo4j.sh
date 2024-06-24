#!/bin/bash

# If you want to run multiple databases, with each database under a different container, then you need to use different folders
# under ~/neo4j/ --- i.e., ~/neo4j/data_db2, ~/neo4j/plugins_db2, ... and use them in the script below

# The memory constraint may need to be tweaked
docker run --memory="6g" --name=container --publish=7474:7474 --publish=7687:7687 --volume=$HOME/neo4j/data:/data --volume=$HOME/neo4j/logs:/logs --volume=$HOME/neo4j/plugins:/plugins --volume=$HOME/neo4j/conf:/conf --volume=$HOME/neo4j/import:/import neo4j:latest
