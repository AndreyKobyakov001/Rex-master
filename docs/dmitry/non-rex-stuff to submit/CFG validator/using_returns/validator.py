"""
This is an experimental version of the validator that tried to use custom-built "return" edges in
the CFG to approach the CFG validation from a different angle.

The main version of the validator uses stacks to join together separate CFG patterns, with each pattern restricted
to the innards of a single function. See `validator.py` for more details.

This version of the validator instead tries to avoid simulating a callstack in the same manner as the old validator.
Instead, it relies on the following edges in the CFG graph:
    contain = link two CFG nodes within a single function
        - nothing new here, same as in the stack-based validator
    invoke = link a call-site CFG node to the entry node of the callee's CFG
        - the driver adds restrictions on invoke edges, so as to not escape the call tree
    returns = link a CFG's exit node to the **direct successor(s)** of all its call sites
        void foo() {
           bar();       // bar's exit node linked to the `int x = 3` node via a `returns` edge
           int x = 3;
        }

        These edges are added as part of a pre-processing step.
        Also, they require that exit nodes are kept around in the CFG graph, so the Rex CFG builder needs to be updated
        to keep exit nodes. This is done in the same place and in the exact same way as where entry nodes are marked to be kept

        Also, the driver adds restrictions on return edges so as to note escape the call tree

With this in place, if varWrite A ever flows in any way (directly, or through function calls, or through returns, ...)
into varWrite B, there should exist a path between their corresponding CFG nodes.
It's not enough, however, for an input path of varWriteNode nodes A,B,C,...,Z, to simply check for the existence of
this pattern in cypher:

(A)
-[:cfgLink]->()-[:contain|invoke|returns*]->()-[:cfgLink]->(B)
-[:cfgLink]->()-[:contain|invoke|returns*]->()-[:cfgLink]->(C)
...
-[:cfgLink]->()-[:contain|invoke|returns*]->()-[:cfgLink]->(Z)

because some dataflows require cycles in the control flow, and cypher patterns greatly restrict cycles -- the above
pattern simply wouldn't return a result.
APOC path expansion may *potentially* be used to get around this problem, but that wasn't attempted yet

Instead, CFG path-checking is attempted piece by piece between pairs of varWriteNode nodes in the path being validated:
(A)-[:cfgLink]->()-[:contain|invoke|returns*]->()-[:cfgLink]->(B)

However, a problem with this approach is that if a function is called in two places, the return edge that's taken
might go into the wrong function. Example:

    void baz() {
        ... nothing special ...
    }
    void bar() {
        baz();
        c = b;
    }
    void foo() {
        bar(); // if this isn't here, then the return from baz into bar won't be taken since returns are restricted to the call tree
        b = a;
        baz();
    }

    False positive [a -> b -> c]:
        foo() -...-> {b = a} -> {baz()} -return into bar()-> {c = b}

The solution would therefore have to not just check for the **existence** of a path between the nodes, but find a path that's
valid with respect to a call stack... which requires enumerating all paths while keeping a call stack...
so basically it will end up looking very similar to the existing solution...

Nevertheless, here is a prototype version which just checks for the existence of paths. It is quite fast.
"""

from neo4j import GraphDatabase
import sys
import random
import csv
import itertools

uri = "bolt://localhost:7687"
username = "neo4j"
password = 123

global _session
def setSession(session):
    global _session
    _session = session

# Returns the locations of function calls along all path of function calls between src and dst
#
# bar() {
#     baz();
# }
# foo() {
#     bar();
#     baz();
#     bar();
# }
#
# If we query for all paths from `foo` to `baz`, then there are 2 distinct paths:
# (1) `foo -> baz` and
#     returns 1 row: g=baz, cfgs=<location of baz() call in side foo()
# (2) `foo -> bar -> baz`
#     returns 2 rows:
#         1. g=bar, cfgs=<both locations of bar() call inside foo()
#         2. g=baz, cfrs=<location of baz() call inside bar()
#
# So this function will return two paths, ordered in increasing length
# [
#     [("baz", [cfgs...])],                    # corresponds to result (1)
#     [("bar", [cfgs...]), ("baz", [cfgs...])] # corresponds to result (2)
# ]
#
# Note: don't need to use a "restricted" version of the callpairs relationship here because by definition,
# we are restricting based on transitive calls (If this changes in the future, will need to update)

# return a dict of
# {
#    <function which contains varWriteNode>:
#       [<cfg corresponding to varWriteNode that is also in the function>],
#   ...
# }
# Note: all location edges use the restricted versions, so as to not return results that are within the bounds
# of the paths being analyzed
def getLocationInfo(varWriteNodeID):
    global _session
    result = _session.run("""
    match (v:varWriteNode{id:$id})<-[:restrictedContain]-(f:cFunction)
    with *, [(v)-[:cfgLink]->(cfg)<-[:contain]-(f) | cfg.id] as cfgs
    return f.id as f, cfgs
    """, id=varWriteNodeID)
    ret = {}
    for row in result:
        ret[row['f']] = row['cfgs']
    return ret

# path is a list of consecutive `getLocationInfo` results, with each result being generated from a node
# along that path
def validatePath(path):
    global _session
    prevcfgs = list(itertools.chain.from_iterable(path[0].values()))

    iterpath = iter(path)
    next(iterpath)
    for locs in iterpath:
        cfgs = list(itertools.chain.from_iterable(locs.values()))
        query = ""
        i = 1

        matches = []
        for cfg in cfgs:
            match = "optional match (a" + str(i) + "{id:\"" + cfg + "\"}) where "
            bits = []
            for prevcfg in prevcfgs:
                bits.append("({id:\"" + prevcfg + "\"})-[:contain|restrictedReturns|restrictedInvoke*0..]->(a" + str(i) + ")")
            match += " or ".join(bits)
            match += " return a" + str(i) + ".id as ret"

            matches.append(match)
            i += 1

        query = "\nunion\n".join(matches)

        prevcfgs = []
        result = _session.run(query)
        for r in result:
            cfg = r['ret']
            if not cfg is None:
                prevcfgs.append(cfg)

        if len(prevcfgs) == 0:
            return False

    return True


