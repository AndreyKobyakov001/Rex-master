// This script creats a bunch of structures within the graph to speed up the queries needed by
// the validator.
// Specifically, each `varWrite` and `call` edge has a corresponding list of CFG node IDs and function IDs in which it
// occurs. However, since it's just a list of IDs, getting to the actual CFG/function node requires a query like:
// match (c:cCFGBlock) where c.id in r.cfgBlocks, where r is a `varWrite` or `call` edge. This is inefficient since it
// requires scanning through all cfg/function blocks.
//
// Instead, this script builds up alternate structures which create nodes for each varWrite/call, and then
// link those nodes directly to the appropriate CFG/function node.


// Variable and function ID index. Without these, runs very slowly
create index varidx for (n:cVariable) on (n.id);
create index funcidx for (n:cFunction) on (n.id);
create index blockidx for (n:cCFGBlock) on (n.id);

// For every `varWrite` relation, create a parallel `varWrite` node.
// These nodes are used to link against CFGBlocks and functions, thus removing the need to
// query against the lists `cfgBlocks` and `containingFuncs`
match (a)-[rs:varWrite]->(b)
with "varWrite:"+a.id+"-->"+b.id as edgeID, a, b
create (a)-[:edgeLink]->(:varWriteNode{id:edgeID})-[:edgeLink]->(b);

// Similarly to above, create `call` nodes
match (a)-[rs:call]->(b)
with "call:"+a.id+"-->"+b.id as edgeID, a, b
create (a)-[:edgeLink]->(:callNode{id:edgeID})-[:edgeLink]->(b);

// Now that the nodes are made, link them to the CFG graph, and to their enclosing functions

// Link varWrite node to enclosing function
match (a)-[r:varWrite]->(b), (a)-[:edgeLink]->(edge:varWriteNode)-[:edgeLink]->(b), (f:cFunction) where f.id in r.containingFuncs
create (f)-[:contain]->(edge);

// Link varWrite node to cfg
match p=(a)-[r:varWrite]->(b), (a)-[:edgeLink]->(edge:varWriteNode)-[:edgeLink]->(b)
unwind r.cfgBlocks as cfgs
with distinct cfgs, p, edge
match (cfg:cCFGBlock{id:cfgs})
create (edge)-[:cfgLink]->(cfg);

// No need to link call node to enclosing function since that can only ever be the calling function!

// Link call node to CFG
match p=(a)-[r:call]->(b), (a)-[:edgeLink]->(edge:callNode)-[:edgeLink]->(b)
unwind r.cfgBlocks as cfgs
with distinct cfgs, p, edge
match (cfg:cCFGBlock{id:cfgs})
create (edge)-[:cfgLink]->(cfg);

// Goal: Instead of querying on the old ()-[:call*]->() edges, and then mapping from each edge to its corresponding
// node, would like to just be able to do transitive call queries on the new callNodes.
//
// Problem: Neo4j doesn't allow to do * on patterns, only on individual edges. So we can't do something like:
// (()-[:edgeLink]->(:callNode)-[:edgeLink]->())*
//
// Solution: Link consecutive call nodes together using a new edge "callpair". Can now achieve the above using
// ()-[:edgeLink]->(:callNode)-[:callpair*0..]->(:callNode)-[:edgeLink]->()
match (a:callNode)-[:edgeLink]->()-[:edgeLink]->(b:callNode)
create (a)-[:callpair]->(b);

/*
 * Unfortunately, the trying to do the same for the `varWriteNode` nodes gives problems.
 *
 * This is because the above strategy for doing "transitive closure over edge nodes" returns the same results as
 * the normal "transitive closure over edges" (i.e., the typical ()-[:call*]->()) ONLY IF there are no cycles
 * in the old edges. For `[:call]` edges, this turns out to be true because MISRA does not permit recursion.
 *
 * However, for `varWrite`, this is most definitely NOT true, and the query results differ.
 *
 * Consider if consecutive varWriteNode nodes were linked together through an edge `dataflow`.
 * Then we could achieve a transitive closure using
 *
 * (:varWriteNode)-[:dataflow*0..]->(:varWriteNode) (1)
 *
 * The dataflow that this path *should* represent is:
 *
 * (a)-[:edgeLink]->(:varWriteNode)-[:edgeLink]->()-[:edgeLink]->(:varWriteNode)-...->(:varWriteNode)-[:edgeLink]->(b)
 *  - (where the two endpoints are :cVariable or :rosPublisher, or any other node type that an have a :varWrite)
 *
 * And this is equivalent, as per the mapping of `[:varWrite]` edges to `(:varWriteNode)` nodes, to (a)-[:varWrite*]->(b)
 *
 * However, running query (1) on autonomoose resulted in the following case:
 */
 // `(w:varWriteNode /* a->b */)-[:dataflow]->(x:varWriteNode /* b->a */)-[:dataflow]->(y:varWriteNode /* a->b */)
 //     -[:dataflow]->(z:varWriteNode /* b->c */)`  (2)
/*
 * Which is really representing
 *
 *  (a)-[:edgeLink]->(:varWriteNode{id:w.id})-[:edgeLink]->(b)-[:edgeLink]->(:varWriteNode{id:x.id})-[:edgeLink]->
 *  (a)-[:edgeLink]->(:varWriteNode{id:y.id})-[:edgeLink]->(b)-[:edgeLink]->(:varWriteNode{id:z.id})-[:edgeLink]->(c)  // (3)
 */
 // AKA per the mapping, (a)-[:varWrite /* w */]->(b)-[:varWrite /* x */]->(a)-[:varWrite /* y */]->(b)-[:varWrite /* z */]->(c) // (4)
/*
 * Unfortunately, this pattern actually cannot be matched using cypher patterns.
 * While cypher allows queries in the form (i)-[]->(j)-[]->(i)-[]->(k), which is what (2) is, it does NOT allow queries of the form
 * `(i)-[]->(j)-[]->(i)-[]->(j)-...`, which is the form of (3) and (4).
 *
 * It's possible a workaround may be done using APOC paths, but this wasn't attempted.
 *
 * So instead, the driver is for now selecting the old-style ()-[:varWrite*]->() and then manually mapping the edges to
 * the (:varWriteNode) nodes.
 * TODO: This **significantly** slows down, see _mapPath in `driver.py`
 */
