FROM ubuntu:18.04

RUN apt-get update && apt-get install -y build-essential git wget cmake zlib1g-dev libssl-dev libncurses5-dev libxml2-dev libboost-all-dev && apt-get clean

WORKDIR /build-clang

RUN wget releases.llvm.org/8.0.0/llvm-8.0.0.src.tar.xz \
    && wget releases.llvm.org/8.0.0/cfe-8.0.0.src.tar.xz \
    && wget releases.llvm.org/8.0.0/clang-tools-extra-8.0.0.src.tar.xz \
    && tar -xvf llvm-8.0.0.src.tar.xz \
    && tar -xvf cfe-8.0.0.src.tar.xz \
    && mv cfe-8.0.0.src llvm-8.0.0.src/tools/clang \
    && tar -xvf clang-tools-extra-8.0.0.src.tar.xz \
    && mv clang-tools-extra-8.0.0.src llvm-8.0.0.src/tools/clang/tools/extra \
    && rm llvm-8.0.0.src.tar.xz cfe-8.0.0.src.tar.xz clang-tools-extra-8.0.0.src.tar.xz

RUN mkdir Clang-Build \
    && cd Clang-Build \
    && cmake -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_EH=ON -DLLVM_ENABLE_RTTI=ON ../llvm-8.0.0.src \
    && make \
    && make install

ENV LLVM_BUILD=/build-clang/Clang-Build \
    LLVM_PATH=/build-clang/Clang-Build \
    CLANG_VER=8.0.0

