#!/bin/bash

export OPENCILK_PATH=~/opencilk-project/build
export KITSUNE_PATH=~/kitsune/build

python3 ../ock++.py --debug -std=c++20 -O2 -o zera_merkle_profiling zera_merkle_profiling.cpp -I/home/magpie/zera-icicle-benchmarks/include -I/home/magpie/icicle-install/icicle/include -I/usr/local/cuda-12.9/include -L/home/magpie/icicle-install/icicle/lib -licicle_device -licicle_hash -licicle_field_bn254 -licicle_curve_bn254 -Wl,-rpath,/home/magpie/icicle-install/icicle/lib/

/usr/local/cuda-12.9/bin/nsys profile -o zera_merkle_profile --force-overwrite true ./zera_merkle_profiling

/usr/local/cuda-12.9/bin/ncu -f -o zera_merkle_profile ./zera_merkle_profiling 