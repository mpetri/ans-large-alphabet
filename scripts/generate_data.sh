#!/bin/bash

./build/generate_inputs.x -t -n 100000000 -o ./data/

mkdir -p ./data/speed
mkdir -p ./data/space

# create symlinks to have folders containg files for the different experiments
ln -s $(pwd)/data/uniform12.txt ./data/space/00-uni-12.txt
ln -s $(pwd)/data/geom0.9.txt ./data/space/01-geo-0.9.txt
ln -s $(pwd)/data/zipf20.txt ./data/space/04-zipf-20.txt
