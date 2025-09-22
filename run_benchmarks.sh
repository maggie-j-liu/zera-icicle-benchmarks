#!/bin/bash

export OPENCILK_PATH=~/opencilk-project/build
export KITSUNE_PATH=~/kitsune/build

clang++ -O2 -o icicle_vector_add_benchmark icicle_vector_add_benchmark.cpp -Iinclude -I/home/magpie/icicle-install/icicle/include -L/home/magpie/icicle-install/icicle/lib -licicle_device -licicle_field_bls12_381 -licicle_curve_bls12_381 -Wl,-rpath,/home/magpie/icicle-install/icicle/lib/
python3 ock++.py -O2 -o zera_vector_add_benchmark zera_vector_add_benchmark.cpp -Iinclude -I/home/magpie/icicle-install/icicle/include

./icicle_vector_add_benchmark
./zera_vector_add_benchmark