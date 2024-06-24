This file compares Rex's output by querying the generated neo4j database. In order to generate the database, the `-c` (CSV) option
was used. CFGs are not included in the comparison.
I'm not sure entirely how useful these results are, but I started collecting them at one point, so here they are.

The Master's Thesis (Detecting Feature-Interaction Hotspots in Automotive Software using Relational Algebra) contains some
stats as to what Rex was extracting at the time it was written. These can be found on page 77 and later. The version of Autonomoose that
was used there was from late October 2017. The version I was running on was commit 765f70a, which is Nov 7 2017.



The following versions of Rex are compared here:
The following data is comparing Autonomoose as run by Rex on commit 7da4a2a, from Sept. 8 2020, and the final version of my work.

Sample 1: Rex @7da4a2a Sept 8, 2020
- compilation error on `ref_ekf`
- 58 duplicate entries in the generate Cypher CSV file -- I manually removed them
- applied all 3 patchfiles to Autonomoose (except the unreachable code one)

Sample 2: Rex @4d4b81c Nov 2, 2020
- some updates to Rex's extraction, but pre-new dataflow
- no more compilation errors on Autonomoose
- manually removed 58 duplicate entries in CSV
- manually insepected all subscribe calls in Autonomoose. There are 43, we are extracting 40 cause of the 3x string literal issue
- manually inspected all advertise calls in Autonomoose. There are 44, we are extracting all
- manually inspected all publish calls in Autonomoose. There are 58 valid ones (one is `debug_points_publisher` which doesn't have a corresponding advertise). However, out of these 58, there are 44 unique publishers, all of which are extracted.
- applied all 3 patchfiles to Autonomoose (except the unreachable code one)
- same 3 patchfiles applied

Sample 3: Latest version (my branch)
- all my latest work, including the new dataflows Rex extracts, and removing leading `/` from topic names.
- no more duplication in CSV
- same 3 patchfiles as above


All three versions have the 3 `subscribe` calls that we're not registering due to them not using string literals:
```
Rex Warning: topic names that are not string literals are not supported. `autonomoose/rospackages/autonomoose_core/vehicle_detection/src/vehicle_detection_nodelet.cpp:317:7`
Rex Warning: topic names that are not string literals are not supported. `autonomoose/rospackages/autonomoose_core/vehicle_detection/src/vehicle_detection_nodelet.cpp:322:7`
Rex Warning: topic names that are not string literals are not supported. `autonomoose/rospackages/autonomoose_core/vehicle_detection/src/vehicle_detection_nodelet.cpp:328:7`
```

Cell entries containing `-` mean their value is the same as the cell to its left

---

**Nodes:**

```
match (n)
return count(n)
```
||sample1|sample2|sample3|
| ------------- | -----:|-:|-:|
| nodes | 5648 | 5801 | 5759 |

- The decrease in nodes from sample2 to sample3 is attributed
to Rex no longer creating nodes for ROS macro code.

**Labels**

```
match (n)
return distinct count(labels(n)), labels(n)
```

||sample1|sample2|sample3|
| ------------- | -----:|-:|-:|
| cClass | 40 | - | - |
| cEnum | 9 | - | - |
| cFunction | 1529 | 1585 | 1450 |
| cStruct | 37 | - | 25 |
| cReturn | n/a | n/a | 359 |
| cVariable | 3832 | 3929 | 3684 |
| rosNodeHandle | 28 | - | - |
| rosPublisher | 51 | - | - |
| rosSubscriber | 46 | - | - |
| rosTimer | 9 | - | - |
| rosTopic | 67 | - | 58 |

- the decrease in rosTopics is due to the leading `/` now being stripped away in `ROSWalker`
- `cReturn` edges were introduced in sample 3

**Edges::**

```
match ()-[r]->()
return count(r)
```

||sample1|sample2|sample3|
|-|-:|-:|-:|
| edges | 17113 | 16916 | 18201 |


```
match ()-[r]->()
return distinct count(type(r)), type(r)
```

||sample1|sample2|sample3|
|--|--:|--:|--:|
| advertise | 44 | - | - |
| call | 2498 | 2581 | 2162 |
| contain | 4467 | 4579 | 4334 |
| obj | 210 | - | 198 |
| pubTarget | 40 | - | - |
| pubVar | 55 | - | - |
| publish | 44 | - | - |
| read | 1964 | 2010 | n/a |
| subscribe | 40 | - | - |
| varInfFunc | 1739 | 1194 | 1248 |
| varInfluence | 33 | 27 | - |
| varWrite | 3375 | 3392 | 5858 |
| write | 2604 | 2700 | 4151 |
- read edges were removed in sample3


- attributing the initial sharp decrease in varInfFunc due to the fact that sample 1 had many `varInfFunc cFunction cFunction` occurrences, and the later increase because in sample 3 we are now tracking `varInfFunc` for nested conditional statements.
- attributing to decrease in many edges, apart from the `*write` edges due to no longer visiting ROS macros


||sample1|sample2|sample3|
|-|-:|-:|-:|
|`()-[:publish]->()-[:subscribe]->()`|13|-|23|
|`()-[:advertise]->()-[:subscribe]->()`|13|-|23|
|`()-[:pubVar]->()-[:pubTarget]->()`|16|-|26|
|`()-[:pubTarget]->()-[:varWrite]->()`|5|-|55|
|`()-[:pubVar]->()-[:pubTarget]->()-[:varWrite]->()`|0|-|32|

- the thesis says there's only 12 direct communications, but here we have 13
- results for the first two rows should be the same number
- results for row 1 (and 2) and 4 have similar numbers, but don't completely overlap -- some subscribers write but aren't published to,
and some subscribers don't write but are published to.
- also note that there are topics that are being published to by multiple publishers, but all topics are subscribed to by unique
subscribers:

    ```
    // Queries showing that subscribers don't share topics

    match p=(:rosTopic)-[:subscribe]->(:rosSubscriber)
        return count(p) // 40 paths in total

    match p=(:rosTopic)-[:subscribe]->(c:rosSubscriber)
        with distinct c as dc
        return count(dc) // 40: same number of unique subscribers involved in these paths as there are total paths
    ```
