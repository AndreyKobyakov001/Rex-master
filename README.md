# Rex - _The ROS Extractor_

This readme contains information about Rex, as well as setup and usage details.

## What is Rex?

Rex is a C/C++ fact extractor that uses Clang libraries to extract details from C or C++ source code to a lightweight but powerful program model. Importantly, Rex differs from [ClangEx](http://plg.uwaterloo.ca/~holt/papers/ta-intro.htm) as it is designed to capture ROS messages between components. This program model can be visualized or queried using relational algebra to discover problems in source code on an ad-hoc basis.

The model generated by Rex is encoded in the Tuple-Attribute (TA) format. This format was developed by Ric Holt at the University of Waterloo and is discussed in [this paper](http://plg.uwaterloo.ca/~holt/papers/ta-intro.htm). The TA format contains entities, their relationships, and attributes that describe these entities and relationships.

While Rex is developed to extract facts to a program model, users must download additional tools from the [Sofware Architecture Group (SWAG)](http://www.swag.uwaterloo.ca/) at the Univesity of Waterloo to visualize or query their programs. This guide will cover how these tools are configured in the "setup" section.

### Features of Rex

Currently, Rex supports the following:

- Parsing of ROS messages between publishers and subscribers. Information sent to topics is also captured.

- Both C & C++ language support including `C++11` and `C++14`.

- A more detailed program model compared to other extractors due to access to Clang's Abstract Syntax Tree (AST).

- Two modes for processing source code. A full analysis mode which analyzes ROS messages and C/C++ language components and a simple analysis mode which processes just ROS messages.

### Rex Metamodel

The following diagram higlights the information Rex extracts from a target C/C++ program:

![alt text](https://github.com/bmuscede/Rex/blob/master/RexMetamodel.png?raw=true "Rex Metamodel")

The blue boxes denote facts extracted from ROS-based software. The table presented below includes the definition of some of the current extracted relations.

| Relation                             | Description                                                                                     |
| ------------------------------------ | ----------------------------------------------------------------------------------------------- |
| call <_function1_> <_function2_>     | <_function1_> calls <_function2_>                                                               |
| write <_function_> <_variable_>      | <_function_> assigns data to <_variable_>                                                       |
| obj <_variable_> <_class_>           | <_variable_> stores an instance of <_class_> (or <_struct_>)                                    |
| contain <_entity1_> <_entity2_>      | <_entity1_> contains <_entity2_> in some form; for instance, a class contain a function         |
| parWrite <_variable1_> <_variabl2_>  | <_variable1_> is passed as parameter to <_variable2_>                                           |
| retWrite <_return1_> <_variabl2_>    | function with <_return1_> as return node is called in an assignment to <_variable2_>            |
| varWrite <_variable1_> <_variabl2_>  | <_variable1_> is used in an assignment to <_variable2_>                                         |
| varInfFunc <_variable_> <_function_> | <_variable_> is used in a condition struture that decides whether or not <_function_> is called |

Deprecated relationships are presented in the following table (see Dmitry worklog for rationale behind removing the read fact).
| Relation | Description |
| ----------- | ----------- |
| read <_function_> <_variable_> | <_function_> reads data stored in <_variable_> |

## Installation Details

### Prerequisties

Rex requires at least `Clang 8.0.0` and requires `CMake 3.0.0` or greater to run. Additionally, to build Rex, `Boost` libraries are required. For `Boost`, the computer building Rex requires `Boost` version `1.6` or greater. Additionally, other libraries are required that will be defined in their own section below. If you meet any of these prerequisites, feel free to skip their associated section below.

#### Installing CMake

First, CMake should be installed. On Linux, this is as simple as running:

```
$ sudo apt-get install cmake
```

This will install the most current version of CMake based on your sources list. Be sure to check the version to ensure you have a version of CMake greater than `3.7.0`. This can be done by running:

```
$ cmake --version
```

---

If you want the most current version of CMake, this will require a bit of additional work. First, download the most recent version from the [CMake website](http://www.cmake.org) and download the Linux source version. The commands below show how CMake is installed from the `3.7.0` Linux source version. Change the version label in the download link to download a different version.

First, we download and unzip the source files:

```
$ wget https://cmake.org/files/v3.7/cmake-3.7.0.tar.gz
$ tar xvzf cmake-3.7.0.tar.gz
$ cd cmake-3.7.0.tar.gz
```

Next, install CMake by configuring the Makefile and running it. This can be done by doing the following:

```
$ ./configure
$ make
$ make install
```

That's it! You can check if CMake is correctly installed by running:

```
$ cmake --version
```

#### Installing Clang

The best way to install Clang on your system is to download Clang directly and compile the source code directly. As stated, `Clang 8.0.0` or greater is required. This guide will cover how to install Clang from source.

First, Clang must be downloaded from the Clang and LLVM website. To do this, simply run the following:

```
$ wget releases.llvm.org/8.0.0/llvm-8.0.0.src.tar.xz
$ tar xf llvm-8.0.0.src.tar.xz
$ wget releases.llvm.org/8.0.0/cfe-8.0.0.src.tar.xz
$ tar xf cfe-8.0.0.src.tar.xz && mv cfe-8.0.0.src llvm-8.0.0.src/tools/clang
$ wget releases.llvm.org/8.0.0/clang-tools-extra-8.0.0.src.tar.xz
$ tar xf clang-tools-extra-8.0.0.src.tar.xz
$ mv clang-tools-extra-8.0.0.src llvm-8.0.0.src/tools/clang/tools/extra
```

Next, we need to build LLVM and Clang. Sit back and make a coffee during this process as this can take up to **several hours** depending on your computer. The following steps will build Clang to a directory called `Clang-Build` adjacent to the Clang install directory. To change this, simply replace the `Clang-Build` directory with any other directory and location you choose.

There are two ways to build LLVM and Clang.

**1)** Build using `make`:

This is the standard way of building. To do this, run the following:

```
$ mkdir Clang-Build
$ cd Clang-Build
$ cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_EH=ON -DLLVM_ENABLE_RTTI=ON ../llvm-8.0.0.src
$ make
$ sudo make install
```

---

**2)** Build using `ninja`:

Ninja is a lightweight build tool that promises to be faster than `make` and other build tools. To do this, run the following:

```
$ mkdir Clang-Build
$ cd Clang-Build
$ cmake -GNinja -DLLVM_ENABLE_RTTI=ON ../llvm-8.0.0.src
$ ninja
$ sudo ninja install
```

That's it! With this, Clang and LLVM will be installed. To ensure proper installation, run the following to check:

```
$ clang --version
```

Lastly, three important environment variables need to be set: `LLVM_BUILD`, `LLVM_PATH` and `CLANG_VER`. The `LLVM_BUILD` directory must point to where LLVM was build to, and `LLVM_PATH` to the same. In this tutorial it was installed to the `Clang-Build` directory. The `CLANG_VER` is the version of Clang being used. An example of how these two variables look are as follows:

```
$ export LLVM_BUILD=/home/user/Clang-Build
$ export CLANG_VER=8.0.0
$ export LLVM_PATH=/home/user/Clang-Build
```

You **must** have these set before you can build Rex.

#### Obtaining Boost Libraries

Boost libraries are also required. This process is very simple on Debian/Ubuntu systems. **Boost version 1.6 or higher is required or else Rex won't compile!**

Simply run the following command to download and install Boost libraries to your system's `include` path:

```
$ sudo apt-get install libboost-all-dev
```

**IMPORTANT NOTE:** Boost libraries are also needed on your system _even if_ you are simply running the executable built on another system. Follow the instructions above to get the necessary Boost libraries to run the portable executable.

#### Other Important Libraries

Rex requires some other libraries to run. In the future, it is expected that these libraries will be included via a build management system such as `Conan`. Until then, these libraries are required to build Rex:

- ZLib (Ubuntu zlib1g-dev)

- PThread (Ubuntu libpthread-stubs0-dev)

- DL (Ubuntu libc6-dev)

- libssl (Ubuntu libssl-dev)

- Curses (Ubuntu ncurses-dev)

- XML2 (Ubuntu libxml2-dev)

If you don't have any of these libraries, please be sure to download the **DEVELOPMENT** version of these before continuing.

### Building Rex

Now that the prerequisties are all satisfied, you can now download and build Rex! If all prerequisties are truly satisfied, Rex should build without issue. Importantly you should have the `LLVM_BUILD` and `CLANG_VER` variables set. See the Clang section if this is not the case for more information.

First, we must checkout the current version of Rex from GitHub. This will be downloaded to your current working directory. The Rex repository has all required files and libraries.

To download, run the following:

```
$ git clone https://git.uwaterloo.ca/swag/Rex.git
```

Next, we want to build the source code. This process may take several minutes due to the heavyweight size of the Clang libraries. This guide will build clang to the `Rex-Build` directory that is adjacent to the `Rex` library. If you want to build to a different directory, replace the following `Rex-Build`s to the directory of your choice.

To build, run the following command:

```
$ mkdir Rex-Build
$ cd Rex-Build
$ cmake -G "Unix Makefiles" ../Rex
$ make
```

To verify that Rex built, ensure the `Rex-Build` directory contains a `include` subdirectory and that the Rex executable exists. Additionally, run the following to check if it runs:

```
$ ./Rex
```

### Installing Additional Anaylsis Tools

There are two specific tools that are required to perform analysis on TA program models generated by Rex. Both of these tools allow for querying and visualizing Rex models. This guide will specify how to install these programs.

First, download the SWAGKit tarball from the University of Waterloo's [SWAG website](http://www.swag.uwaterloo.ca/swagkit/index.html) and unzip it to the directory of your choice:

```
$ wget http://www.swag.uwaterloo.ca/swagkit/distro/swagkit_linux86_bin_v3.03b.tar.gz
$ tar -xvzf swagkit_linux86_bin_v3.03b.tar.gz
```

Next, we want to add SWAGKit's binary path to your environment variables. To do this, simply do the following. **Note:** Replace the path to SWAGKit as shown in the following commands with the path to SWAGKit on your system!

```
$ echo "#SWAGKit Environment Variables:" >> ~/.bashrc
$ echo "export SWAGKIT_PATH=<REPLACE_WITH_SWAGKIT_PATH>" >> ~/.bashrc
$ echo "export PATH=$SWAGKIT_PATH/bin:$PATH" >> ~/.bashrc
```

That's it! You should be able to test if this worked by doing the following. The `bash` command will reload bash's environment variables and `grok` and `lsedit` are two analysis programs widely used.

```
$ bash
$ grok
$ lsedit
```

If both `Grok` and `LSEdit` started successfully, SWAGKit was configured on your computer. You are now able to run `Rex` and analyze program models!