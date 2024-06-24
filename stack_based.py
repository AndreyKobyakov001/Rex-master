from neo4j import GraphDatabase
import sys
import random
import csv

if len(sys.argv) < 4:
    print("Usage: " + sys.argv[0] + " your_neo4j_username your_neo4j_password resultCSV")
    # Temporarily disabled for testing without Neo4j.
    exit(1)

uri = "bolt://localhost:7687"
username = sys.argv[1]
password = sys.argv[2]
resultCSV = open(sys.argv[3], 'w')

driver = GraphDatabase.driver(uri, auth=(username, password), max_connection_lifetime=-1)

def printPath(path):
    first = True
    for r in path.relationships:
        if first:
            print(r.start_node['id'], end="")
            first = False
        print(" --" + r.type + "--> " + r.end_node['id'], end="")
    print('\n')


class StackFrame:

    def __init__(self, fnID, startCFGIDs):
        self.fnID = fnID
        self.stepCount = 0
        self.match = ""
        self.pattern = ""
        self.whereID = ""
        self.addStep(startCFGIDs)

    def addStep(self, CFGIDs):
        self.stepCount += 1

        if self.stepCount != 1:
            self.match += ", "
            self.pattern += "-[:contain*0..]->"
            self.whereID += " and "

        nodeName = "n" + str(self.stepCount)
        self.match += "(" + nodeName + ")"
        self.pattern += "(" + nodeName + ")"
        self.whereID += nodeName + ".id in " + str(CFGIDs)

    def empty(self):
        return self.stepCount == 1

    def getQuery(self):
        return "match " + self.match + " where " + self.whereID + " and " + self.pattern + " return 1"

def getEnclosingFunction(cfgID):
    result = session.run('''match (f)-[:functionCFGLink]->()-[:contain*]->(n {id: $cfgID})
                            return f.id as fID''', cfgID=cfgID)
    return result.peek()['fID']

def callsTransitively(f1, f2):
    result = session.run('''match (f1 {id:$f1ID})-[:call*]->(f2 {id:$f2ID})
                            return 1''', f1ID=f1, f2ID=f2)
    return result.peek() != None

def getTransitiveCallSites(srcFn, dstFn):
    result = session.run('''
    match p=(f {id: $srcFn})-[:call*]->(g {id: $dstFn})
    match (fB {id: $srcFn+':CFG:ENTRY'})-[:contain*]->(callsite) where callsite.id in relationships(p)[0].cfgBlocks
    return collect(callsite.id) as callsiteCFGs
    ''', srcFn=srcFn, dstFn=dstFn)
    if result.peek() is None:
        return []
    return result.peek()['callsiteCFGs']


totalQueriesRun = 0

def visitPath(path):
    global totalQueriesRun

    # These two variables eliminate paths with useless call* prefixes:
    # We are interested only in paths representing a unique data flow. A data flow is characterized
    # by a sequence of `write, varWrite, varInfFunc`. Any call* prefixes to the behavior alteration path are therefore not
    # *part* of the data flow, but may be necessary for it to exist.
    # Example:
    # `a -call-> b -write-> x -....-> z -varInfFunc-> g`
    # The data flow starts at `b` and ends at `g`. If the control flow never returns to `a` at any point in the path,
    # then then removing `a` will leave the data-flow portion of the path unchanged.
    # On the other hand, if say the final `varInfFunc` resides in `a`, then `a` is essential to this particular
    # data flow path.
    #
    # Therefore, if the first relationship a call, check that at some point along the path, we return to that first
    # function.
    firstRelationshipIsCall = path.relationships[0].type == "call"
    returnedToFirstFunc = False

    def workOnNextItem(stack, queries, remainingRels):
        nextRel = remainingRels[0]
        nextCFGs = nextRel['cfgBlocks']

        groupByFunc = {}
        for cfg in nextCFGs:
            ef = getEnclosingFunction(cfg)
            if ef not in groupByFunc:
                groupByFunc[ef] = []
            groupByFunc[ef].append(cfg)

        for f in groupByFunc:
            if doWork(stack.copy(), queries.copy(), f, groupByFunc[f], nextRel, remainingRels[1:]):
                return True

        return False

    def doWork(stack, queries, fnID, CFGs, curRel, remainingRels):
        nonlocal returnedToFirstFunc
        global totalQueriesRun
        if len(stack) != 0:
            # find closest common ancestor
            while len(stack) != 0:
                if (stack[-1].fnID == fnID or callsTransitively(stack[-1].fnID, fnID)):
                    break
                if not stack[-1].empty():
                    queries.append(stack[-1].getQuery())
                stack.pop()


            if len(stack) == 0:
                # no ancestor, path is disjoint
                return False

            if (stack[-1].fnID == fnID):
                stack[-1].addStep(CFGs)
                if len(stack) == 1:
                    # The stack has been popped all the way to the first function
                    returnedToFirstFunc = True
            else: # called transitively, find starting call
                callsites = getTransitiveCallSites(stack[-1].fnID, fnID)
                stack[-1].addStep(callsites)

        stack.append(StackFrame(fnID, CFGs))

        if len(remainingRels) == 0:
            if firstRelationshipIsCall and not returnedToFirstFunc:
                # This path has a call* prefix not essential for the behavior alteration path
                return False

            while len(stack) != 0:
                if not stack[-1].empty(): # should ONLY be true at most once -- how to code this
                    queries.append(stack[-1].getQuery())
                stack.pop()

            if len(queries) == 0:
                raise "SOMETHING IS SUPER WRONG"

            for q in queries:
                totalQueriesRun += 1
                result = session.run(q)
                if result.peek() is None:
                    return False
            return True
        else:
            return workOnNextItem(stack, queries, remainingRels)

    return workOnNextItem([], [], path.relationships)


with driver.session() as session:
    resultsWriter = csv.writer(resultCSV, delimiter=',')

    # # working
    result = session.run('''
    match p=(srcFunc:cFunction)-[:call|write|varWrite*]->()-[:varInfFunc]->(dstFunc)
    with *, substring(split(srcFunc.filename,'/')[-1], 0, 3) as srcModule, substring(split(dstFunc.filename,'/')[-1], 0, 3) as dstModule
    // where srcFunc.id <> dstFunc.id and toLower(srcModule) <> toLower(dstModule)
    return p;''')


    # result = session.run('''
    #     match ()-[r:subscribe]->()-[:call]->(cbF)
    #     with distinct cbF
    #
    #     match p=(cbF)-[:call|write|varWrite*]->()-[:varInfFunc]->(f)
    #     where not (f.id =~ ".*;ros::.*") and not(f.id =~ ".*;boost::.*")
    #
    #     return p
    #      ''')

    # Without calls
    # result = session.run('''
    # match p=(srcFunc:cFunction)-[:write]->()-[:varWrite*0..]->()-[:varInfFunc]->(dstFunc)
    # with *, substring(split(srcFunc.filename,'/')[-1], 0, 3) as srcModule, substring(split(dstFunc.filename,'/')[-1], 0, 3) as dstModule
    # where srcFunc.id <> dstFunc.id and toLower(srcModule) <> toLower(dstModule)
    # return p;''')


    valid_paths = []
    invalid_paths = []
    numRow = 0
    for row in result:
        numRow += 1
        path = row['p']

        if visitPath(path):
            valid_paths.append(path)
        else:
            invalid_paths.append(path)

    print("TOTAL QUERIES RUN:", totalQueriesRun)
    print("NUM VALID:", len(valid_paths))
    for path in valid_paths:
        printPath(path)

    # print("INVALID PATHS:")
    # for path in invalid_paths:
    #     printPath(path)

    session.close()

driver.close()
