This document covers my changes and notes about Rex.

## Contents

- [Re-writing the Rex Data Flow Extraction](#re-writing-the-rex-data-flow-extraction)
  - [Background](#background)
  - [Algorithm](#algorithm)
  - [DataFlows Covered](#dataflows-covered)
  - [Capturing function returns](#capturing-function-returns)
  - [Remaining Work](#remaining-work)
    - [Writing through pointers / references](#writing-through-pointers--references)
    - [Class Variables](#class-variables)
- [CFGs](#cfgs)
  - [Default-value calls](#default-value-calls)
  - [Autonomoose post-professing step](#autonomoose-post-professing-step)
  - [Remaining Work](#remaining-work)
    - [Splitting up CFG nodes](#splitting-up-cfg-nodes)
    - [Removing duplicate cfgBlocks entries...](#removing-duplicate-cfgblocks-entries)
  - [An observation about Clang's CFGs](#an-observation-about-clangs-cfgs)
- [Member Initialization Lists (MILs)](#member-initialization-lists-mils)
- [Autonomoose Modules](#autonomoose-modules)
- [Extraneous Logs / Observations](#extraneous-logs--observations)



# Re-writing the Rex Data Flow Extraction

## Background

The existing dataflow algorithm supported only a limited set of C++ cases. However, my work mainly focused on tracking
dataflows within Autonomoose callbacks, and it turned out that a large number of the dataflows that the parameters were used in
directly were ever being caught. For instance, pointer dereferences, multiple indirection, rangefor statements, etc., were
all being missed in some capacity.

The existing read/write extractor had a lot of logic surrounding the distinction between `read` and `write` access, and was
merging them into a separate `both` access case when both were occurring for the same variable. It used these states to
create separate `read` and `write` edges in Rex. This significantly increased
the complexity / reduced readability of the algorithm. However, it turned out that the `read` edges were neither used in any current
analysis, nor were they mentioned in the Rex Thesis.

Additionally, the `varWrite` and `write` edges were being managed independently in some cases, even though semantically speaking it should
always be the case that when a variable gets a `varWrite` in a function, that same variable should also get a `write` edge from
the function.

Lastly, the way this read/write extractor was written made it relatively tricky to extend support for new C++ AST node types,
since the logic was split up in many different ways among different functions. In addition, these methods weren't super composable.
Let's say we want to capture `a = (b = a.foo(b.g(x)))`:
data flows across a bunch of different boundaries:
nested equality, nested function calls, parameter writes, ...
It would be nice to just be able to define the "rules" for the pieces (i.e., for `operator=`, all RHS data flows into LHS),
and then be able to compose these pieces in any way you'd like (Note: this particular example is over-the-top and non-MISRA compliant,
but you get the picture).

Therefore, to facilitate the addition of dataflows for the Autonomoose callbacks I was working with, and also to make it
easier to add new dataflows in the future, I re-wrote the dataflow extractor. In the process, I also removed adding `read` edges,
since they are not used.

## Algorithm

The way in which the data flow extraction works is by examining all data flow "boundaries". These are all points
where data comes in from the "right hand side (rhs)" into the "left hand side (lhs)". The process is as follows:
- as the Clang traversal visits the AST nodes, if the node contains a data flow boundary, then the node is passed into the data flow
extractor through the interface `addWriteRelationships`

```
addWriteRelationships(AST boundary, FunctionDecl currentFunction) {
    // pattern matching on the AST node to find all LHS and RHS subnodes
    // the LHS subnodes are those which receive data accross this boundary
    // the RHS subnodes are those which transmit data accross this boundary
    lhsAST = ...;
    rhsAST = ...;

    // extract the names of all variables in the LHS which end up receiving data
    lhsVariables = extractLValues(lhsAST);
    // extract the names of all variables in the RHS which end up transmitting data
    rhsVariables = extractRValues(rhsAST);

    // add the actual Rex edges
    recordFunctionVarUsage(currentFunction, lhsVariables, rhsVariables);
}

recordFunctionVarUsage(currentFunction, lhsVariableNames, rhsVariableNames)
{
    for lhs in lhsVariableNames
    {
        createWriteRelation(currentFunction, lhs); // currentFunction -write-> lhs
        for rhs in rhsVariableNames
        {
            createVarWriteRelation(lhs, rhs); // rhs -varWrite-> lhs
        }
    }
}
```

Note: in order to actually dispatch the appropriate overload of the AST node, there are delegating functions which take
Expr* and Stmt* types, and then go to the appropriate version (i.e., BinaryOperator*). This is because the visitor pattern
can't really be used, since these are Clang types. Perhaps some template magic would be cleaner? (see the actual implementation in `ParentWalker.cpp`)


With this setup, it now becomes very easy to add a new AST node to the dataflow extractor. See `ParentWalker.h` for
an example of `operator=`. In short, all it takes is to define 3 methods for any candidate AST node:
- the actual dataflow extraction method given above (`addWriteRelationships`)
- 2 extra methods, one for for extracting "rvalues" from the AST node, and the other for "lvalues". These two methods make
the solution very composable, allowing for the detection of arbitrarily nested dataflows in a very modular manner.
    - note: I'm using the term "lvalue" and "rvalue" very loosely -- they don't actually fully correspond to C++ lvalues/rvalues...
    a better name would be nice (i.e., "writingVariables"+"receivingVariables"?)

In addition, the `varWrite` and `write` relationships are now closely and correctly related. For all `varWrite` there is a `write`, but
a `write` can be created even if there is no `varWrite`:
```
void foo() {
    int x = 2;
}
```
- `rhs` will be empty because it's just the literal `2`, but `lhs = "x"`, so we'll get `foo -writes-> x`. Similarly for unary operators (`++x`).

The algorithm is implemented within `ParentWalker.h` and `ParentWalker.cpp`. It is called as part of the Clang traversal within `ROSWalker`.

## DataFlows Covered

These are all documented within their implementation inside `ParentWalker.cpp`

- `CXXConstructExpr` (**new**)
    - parameters now write to the instance being constructed as well as to the parameters
        - note, MIL not yet covered (forgot about these...)

- `BinaryOperator`

- `UnaryOperator`
    - added pointer-deref `operator*` (**new**)

- `ArraySubscriptExpr` -- (**new**)
    - autonomoose had a couple `a.b.c[i].d = x.y.z[j].w`
    - this doesn't do any dataflows, the l/rvalue extractors simply return the array object

- `ReturnStmt` -- new, explained [below](#capturing-function-returns)

- `DeclStmt` -- fixes a bug in the old implementation
    - `int x = y, w = z;`would previously do `z -varWrite-> x,y` when z is actually only writing to `y`.

- `CXXForRangestmt` -- **(new)**, works basically the same as a normal `for` loop

- `CXXOperatorCallExpr` -- **(new)**
    - added overloads for some of the supported operators. This was crucial for Autonomoose's dataflows, which use
    `boost` smart pointers a lot, so `operator*` and `operator->` overloads were necessary.

- `CallExpr` -- see [returns](#capturing-function-returns)

- `DeclRefExpr`

- `MemberExpr`
    - the previous implementation of member expressions did not work for anything other than a variable reference, or `this->`:
        - `a.getB().bar = z` was not recognized
        - `a.arr[i].bar = z` was not recognized
        - ...
    - the new version returns the l/rvalue of the left-most type in the indirection chain
        - `a.foo().arr[i].c = x` --> `x -varWrite-> a`
        - for `this->a`, we still return `MyClass::a`, as before
        - see [class variables](#class-variables) for more info



## Capturing function returns

No dataflows into `a` were previously captured here by Rex.
In order to extend Rex for function returns, added a dummy "ret" node for each function, and added `ReturnStmt` to the
data flow extractor.
```
int add(int x, int y) {
    return x + y; // x -varWrite-> add::ret, y -varWrite-> add::ret
}
a = add(b, c); // b -varWrite-> add::x, c -varWrite-> add::y, add::ret -varWrite-> a
```

- `extractRValues` for function calls now returns the `::ret` node, thereby allowing for the dataflows shown,
and as a result, there now exist transitive dataflows `b -varWrite*2-> a` and `c -varWrite*2-> a`

- Currently this isn't being done, but if the `::ret` nodes are messy, can always remove them in a neo4j post-processing step,
linking together directly the two endpoints they connect

For member functions, we do all the same dataflows as above, but also add a write from the instance. This mimics the behavior of
direct member access `a = myadder.x`, which tries to minimize false negatives caused by the unsolved [class variable](#class-variables)
problem. Once the class variables are solved, can probably start relying *entirely* on just the `::ret` node.
```
int MyAdder::add(int x, int y) {
    return x + y;
}
a = myAdder.add(b, c); // myAdder -varWrite-> a

a = vec.begin(); // *very* common, now we correctly capture `vec -varWrite-> a`
a = vec.at(i); // *very* common, now we correctly capture `vec -varWrite-> a`
```

For library functions, the `::ret` variable is not generated, because we never visit their bodies, so we can never
actually trace return data through to the `return`. To account for this, just give up and return all
parameters in `extractRValues` for these library functions. This definitely adds false positives, like
`a = vec.at(i); // i -varWrite-> a`, but is necessary to capture some Autonomoose dataflows.
```
a = sin(x); // x -varWrite-> a | sin is a library function
a = b->foo(c); // c -varWrite-> a | I forgot where I found this type of library call in Autonomoose, it was a ROS library function...
```

If this becomes annoying, can think of adding an "allow"/"deny" list to special case common methods like `at()`.

Note, though, that now for expressions like `x = a.foo(i)`, there's  a mix of dataflows:
the expected `foo::ret -varWrite-> x`, but also `a -varWrite-> x`, and `i -varWrite-> a` (explained in the class variable problem).
Therefore, there are actually
two "parallel" dataflows modelled here:
```
i -varWrite-> foo::param -varWrite*-> foo::ret -varWrite-> x
i -varWrite-> a -varWrite-> x
```
Again, this is as a consequence of attempting to avoid false negatives with respect to the class variable dataflow problem.
Once resolved, we can rely more on trusting the innards of functions without assuming what gets returned, thus hopefully
being able to eliminate the `i -varWrite-> a -varWrite-> x` dataflow.


## Remaining Work

- extend support for more overloaded operators:
    - `operator()()` -- used by `map(x,y)` in `LocalPlannerNodelet::gridMapCb`
    - `operator+` for strings (not very high priority)
    - ...
- pointer-to-member operators `.*` and `->*` (super low priority)
- [MILs](#member-initialization-lists-mils)




### Writing through pointers / references

Writes through non-const pointers / references (`(*p) = x`) need to be implemented because they are used a fair amount
in Autonomoose, including within callback dataflows. As per MISRA standards, a pointer/reference in a function parameter
should be `const` if the parameter is not modified, so this stuff should only be tracked if it is not `const`.

A brief attempt was made at including them into Rex, but it didn't work.
For every variable that got initialized to a reference:
```
int& a = b;
```
I generated `b -varWrite-> a` as per usual, but then added a `a -varWrite-> b` as well. Now, any variable that writes to `a`:
`a = x`, will now also transitively write to `b`! Of course, this doesn't work because this the `a -varWrite-> b` edge creates cycles in dataflow that aren't really there:
`b -> a -> b`, `a -> b -> a`. So if `x` is used in a lot of dataflows, this causes a lot of invalid paths:
```
.... bunch of possible dataflow branches ... -> x -> a -> b -> a
```

This is fundamentally caused by a misuse of the `varWrite` abstraction. `varWrite` is meant to track the flow of *data*. If `a = &b`,
we aren't really writing `b`'s data to `a`. And `(*a) = x` doesn't really mean that `x` gets written to `a`. Rather, there is an intermediate
`refersTo` relationship which links the two endpoints `b` and `x` together, and the writes themselves are more like `writesThrough`.

Now, the original solution was made before the CFG validation
work started using neo4j to do post-processing on the database, preparing it for more complex queries. However, now that this is
potentially something that will stay, it might be possible to add these proposed relationships in Rex, and then process the
resulting transitive writes in the neo4j post-processing step. For instance,
```
// fyi, isReference would also need to be added to Rex
match (src:cVariable{isReference:"0"})-[:writesThrough]->(:cVariable{isReference:"1"})-[:refersTo*]->(dst:cVariable{isReference:"0"})
merge (src)-[:varWrite]->(dst)
```

Of course, this now poses the problem of tracking locations of these `varWrite` edges, since in visualization, it would be useful
to be able to show dataflow paths as they relate to the actual source code. In addition, the CFG checking uses CFG location information
of `varWrite` edges to eliminate false positives. Perhaps there would need to be an "unmapping" phase, where we recover the original `refersTo`/`writesThrough` paths?

More thought is needed.

Another question is how to handle pointer re-assignment.

```
a = &b;
*a = x;
a = &c;
*a = y;
```

Ideally, we would want to limit the amount of false positives generated from these cases as much as possible: `x -varWrite*-> c` and
`y -varWrite*-> b` should not be part of the graph!

Also, what edge would we use if we're assigning the contents of one pointer to another?
```
int* a;
int* b;
a = b;
```

**Dataflow Abstraction**

If we are to track `a = &b` or `*a = b` using the new Rex dataflow extraction, there are two main ways to do this:

```
addWriteRelationships(AST boundary, FunctionDecl currentFunction) {
    lhsAST = ...; // add pattern matching for `operator*`
    rhsAST = ...; // add pattern matching for `operator&`

    lhsVariables = extractLValues(lhsAST);
    rhsVariables = extractRValues(rhsAST);

    // either use custom write logic here based on the results of the pattern match,
    // or delegate it to `recordFunctionVarUsage`
    recordFunctionVarUsage(...);
}
```
The problem with this approach is that it would force constantly repeating this logic across all the different boundaries where
it can occur (i.e., function parameters, range-for loops, declarations, assignment operators, ...).
The whole point of the dataflow abstraction was to allow for easy re-usable composition of dataflows.

An alternative, probably better, approach is to enhance the data being returned by the `extractLValue`/`extractRValue` methods with type
information. Currently, all they do is return the IDs of the variables acting as "r/lvalues", but if we extend support to include
type info, then we can "tag" the returned types with extra stuff like "this is an address to ...", and then let `recordFunctionVarUsage`,
which is a shared point among all the AST node types, to deal with the logic of what edges actually need to be added.
Example:
```
// Currently these all return sets of strings, but say they are changed to return "variables" instead
set<MyVariable> extractLValues(DeclRef) { ... }
set<MyVariable> extractLValues(MemberExpr) { ... }
set<MyVariable> extractLValues(CallExpr) { ... }
...
set<MyVariable> extractLValues(UnaryOperator op) {
    if ("is operator&") {
        set<MyVariable> nestedLValue = extractLValues(op->getSubExpr());

        for (auto& t: nestedLValue) {
            // In this context, it does seem a little weird to be returning *multiple* addresses, but it might just have to be
            // that way -- see the "Class Variables" section
            t.type = MyTypeSystem::AddressOf(t.type);
        }
        return nestedLValues;
    }
}

recordFunctionVarUsage(curFunc, set<MyVariable> lhsVars, set<MyVariable> rhsVars) {
    // same as before, except now only add `varWrite` if the lhs and rhs are both non-address types
    // otherwise, add the custom `refersTo` edges or whatever the solution ends up being
}

// Implement `operator*` as "Deref" type similarly
```


**Useful Code**

Here is a function I used when I was experimenting to see if a type is a non-const pointer or reference.
```
bool isNonConstPointerOrReference(QualType type) {
    if (type->isReferenceType() && !type->getPointeeType().isConstQualified()) {
        return true;
    } else if (type->isPointerType()) {
        while (type->isPointerType())
            type = type->getPointeeType();

        if (!type.isConstQualified())
            return true;
    }
    return false;
}
```


### Class Variables

The previous dataflow extractor in Rex used the following lvalues/rvalues when creating dataflows.
```
a.b = c ==> c -varWrite-> a
a.b.c = d ==> d -varWrite-> B::c
```

Basically, if there was more than one level of indirection, the class variable was used as the l/rvalue; otherwise,
use the lhs of the dot operator. This is a little inconsistent.

Since the dataflow extractor is now very composable, it was very easy to extend the class variable logic to the whole member chain:
```
lvalues(a.b.c) =  B::c + lvalues(a.b) = B::c + A::b + lvalues(a) = B::c + A::b + a
```

However, using this in the l/rvalue extraction methods actually created a **lot** of false positive dataflows within Autonomoose,
because anytime data flows into a class variable, it would then flow into **all** the places that class variable is used --
instance information is not used. This also tended to create a lot of self-write cycles.
A big problematic instance of this is the `State` class used in the `local_planner`
module in autonomoose.

All in all, the problem is caused by an incorrect use of class variables altogether, so for now they have been removed:
`d = a.b.c` --> `a -varWrite-> d`

Instead, a different approach is needed if we are to improve the accuracy of tracking member data.

An immediate improvement that can be made is to track as much instancing information as possible.
Then, as a neo4j post-processing step, we can merge all instancing information with the writes. Example:
```
// Consider the proposed edges:

A::setM(int v) {
    this->m = v; // A -contains-> A::m <-varWrite- v.
                 // As part of the CFG work, we are also now tracking that the `varWrite` is inside `A::setM`
}
a.setM(b); // a -calledOn-> A::setM   (`calledOn` is not a real edge... yet)

// With this info, neo4j could somehow join this stuff:

Since a is called on A::setM, then we can replace the `v -varWrite-> A::m` with `v -varWrite-> a.m`
```
- the reason to delay this to the post-processing phase is to account for classes imported from separate compilation units, which Rex
visits separately

This would also capture the case of classes that never get instantiated, like callbacks in Autonomoose.
These methods belong to "nodelet" classes,
which are never explicitly instantiated by the user code. Instead, they are hooked up to the ROS framework through macros like
`PLUGINLIB_DECLARE_CLASS`, and are then instantiated / called within the workings of ROS.
So the neo4j post-processing step would simply not be able to find instances for these methods, and just leave the `varWrite` edges be,
`v -varWrite-> MyNodelet::m`, which is the best that can be done.


However, the whole process described here doesn't capture all dataflows.

```
a.setM(m); // m -varWrite-> a.m
b = a;     // a -varWrite-> b
x = b.getM(); // b.m -varWrite-> x
-----
Missing `m -varWrite*-> x`! How to introduce this as precisely as possible? What if there's
multiple levels of indirection: b.c.d.getE(), or multiple levels of re-assignment `b = a; c = b; ...`
```

Creating the edges `b -varWrite-> x` and `m -varWrite-> a` would solve this (and this is the current solution), but with it comes a whole slew
of false positives:

```
a.setM(m); // m -varWrite-> a
b = a; // b -varWrite-> a
x = b.getN(); // b -varWrite-> x

m -varWrite-> a -varWrite-> b -varWrite-> x is now a false positive! (because calling `getN()` not `getM()`)
```

Note: this whole problem of instance re-assignment is also relevant to direct member access, not just functions.
```
a = d.m;
b = a;
x = a.m;
```

For now, use this false-positive solution: for any indirection, always use the left-most object as the l/rvalue:
`a = b.c ==> b -varWrite-> a`, `a = b.foo() ==> b -varWrite-> a`, to avoid the false negatives described.

More thought is needed.




# CFGs

Clang's CFGs were added to the fact extractor in order to enable detection of false positive facts.
```
z = y;
y = x
```
Here, `x -varWrite-> y -varWrite-> x` is a false positive. Surrounding the block in a loop, however, is no longer a false
positive:
```
while (y < 10) {
    z = y;
    y = x
}
```
Note that we are not examining the data itself yet -- so let's say it could be known that `x >= 10` before the loop,
then the loop would technically only run once, and the dataflow would still be a false positive. This isn't part of the analysis
yet.

**Adding the CFGs**

This process is documented throughout the CFG building process in `ParentWalker.h`, `ParentWalker.cpp`, `ROSWalker.cpp`, especially
in the `ParentWalker::CFGModel` class. In brief, the CFGs are added to Rex through a 3-step process that is repeated per-function that
is visited by Rex:
- build the function's CFG and map the basic blocks to the function's AST
- as Rex generates facts, it links them to their containing CFG blocks, and marks those blocks as "used"
- the CFG filters out unused blocks, trying to reduce the number of potential CFG paths that will have to be analyzed, and finally
  uses the remaining blocks to build the CFG TA graph.

**CFG Graph Schema:**

The CFG TA graph consists of `cCFGBlock` nodes. If a CFG block leads directly to a successor block within the same function,
it connects to it through a `contain` edge. If a CFG block calls a function, it is connected to the `ENTRY` block of the callee's
CFG through an `invoke` edge. Each function is connected to its CFG blocks through a `contain` edge. Each function
is connected to its `ENTRY` CFG block through a `functionCFGLink` edge.

Clang builds its CFGs in reverse order of control flow, and each function gets one exit block. As a result, the exit block
always has ID 0. The entry block ID, on the other hand, depends on the total number of blocks in each function, so it gets its own
special ID.

When linking an edge to a block in the CFG, simply add the block's ID to the edge's list attribute `cfgBlocks`. It was also
useful to link each edge to its containing functions, so the function ID is added to the list `containingFuncs`.
This information is used in the Autonomoose post-processing step.

## Default-value calls

Autonomoose has some cases where a function parameter's default value is the result of a function call:
```
Rex Warning: Could not find a mapping from call expression to CFG block. /mnt/c/Users/dimit/Documents/ura2/catkin_ws/src/autonomoose/rospackages/autonomoose_core/local_planner/include/local_planner/opt_local_planner/grid_map.hpp:81:51

opt_error setGeometry(
  const opt_local_planner::Dimensions &length,
  const double resolution,
  const opt_local_planner::Position &origin = Position::Zero()); // <--
```


The CFG isn't able to capture these calls, since they're not really part of runtime control flow.... However, the assumption is that these functions are pure, since it would make sense that behavior prior to the function's body should not change whether a user provides a default value or not, and if the function had any side effects (writing to members, publishes, etc...) that would be a huge code smell.
If these assumptions hold, then these calls shouldn't be useful for the analysis anyways, it's ok that the CFG skips them.

**Although**: one *potential* case in which this is still a problem is something like:

```
class A {
public:
    int getMem() { return mem; }
    void foo(int x=getMem()) { ... }
private:
    int mem;
};

void bar(A a, int y) {
    a.mem = y;
    a.foo(); // now `y` flows transitively into `foo::x`, but the CFG is unable to confirm this
}
```

However this seems **really** unlikely?


## Autonomoose post-professing step

This is documented within `preprocess.cypher` under the `main` version of the CFG validator. In brief, in order to simplify
querying using the CFG/block ID lists given to each edge (`edge.cfgBlocks`, `edge.containingFuncs`), create nodes corresponding to edges, and link those nodes directly to the items in the edge's `cfgBlocks`/`containingFuncs` lists.

## Remaining Work

- [MILs](#member-initialization-lists-mils)

### Splitting up CFG nodes

Basic-blocks are successfully split up. However, it turns out that the resultant "subblocks" should be split up even further.

To get a more in-depth overview of how node splitting works, see the implementation's documentation in Rex.

This is a brief summary in the commit message implementing the splitting.

---

Split CFG basic blocks into statement-level blocks.

Before this commit, a single Rex CFG node represented a single Clang CFG
basic block. As a result, a single node could contain multiple facts
that form a dataflow.
For instance:
```
void foo(int x, int y, int z) {
    z = y; // y -varWrite-> z
    y = x; // x -varWrite-> y
}
```

would generate a single CFG node containing both varWrite facts,
and it would therefore be impossible to know that the dataflow `x
-varWrite-> y -varWrite-> z` is infeasible.

This commit splits up all basic blocks into their individual statements.
So the above example would generate a separate Rex CFG node for each of the
two assignments, and thus the order of these nodes could be used to
invalidate the proposed dataflow.

Unfortunately, this problem is not completely solved because Rex can
generate multiple facts per AST statement.
Some examples of cases that can still generate undetectable false
positives:

```
// Found in Autonomoose. Each individual case may occur many times throughout Autonomoose.
a = a + b // b -> a -> a


a = b->foo(a); // foo is a library call, so there exists edge a -> a
               // thus we get b -> a -> a


// Found in SandwichBar
a.foo(a->at(i)->getBar() - var); // var -> a -> a
                                 // i -> a -> a
                                 // a->a->foo()::param

///// Examples found in Autonomoose that rely on class variables. Currently these are disabled (see notes on Class Variables),
///// but if re-introduced, these will occur.

a.r = b.at(i).r; // i -> r -> r

a.foo(a.b); // b -> a -> a

a.b.erase(x, a.b.end()); // x -> a -> b

a.x = b.x; // b -> x -> a
           // x -> x -> a
           // b -> x -> x
```

---


Some other examples of multiple varWrites falling under the same CFG block.

```
void foo() {
    int x = a.bar(baz(y));
}

CFG:

 [B1]
   1: baz(y)
   2: a.bar([B1.1])
   3: int x = a.bar(baz(y)); // the `baz(y)` is the same AST node, not sure why the clang printer doesn't replace it with [B1.1] here, but does in the stmt above

1.1
y -varWrite-> baz::param
foo -call-> baz

1.2
baz::ret -varWrite-> bar::param
baz::ret -varWrite-> a
foo -call-> bar

1.3
bar::ret -varWrite-> x
a -varWrite-> x

Notice how the parameter writes are always in the same block as the function calls.
Also notice how the parameter writes are always in the same block as the writes to the class instance.
```

So to resolve this, need to further split up the new CFG blocks.

A proposed way to do this is to integrate the CFG splitting logic closely with the new dataflow logic.
For instance, when analyzing `a = b.foo(x)`, we would insert a "barrier" between the writes on either side of the `=`, indicating
that the writes on the right side should get their own CFG block that should link into the writes on the left side.

### Removing duplicate cfgBlocks entries...

For some reason this returns results, and indeed, there are duplicate entries in the list of cfgBlocks. Not entirely sure why...
```
match p=()-[r:varWrite]->()
with r.cfgBlocks as l, r
where size(apoc.coll.intersection(l, l)) <> size(l)
return r.cfgBlocks
```

## An observation about Clang's CFGs

```
/// CFGBuilder - This class implements CFG construction from an AST.
///   The builder is stateful: an instance of the builder should be used to only
///   construct a single CFG.
///
///   Example usage:
///
///     CFGBuilder builder;
///     std::unique_ptr<CFG> cfg = builder.buildCFG(decl, stmt1);
///
///  CFG construction is done via a recursive walk of an AST.  We actually parse
///  the AST in reverse order so that the successor of a basic block is
///  constructed prior to its predecessor.  This allows us to nicely capture
///  implicit fall-throughs without extra basic blocks.
class CFGBuilder {
```
- https://clang.llvm.org/doxygen/CFG_8cpp_source.html#l00466

The claim that "a block is always constructed prior to its predecessor" is impossible, since the CFG
is **not** a DAG.

```
void foo() {
    for (auto i = 0; i < 1; i ++) {}
}
```

we get this CFG:

```
[B4 (ENTRY)]
   Succs (1): B3

[B1]
  1: i++
  Preds (1): B2
  Succs (1): B2

[B2]
  1: i < 1
  T: for (...; [B2.1]; ...)
  Preds (2): B1 B3
  Succs (2): B1 B0

[B3]
  1: auto i = 0;
  Preds (1): B4
  Succs (1): B2

[B0 (EXIT)]
  Preds (1): B2
```

Here there is a cycle between B1 and B2

Looking at the CFG.cpp and CFG.h source code, it does seem that whenever a block is added to the block list, it is
appended and constructed during this process (ctrl-f `Blocks.`), and if a cycle occurs, it occurs for a block
that has already been visited, and the block is *not* re-added to the list. So if we trust that the claim about
the CFG being built in reverse order is true, then it should always be the case that for two connected CFG blocks,
the one with the higher ID will get executed *first*, even if there is eventually a cycle between the two

# Member Initialization Lists (MILs)

MILs are explained separately here because they rely on [class variables](#class-variables), the [CFG](#cfgs) work, and also on
the new [dataflows](#re-writing-the-rex-data-flow-extraction).

MIL dataflow + CFG tracking simply hasn't been implemented yet. However, it does rely on class variables.

---

When constructing an object:
```
A::A(int p): x_(p) { } // missing `A(int)::p -varWrite-> x_`

void foo(int x) {
    A a(x); // x -varWrite-> a
            // x -varWrite-> A(int)::p
}
```

we correctly capture that `x -varWrite-> a`, and that `x -varWrite-> A(int)::p`, but we miss any `varWrite`s that end
up happening in the member initialization list. So this isn't completely horrible, since we are capturing the critical
write from `x` to `a`, but we are missing precision with tracking the flow of `x` through `A`'s MIL into its member variables.
Of course, this re-raises the issue of class variables.

Here is an example demonstrating what we do and don't currently track in the AST dataflows and in the CFG:

```
struct B {
    int b;
    B(const B& o) {} // non-trivial copy-ctor causes a bunch of varWrites
};
struct A {
    B x;
    A(B y):
        x(y) // the AST dataflows capture that `y -varWrite-> B::o`, but can't find a CFG mapping for this particular varWrite! warning generated
             // the AST dataflows miss the important `y -varWrite-> x`, CFG probably does as well, but no warning raised
    {}
};
void foo(B i) {
    A a(i); // caputures that `i -varWrite-> B::o`, CFG captures this completely fine, unlike above
            // both CFG and dataflows also capture that `i -varWrite-> a` and that `i -varWrite-> A::y`, but that is as expected, unrelated to MIL
}
```

- For types that have non-trivial copy-ctors, there actually are a bunch of varWrites that happen, since the copy-ctor's
params are written to. In the example above, it turns out that for one particular `varWrite` captured by Rex, the CFG misses it, so
this generates a warning -- this warning gets raised in a decent amount of places when running on Autonomoose.

- However, the actual varWrite that we really care about in the MIL, `y -varWrite-> x`, is missed by Rex (and probably the CFG as well, but
no warning is raised since no fact is generated, and we only warn when a Rex fact can't be linked to the CFG...)

# Autonomoose Modules

**Modules**

Rex doesn't have a formal notion of what a ROS module is. A ROS module can only interact with other ROS modules through
publish/subscribe calls. However, existing cypher queries extract a variable/function/whatever/...'s module from its filename:
```
Snippet taken with slight modification from `allCrossModuleDataflow.cypher`, which is an old behavior alteration script found within an old folder
of queries @RobHackman had lying around.

...
match (srcFunction:cFunction)
substring(split(srcFunction.filename,'/')[-1], 0, 3) as srcModule
...
```

This is misleading though, because there are libraries within Autonomoose that end up appearing to be under a separate "module", even though
they aren't actually a ROS module.

```
This is a very old, highly sub-optimal query that I was messing around with to select dataflows across 3 or more distinct modules --
it's not actually selecting cross-module dataflows as I originally thought, since it's not
examining `pubVar/pubTarget` pairs, but instead only following `varWrite`s, which always fall within one module. However, it is
exemplary of the type of "module" extraction Cypher queries are currently doing, and it actually ends up returning results since these "modules"
don't reflect real ROS modules.

match p=(srcFunc:cFunction)-[:write]->()-[:varWrite*0..]->()-[:varInfFunc]->(dstFunc)
with *, toLower(substring(split(srcFunc.filename,'/')[-1], 0, 3)) as srcModule, toLower(substring(split(dstFunc.filename,'/')[-1], 0, 3)) as dstModule
where srcFunc.id <> dstFunc.id and srcModule <> dstModule
match (n)
where n in nodes(p) and n <> srcFunc and n <> dstFunc and toLower(substring(split(n.filename,'/')[-1], 0, 3)) <> srcModule and toLower(substring(split(n.filename,'/')[-1], 0, 3)) <> dstModule
return distinct p;
```

# Extraneous Logs / Observations


1) Functions can be on the LHS of `varWrite` relation, this happens when callbacks are passed to the `subscribe` calls in Autonomoose.
Not sure if this is problematic...

2) I observed that some callbacks and functions had **really** large CFG paths, even though they really weren't all that long... it turned out
that the ROS logging macros (i.e., `NODELET_DEBUG`) were to blame. It turned out that we were also adding Rex facts for these. So the
`ParentWalker::isInSystemHeader` check was extended to cover these, and the capability to delete arbitrary nodes was added to the CFG


3) These were some old queries I was using to detect dataflows:

---

Get all distinct dataflows between two pub/sub interfaces. Doesn't terminate due to unconstrainted `varWrite`
```
match p1=()-[:pubVar]->()-[:pubTarget]->()-[:varWrite]->(b), p2=(b)-[:varWrite*0..]->()-[:pubVar]->()-[:subscribe]->()-[:call]->()
with apoc.path.combine(p1,p2) as p
return p
```

---

4) The following is from before I was marking edges in the CFG validator to constrain the `varWrite*` queries I was doing,
and before class variables were removed. As a result, these queries were not terminating, and I was trying to find the cause -- the conclusion
was that class members + false positives on class instances were the culprits.

---

There are too many cycles to enumerate all `varWrite*` paths from `this->opt`, and therefore from 3 callbacks within `LocalPlannerNodelet`.

3 Rex versions were used. These can be succinctly shown in terms of the following expression: `a.b = d.e.foo(x.y)`

1.
- `d, x write to a`
- `x writes to d`
- `x writes to foo::param1`
2.
- `d, D::e write to a, A::b`
- `x, X::y write to d, D::e`
- `x, X::y write to foo::param1
3.
- for library function calls, same as (1); otherwise,
- `x` writes to foo::param1
- `foo::ret` writes to `a`

The idea is that version `3` will most accurately model dataflow, since it will minimize the amount of optimistic false positives introduced by versions (1) and (2).

Take example (2), which is a superset of (1):
```
a.b = d.e.foo(x.y)
```
- `x, X::y write to a, A::b`
    - there is no guarantee that the data flowing from the parameters will make it out of the function
- `x, X::y write to d, D::e`
    - there is no guarantee that the function will write to `D::e`. Furthermore, saying that it writes to `d` is too vague (`d` can have many data members!)


However, for all 3 versions, there is a significant amount of `varWrite*` results for `this->opt`:

Case 2 is a superset of case 1 and adds **significantly** more cycles. This is presumed to be the reason for the path explosion.

Case 3 is relies on function/parameter dataflow as opposed to "optimistic" parameter-to-instance-to-lhs writes. Unfortunately, the returns introduce
a new type of path explosion: if both `f` and `g` call `h`, then all dataflows entering `h` will return into both `f` and `g`.


So it seems like regardless of the approach being used, in order to make the queries terminate, we will need to match not all possible paths, but some significantly smaller subset.

---


