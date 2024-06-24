"""
This script validates dataflow paths within autonomoose.
Specifically, it checks for any callback parameter data that makes its way into a publish.
"""

import driver

"""
Select all callbacks that have a corresponding publisher
"""
def callbackSelector(session):
    return session.run("""
    match ()-[:pubVar]->()-[:pubTarget]->()<-[:contain]-(cb:cFunction)
        where exists((cb)-[:call*0..]->()-[:contain]->(:varWriteNode)-[:edgeLink]->(:rosPublisher))
    return distinct cb.id as cb
    """)

"""
Select all dataflow paths starting with the callback's parameter and ending in a ROS publisher.
"""
def pathSelector(cb, session):
        # Within the scope of the constraining edges, query for all dataflows that start with the callback's parameter
        # and end with a ROS publisher.
        return session.run("""
        match (:cFunction{id:$cbid})-[:contain]->(cbv:cVariable{isParam:"1"})
        match p=(cbv)-[:myVarWrite*]->(:rosPublisher)
        return p
        """, cbid=cb)

driver.run(callbackSelector, pathSelector)
