#!/bin/bash

export OPENCILK_PATH=~/opencilk-project/build
export KITSUNE_PATH=~/kitsune/build

python3 ../ock++.py --debug -std=c++20 -O2 -o zera_msm zera_msm.cpp -I/home/magpie/zera-icicle-benchmarks/include -I/home/magpie/icicle-install/icicle/include -L/home/magpie/icicle-install/icicle/lib -licicle_device -licicle_field_bls12_377 -licicle_curve_bls12_377 -Wl,-rpath,/home/magpie/icicle-install/icicle/lib/

./zera_msm