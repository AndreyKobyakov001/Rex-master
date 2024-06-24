"""
This is a copy of the normal tests file except it uses the "trying returns" driver
"""
"""
This script uses the driver to run test cases, exercising the validator.
It is built around the format of `test_input.cpp`, where each test case mimics a ROS callback,
and the dataflow ends in a specially-named variable, mimicking a ROS publisher.

Test 4 fails, but test 5 passes (test 5 fails in main because of a flaw in the call stack stuff, but this implementation
doesn't yet use a call stack)
"""
import driver

def callbackSelector(session):
    # Populate the `test` dict to specify which tests to run

    # Format: {testID: [casesIDs]}
    tests = {} # run all tests
    # tests = {1:[], 3:[]} # run all cases under test1 and test3
    # tests = {2:[4,5], 3:[1]} # run cases 4, 5 under test2 and case 1 under test3

    testclauses = []
    for test, cases in tests.items():
        testclause = "cb.id contains \"test%s\"" % test # select the namespace
        if len(cases) != 0:
            # select only the specified tests under the namespace
            testclause += " and (" + " or ".join(["cb.id contains \"start%s\"" % case for case in cases]) + ")"
        else:
            testclause += " and cb.id contains \"start\"" # select all tests under the namespace
        testclauses.append("(" + testclause + ")")

    if len(tests) != 0:
        where = "where " + " or ".join(testclauses)
    else:
        # Run all tests under all namespaces
        where = "where cb.id contains \"test\" and cb.id contains \"start\""
    query = "match (cb:cFunction) " + where + " return cb.id as cb"

    return session.run(query)

# Select all dataflow paths starting with the "callbacks" and ending with the variable "end", which mimics
# a ROS publisher
def pathSelector(cb, session):
    return session.run("""
            match (:cFunction{id:$cbid})-[:contain]->(cbv:cVariable{isParam:"1"})
            match p=(cbv)-[:myVarWrite*]->(end:cVariable) where end.id contains "end"
            return p
            """, cbid=cb)

driver.run(callbackSelector, pathSelector)
