#!/bin/bash

docker stop db

# wipe the database files -- `neo4j-admin import` can only work on clean databases
# - couldn't find a way to wipe only the data without the user account settings tho... this ends up resetting
#   the password, so I modified the configuration to disable authentication
# - couldn't get `~` or $HOME to work, had to hard code home directory may have been some
#   `sudo` issues... might not be necessary
sudo rm -rf /home/dkobets/neo4j/db/*

docker start -a importcsv
docker start -a db
