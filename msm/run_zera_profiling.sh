#!/bin/bash

export OPENCILK_PATH=~/opencilk-project/build
export KITSUNE_PATH=~/kitsune/build

python3 ../ock++.py -std=c++20 -O2 -o zera_msm_profiling zera_msm_profiling.cpp -I/home/magpie/zera-icicle-benchmarks/include -I/home/magpie/icicle-install/icicle/include -L/home/magpie/icicle-install/icicle/lib -licicle_device -licicle_field_bls12_377 -licicle_curve_bls12_377 -Wl,-rpath,/home/magpie/icicle-install/icicle/lib/

/usr/local/cuda-12.9/bin/nsys profile -o zera_msm_profile --force-overwrite true ./zera_msm_profiling

/usr/local/cuda-12.9/bin/ncu -f -o zera_msm_profile ./zera_msm_profiling 