#!/bin/bash

# (1) download europarl
cd ./data/
wget http://www.statmt.org/wmt14/training-monolingual-europarl-v7/europarl-v7.en.gz
gzip -d europarl-v7.en.gz
cd ..

./build/generate_inputs.x -t -n 100000 -o ./data/


    desc.add_options()
        ("help,h", "produce help message")
        ("num,n",po::value<size_t>()->required(), "number of integers per file")
        ("input,i",po::value<std::string>()->required(), "input file")
        ("text,t", "text instead of uint32_t output")
        ("word,w", "word instead of bytes")
        ("output,o",po::value<std::string>()->required(), "output prefix");