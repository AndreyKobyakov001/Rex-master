
# Overview

This project runs CFG validation on dataflow paths. It provides a driver which:
1. selects starting functions
2. for each starting function, select dataflow paths
3. for each dataflow path, check to see if it is feasible via the CFG
4. print the results

The selectors for steps 1 and 2 are provided to the driver externally. An example of how this is done can be seen
in `autonomoose.py` or `tests.py`

## Files

`validator.py` checks whether a given path is valid with respect to the CFG

`driver.py` runs the validator on provided dataflow paths, and outputs the results.

`autonomoose.sh` runs the driver on dataflow paths starting at subscribers and ending with ROS publishers.
Currently, all dataflows returned are correct with respect to the CFG (no false positives are flagged).

`tests.sh` use the driver to exercise the validator on various control flows. It requires the database to be built from `test_input.cpp`
The test output must be manually inspected.

`preprocess.cypher` builds up various necessary structures within the graph
`preprocess.sh` runs `preprocess.cypher` on the graph. Neo4j is expected to be loaded within a docker container.


# Running and Setup

Before running, set the neo4j username and password in the driver

## Rex version

Should be running a version of rex which produces:
- for every `[r:varWrite]`:
    - `r.containingFuncs` is a list of function IDs which contain this relation
    - `r.cfgBlocks` is a list of CFG IDs which contain this relation
- for every `(:cCFGBlock)`, there is a relationship `(f:cFunction)-[:contain]->(:cCFGBlock)` where CFG part of function `f`'s CFG

## Preprocessing

The first time the database is loaded, run `preprocess.sh` BEFORE running the validators. This should only be done **once** per
database.

## Running on autonomoose:
`python3 autonomoose.sh`

## Running the tests:

1. Use Rex to generate the test database from `test_input.cpp`.
2. Run the preprocessing stuff
3. `python3 tests.py` to run the validator's test cases

# Remaining Work

3 TODOs in `validator.py`, two of which improve the accuracy of the CFG check to eliminate missed false positives,
and one which might improve performance
    - these missing false positives result in test5 to fail. All other tests pass.

1 TODO in `preprocess.cypher`

2 TODO in the `driver.py`
