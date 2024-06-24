# Overview

This project provides an alternate implementation of the validation algorithm used to check
whether a dataflow path is valid with respect to a program's CFG.

See `../main/README.md` for details about how this project and the `main` project are setup.

The relevant differences between this version and the one under `../main` are:
- `validator.py` -- completely different logic
- `preprocess.cypher` -- adds return edges to the graph
- `driver.cypher` -- adds constraints onto return and invoke edges
- `autonomoose.py` is not yet included, since the tests don't fully pass
- `test_input.cpp` is not included, use the same tests as `main`
These and other files are also modified to use the appropriate version of the driver/validator/etc...

Currently, tests pass except test4, the solution to which would probably make this version and the existing version
under `main` eerily similar.

