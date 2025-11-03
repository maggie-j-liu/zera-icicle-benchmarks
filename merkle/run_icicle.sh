#!/bin/bash

export OPENCILK_PATH=~/opencilk-project/build
export KITSUNE_PATH=~/kitsune/build

clang++ -O2 -o icicle_merkle icicle_merkle.cpp -I/home/magpie/zera-icicle-benchmarks/include -I/home/magpie/icicle-install/icicle/include -L/home/magpie/icicle-install/icicle/lib -licicle_device -licicle_hash -licicle_field_bn254 -licicle_curve_bn254 -Wl,-rpath,/home/magpie/icicle-install/icicle/lib/

./icicle_merkle