#!/bin/bash

# If you want to run some cypher file directly on the database, use this.
# Also, if importing a small dataset, this can be faster than having to use the CSV method, since that requires
# stopping the database, wiping it, importing the CSV, restarting the database.

# If importing, may want to wipe the database first:
# docker exec db bash -c "cat <(echo 'MATCH (n) DETACH DELETE n;') | cypher-shell -u neo4j -p 123"

# $1 = script.cypher
docker cp $1 db:/script.cypher
docker exec db bash -c "cat /script.cypher | cypher-shell -u neo4j -p 123"
