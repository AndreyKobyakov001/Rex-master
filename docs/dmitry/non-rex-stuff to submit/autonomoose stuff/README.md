The Autonomoose version primarily used was git hash `765f70a` (Nov 2017). Other nearby revisions should work tho.

# Files

`runOnAutonomoose.sh` is a modified version of the existing file under `Rex/Tools`. Use it to run Rex on Autonomoose. See the file
for more info.

**Patchfiles**

These should be applied to Autonomoose to tweak it as per Rex's expectations.
```
git apply <patchfile> // apply the patch
git apply -R <patchfile> // unapply the patch
```

- `cmd_vel_topic_id.patch` renames a topic to a (hopefully) equivalent name. This is due to https://git.uwaterloo.ca/swag/Rex/-/issues/36
    - if Rex is also on a version which strips leading `/` from topic names, then there will be 1 less topic node.

These two rename some callbacks' parameters in the header to be the same as in the source. Rex requires header/source params to be identical.
As a result, there are 2 fewere nodes in Rex.

- `global_path_msg.patch`
- `gridmap_msg.patch`

Under `VehicleControlNodelet::WheelSpeedCb`, there is some code purposefully disabled with a `if (false) { ... }` block.
It gave me some problems at one point, but doesn't seem to anymore... here is a patch just in case
- `remove_unreachable_code.patch`

**Documentation**

`autonomoose_dataflows.md`
- describes the dataflows that were being targeted in the CFG validation work.

`autonomoose_stats.md`
- describes the state of Autonomoose that was being worked on and compares it to previous versions

# Other

- `a = b = c` is forbidden by MISRA C++ 2008 6.2.1, however, Autonomoose violates this in one place:

```
route_publisher/src/tinyspline/tinyspline.c:67 `*k = *s = 0;`
```

- There are three instances of advertisers/subscribers using the topic name in a variable instead of in a string literal.
This is not supported by Rex, but I never got around to tweaking those, was focused on other stuff... can run Rex to find them
among the warnings (search `rex warning:.* string literal`) and replace them with the correct strings

