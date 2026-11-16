#!/bin/bash

function get_dependencies {
    :
}

function build_nextpnr {
    mkdir build
    pushd build
    # libstdc++ assertions turn out-of-bounds vector accesses (as in `--test` below) into failures
    cmake .. -DARCH=generic -DWERROR=on -DCMAKE_CXX_FLAGS=-D_GLIBCXX_ASSERTIONS
    make nextpnr-generic -j`nproc`
    popd
}

function run_tests {
    export PATH=${GITHUB_WORKSPACE}/.yosys/bin:$PATH
    ( export NPNR=$(pwd)/build/nextpnr-generic && cd tests/generic/flow && ./run.sh )
}

function run_archcheck {
    pushd build
    # TODO
    ./nextpnr-generic --uarch example --test
    popd
}
