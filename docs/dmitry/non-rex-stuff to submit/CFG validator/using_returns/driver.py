"""
This is a copy of the normal driver except it uses the "trying returns" validator
"""
"""
This script selects the paths to test, runs the validator on each path, and prints out the results
"""

from neo4j import GraphDatabase
import validator
from timeit import default_timer as timer

uri = "bolt://localhost:7687"
username = "neo4j"
password = "123"

driver = GraphDatabase.driver(uri, auth=(username, password), max_connection_lifetime=-1)

# Prints a neo4j path in a human-readable way
def _printPath(path):
    first = True
    for r in path.relationships:
        if first:
            print(r.start_node['id'], end="")
            first = False
        print(" --" + r.type + "--> " + r.end_node['id'], end="")
    print('\n')

"""
Map a `(a)-[:varWrite*]->(b)` path to a
`(a)-[:edgeLink]->(:varWriteNode)-[:edgeLink]->()-...-[:edgeLink]->(b)` path

See the preprocess cypher script for more details
"""
def _mapPath(path, session):
    def nodeByID(ID):
        return "({id:\"" + ID + "\"})"

    startRel = path.relationships[0]
    pattern = nodeByID(startRel.start_node['id'])
    for varWrite in path.relationships:
        pattern += "-[:edgeLink]->(:varWriteNode)-[:edgeLink]->" + nodeByID(varWrite.end_node['id'])

    query = "match p=" + pattern + " return p"

    result = session.run(query)
    return result.single()['p']

"""
Originally, the state of Rex was such that there were barely any dataflows that originated with the callback
parameter of the ROS callbacks. i.e.,
void myCallback(const DataMsg& msg) {
    ... using msg ... // very few things were captured
}

However, once Rex's dataflow recognition was expanded, suddenly there were *too* many dataflows that were being
returned by queries like `(msg)-[:varWrite*]->()` -- the queries wouldn't terminate because they kept escaping
the bounds of `myCallback`'s transitive callees -- i.e., if `myCallback` only calls function `foo` and `foo` only
calls function `bar`, the paths returned from the query would also contain results from many *other* functions.
The `LocalPlannerModule::opt` variable is one particular bad example of this.

And queries like `(msg)-[:varWrite*]->(:rosPublisher)`, where an attempt was made to constrain the endpoint of
the returned paths, still didn't terminate, since realistically, in an arbitrary graph, all paths would still need
to be searched...

A more general solution for querying for dataflows was needed. The way this script achieves it is by the observation
that in Autonomoose, the query `(myCallback)-[:call*]->()` is fast and that the deepest call chain throughout the
entirety of autonomoose for *any* function, not just callbacks, is 10 long:

    ```
    match p=(:cFunction)-[:call*]->()
    with length(p) as l
    return l
    order by l desc
    limit 1
    ```

Therefore, if the scope of the `varWrite*` selection could be constrained to JUST the call tree of `myCallback`,
then the query should terminate, since there really aren't that many varWrites per call tree.

One attempt was to use Cypher projections to select only all nodes that fall under the call tree.
The problem with this, however, is that cypher projections support a limited set of
algorithms from Cypher's Graph Data Science library -- you can't run arbitrary cypher queries on the projected subgraph.
Alternatively, it might be possible to save the projected graph as its own database and then run queries on that,
but the Neo4j Community Edition does not support trivial swapping of databases (requires restarting neo4j and messing
with config variables).

Instead, the chosen solution is to, for every function under the callback's call tree, find all `varWrite` edges
inside those functions and mark them. Then, the varWrite* query can be restricted to use only the marked edges.
To mark the edges, for every varWrite edge, create an identical "myVarWrite" edge, and we can now run
    (msg)-[:myVarWrite*]->(:rosPublisher), and it terminates quickly!

    TODO: I didn't realize that the syntax for doing transitive closure on edges with a property set is possible.
    So can experiment with setting a property on the edges instead, i.e., r.restricted = 1, and then doing this query
    instead.
        (msg)-[:varWrite*{restricted:1}]->()
    Should see which one is faster to query, properties, or edges? Which one is faster to set up / tear down?

Note: If the method of restricting is changed from just following the call tree, will need to update
the method getCallChains() in the stack algorithm to prevent it from going out of bounds
"""
def _setConstraints(cb, session):
    # Using apoc.refactor.rename.type is for some reason giving threading errors in the neo4j console when the queries
    # start running. I think this has something to do with the way the refactorings are "committed" to the database
    # with respect to the python driver. For now, just making parallel edges is fine.
    # session.run("call apoc.refactor.rename.type('varWrite', 'myVarWrite') yield committedOperations return 1")

    # Add parallel edges within the bounds of the callback's transitive call* chain, so that
    # we can query for dataflows without exploding.
    session.run("""
    match (:cFunction{id:$cbid})-[:call*0..]->()-[:contain]->(v:varWriteNode), (a)-[:edgeLink]->(v)-[:edgeLink]->(b)
    merge (a)-[:myVarWrite]->(b)
    """, cbid=cb)
    # Do the same for "contain" edges so that when we query for an edge's list of locations, we stay within the bounds
    session.run("""
    match (:cFunction{id:$cbid})-[:call*0..]->(f)-[:contain]->(v:varWriteNode)
    merge (f)-[:restrictedContain]->(v)
    """, cbid=cb)

    # In this approach, CFG invoke and returns edges are being followed, so keep them within bounds by restricting
    # them
    session.run("""
    match (:cFunction{id:$cbid})-[:call*0..]->()-[:contain]->(callcfg:cCFGBlock)-[:invoke]->(calleecfg)
    match (callee:cFunction)-[:contain]->(retfrom:cCFGBlock)-[:returns]->(retto)<-[contain]-(callcfg)

    merge (callcfg)-[:restrictedInvoke]->(calleecfg)
    merge (retfrom)-[:restrictedReturns]->(retto)
    """, cbid=cb)
def _removeConstraints(session):
    session.run("match ()-[r:myVarWrite]->() delete r")
    session.run("match ()-[r:restrictedContain]->() delete r")
    session.run("match ()-[r:restrictedInvoke]->() delete r")
    session.run("match ()-[r:restrictedReturns]->() delete r")

"""
The main method.

callbackSelector(session) = function which returns a neo4j result where each row contains an entry 'cb' containing the ID
                           of the callback being analyzed. This callback is passed to pathSelector
pathSelector(cb, session) = function which returns a neo4j result where each row contains an entry 'p' which is a dataflow
                           path that can be passed to _mapPath. These are the paths that get validated.
                           @param cb = the callback function which starts off the paths

    Note: The path selector should use the constraining structures generated by the "_setConstraints" method
"""
def run(callbackSelector, pathSelector):

    with driver.session() as session:
        validator.setSession(session)

        # Run at start just in case previous runs were aborted
        _removeConstraints(session)

        # Select callbacks. This is just so we can iterate over them and print out the progress of the
        # dataflow selection + validation for each one.
        result = callbackSelector(session)

        for row in result:
            cb = row['cb']

            print("\n======")
            print("Running on Callback " + cb)
            print("======")

            _setConstraints(cb, session)

            path_results = pathSelector(cb, session)

            invalid = []
            valid = []

            for row in path_results:
                originalPath = row['p']

                # Map the returned [:varWrite*] path onto the parallel database structure
                path = _mapPath(originalPath, session)

                # Build up the input to the path validator
                varWriteLocations = []
                for n in path.nodes:
                    if "varWriteNode" in n.labels:
                        loc = validator.getLocationInfo(n['id'])
                        varWriteLocations.append(loc)


                # Run the validator
                if not validator.validatePath(varWriteLocations):
                    invalid.append(originalPath)
                else:
                    valid.append(originalPath)


            _removeConstraints(session)

            print("VALID:")

            for path in valid:
                _printPath(path)

            print("~~~")
            print("INVALID:")

            for path in invalid:
                _printPath(path)

        session.close()

    driver.close()
