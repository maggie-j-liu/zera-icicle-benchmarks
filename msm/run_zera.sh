#!/bin/bash

export OPENCILK_PATH=~/opencilk-project/build
export KITSUNE_PATH=~/kitsune/build

python3 ../ock++.py -std=c++20 -O2 -o zera_msm zera_msm.cpp -I/home/magpie/zera-icicle-benchmarks/include -I/home/magpie/icicle-install/icicle/include -L/home/magpie/icicle-install/icicle/lib -licicle_device

./zera_msm