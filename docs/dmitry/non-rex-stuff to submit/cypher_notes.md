
# Contents

- [Labels](#labels)
 - [Cypher queries organize their matches in alphabetical order!!!](#cypher-queries-organize-their-matches-in-alphabetical-order)
 - [Weird Behavior](#weird-behavior)
   - [Substring Inequality](#substring-inequality)
   - [`with` Clause](#with-clause)
   - [[varWrite*2] vs [varWrite]->()-[varWrite]](#varwrite2-vs-varwrite--varwrite)
   - [Matching on a Subgraph](#matching-on-a-subgraph)
   - [Capturing a path changes the query plan used to match the path, making it take longer](#capturing-a-path-changes-the-query-plan-used-to-match-the-path-making-it-take-longer)

## Labels

Labels are *not* always recommended. I found this increased the query times in some cases, and here is an explanation on stackoverflow:
https://stackoverflow.com/questions/55464897/did-i-just-increase-the-db-hit-count-x2-by-adding-a-node-label-into-my-cypher


## Cypher queries organize their matches in alphabetical order!!!

```
match p=(b {id:"decl;local_planner;local_planner::LocalPlannerNodelet::obstacleListCb(const anm_msgs::DynamicObstacleListConstPtr &,)::idx;;"})<-[:varWrite*]-()
return p // returns 0 -- there are no incoming edges

match p=(a {id:"decl;local_planner;local_planner::LocalPlannerNodelet::obstacleListCb(const anm_msgs::DynamicObstacleListConstPtr &,)::itr;;"})-[:varWrite*]->()
return p // takes forever -- a LOT of paths

// combining the two, you would expect it to short-circuit nicely...
match p=(b {id:"decl;local_planner;local_planner::LocalPlannerNodelet::obstacleListCb(const anm_msgs::DynamicObstacleListConstPtr &,)::idx;;"})<-[:varWrite*]-(a {id:"decl;local_planner;local_planner::LocalPlannerNodelet::obstacleListCb(const anm_msgs::DynamicObstacleListConstPtr &,)::itr;;"})
return p // hangs!

// It's clear from running the query planner (prepend `explain` to the above query and run it in the browser), that the `b` match is being applied *after*
// the query selects all paths from `a` -- but this part doesn't terminate!

// Trying to mix up the query doesn't help!
match (b {id:"decl;local_planner;local_planner::LocalPlannerNodelet::obstacleListCb(const anm_msgs::DynamicObstacleListConstPtr &,)::idx;;"})
match (a {id:"decl;local_planner;local_planner::LocalPlannerNodelet::obstacleListCb(const anm_msgs::DynamicObstacleListConstPtr &,)::itr;;"})
match p=(b)<-[:varWrite*]-(a)
return p // hangs

match (b {id:"decl;local_planner;local_planner::LocalPlannerNodelet::obstacleListCb(const anm_msgs::DynamicObstacleListConstPtr &,)::idx;;"})
match (a {id:"decl;local_planner;local_planner::LocalPlannerNodelet::obstacleListCb(const anm_msgs::DynamicObstacleListConstPtr &,)::itr;;"})
match p=(a)-[:varWrite*]->(b)
return p // hangs


HOWEVER, swapping the names of the matched nodes instantly resolves the problem!

match (a {id:"decl;local_planner;local_planner::LocalPlannerNodelet::obstacleListCb(const anm_msgs::DynamicObstacleListConstPtr &,)::idx;;"})
match (b {id:"decl;local_planner;local_planner::LocalPlannerNodelet::obstacleListCb(const anm_msgs::DynamicObstacleListConstPtr &,)::itr;;"})
match p=(b)-[:varWrite*]->(a)
return p // returns 0 instantly
```

## Weird Behavior

### Substring Inequality

`substring` bug... not sure why the equality doesn't work. I've messed around and also got it failing for longer
cases (i.e., substring more than 1 length long), but this is the simplest case which would presumably reveal the problem.
```
match (a)
// return substring("ae", 1)="x"  // (1) True
// return "e"="x"   // (2) False
// return substring("ae", 1)  // (3) e
limit 1
```
A way around this is to use the `=~` regex operator, but I can't find anywhere in the cypher docs where they claim
this is the *correct* way to do string comparisons...


### `with` Clause

Adding `parm` in the `with` clause removes all results. Is this intended?
- read up on the `with` clause

```
match (cb{isCallbackFunc:"1"})-[:contain]->(parm{isParam:"1"})
with collect(parm.id) as parms, cb
    where size(parms) <> 1
    return cb.id, parms // returns 3 results


match (cb{isCallbackFunc:"1"})-[:contain]->(parm{isParam:"1"})
with collect(parm.id) as parms, cb, parm
    where size(parms) <> 1
    return cb.id, parms // returns 0
```



### [varWrite*2] vs [varWrite]->()-[varWrite]

The official [neo4j documentation](https://neo4j.com/docs/cypher-manual/current/syntax/patterns/) asserts that a numeric star syntax
is **equivalent to** the expanded form.

However, at some point I had noticed discrepancies between the versions. These queries don't replicate the problem on the current state
of Rex's database output (I think they were being run back when class variables were being included in writes),
but at one point they did, so might be useful to keep this problem in mind...

Note: to get these timings, I restarted the database between runs, so as to not rely on the cache

```
// ~ 6 seconds
// match p=(a {id:"decl;local_planner;local_planner::LocalPlannerNodelet::opt;;"})-[:varWrite]->()
// -[:varWrite]->()
// -[:varWrite]->()
// -[:varWrite]->()
// -[:varWrite]->()
// -[:varWrite]->()
// -[:varWrite]->()
// -[:varWrite]->()
// -[:varWrite]->()
// -[:varWrite]->()
// -[:varWrite]->()
// -[:varWrite]->(a)
// ~ 18 seconds
// match p=(a {id:"decl;local_planner;local_planner::LocalPlannerNodelet::opt;;"})-[:varWrite*11]->()-[:varWrite]->(a)
// ~ Takes too long
// OutOfMemoryException
// match p=(a {id:"decl;local_planner;local_planner::LocalPlannerNodelet::opt;;"})-[:varWrite*12]->(a)
// return p
```


### Matching on a Subgraph

A problem that I was facing with verifying dataflows with the CFG was actually selecting the dataflow chains to begin with.
Doing generic `()-[:pubVar]->()-[:pubTarget]->()-[:contain]->({isParam:"1"})-[:varWrite*]->(:rosPublisher)` doesn't terminate.

I was hoping to instead be able to restrict the graph to only the transitive calls of the function. However, I couldn't
find a way to actually select a subgraph from neo4j, and then running generic queries against that subgraph.

Here are some things I tried:

https://community.neo4j.com/t/how-to-match-a-subset-of-nodes-then-match-a-pattern-within-the-subset/6576/2
- Doesn't work because the `all(r in relationships(p) ... )` performs the filtering *after* a path has been selected -- it doesn't
actually use the filtering to select the paths

https://community.neo4j.com/t/how-to-create-a-subgraph-and-run-graph-algorithms-only-on-that/12224
- points to this page: https://neo4j.com/docs/graph-data-science/current/management-ops/graph-catalog-ops/
- but both the old and the new version of cypher projections limit the algorithms that can be run to those from the "Graph Data Science"
library -- generic queries are not available
- I also tried exporting the projected graph into its own database and then swap to it to run the queries, but got errors.
https://stackoverflow.com/questions/60429947/error-occurs-when-creating-a-new-database-under-neo4j-4-0 explains that this simply isn't a feasible option
with neo4j community edition

So in the end, I opted for marking the edges I want to query on, and then selecting a transitive closure on these marked edges. This is explained
in detail within the CFG validator project.


### Capturing a path changes the query plan used to match the path, making it take longer

This query terminates very quickly, yielding a `1`.
The query plan shows that first it matches the publisher node, then matches backwards in `varWrite*` for an `isParam`, and then does the `contain`,
and then matches the start ID
```
match ({id:"decl;local_planner;local_planner::LocalPlannerNodelet::globalPathCb(const nav_msgs::PathConstPtr &,);;"})-[:contain]->({isParam:"1"})-[:varWrite*]->({id:"decl;local_planner;local_planner::LocalPlannerNodelet::path_pub_;;"})
return 1
limit 1
```

Prepending a `p=` to the above query, so as to store the resultant path, changes the plan to first filter for `isParam`, then
to do a `varWrite*` for the end ID, and then match the contain, matching the start ID.

The change in order from "match from pub to isParam" to "match from isParam to pub" causes the new query to take forever
```
match p=({id:"decl;local_planner;local_planner::LocalPlannerNodelet::globalPathCb(const nav_msgs::PathConstPtr &,);;"})-[:contain]->({isParam:"1"})-[:varWrite*]->({id:"decl;local_planner;local_planner::LocalPlannerNodelet::path_pub_;;"})
return 1
limit 1
```
