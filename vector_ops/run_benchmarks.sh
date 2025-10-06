#!/bin/bash

export OPENCILK_PATH=~/opencilk-project/build
export KITSUNE_PATH=~/kitsune/build

clang++ -O2 -o icicle_vector_elementwise_benchmark icicle_vector_elementwise_benchmark.cpp -I/home/magpie/zera-icicle-benchmarks/include -I/home/magpie/icicle-install/icicle/include -L/home/magpie/icicle-install/icicle/lib -licicle_device -licicle_field_bls12_381 -licicle_curve_bls12_381 -Wl,-rpath,/home/magpie/icicle-install/icicle/lib/
python3 ../ock++.py -O2 -o zera_vector_elementwise_benchmark zera_vector_elementwise_benchmark.cpp -I/home/magpie/zera-icicle-benchmarks/include -I/home/magpie/icicle-install/icicle/include

./icicle_vector_elementwise_benchmark > output.txt
./zera_vector_elementwise_benchmark >> output.txt

python3 parse_results.py