#!/bin/bash

docker stop $1
docker start -a $1
