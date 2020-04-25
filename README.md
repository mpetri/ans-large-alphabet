Large-Alphabet Semi-Static Entropy Coding Via Asymmetric Numeral Systems
======================

This repository is the code released in conjunction of with the following paper:

`
Alistair Moffat, Matthias Petri: Large-Alphabet Semi-Static Entropy Coding Via Asymmetric Numeral Systems. ACM Transactions on Information Systems
`

which implements several integer list coding schemes which utilise ANS entropy coders.

Requirements
--------

1. C++ compiler which supports C++-17
2. Boost installed

Installation
-------------

1. `git checkout https://github.com/mpetri/ans-large-alphabet.git`
2. `cd ans-large-alphabet`
3. `git submodule update --init --recursive`
2. `mkdir build`
3. `cd build`
4. `cmake ..`
5. `make -j`


Download and Generating the Data
----------------

run the following scripts to download and generate the data used in the paper:

1. `bash ./scripts/generate_data.sh` will generate the synthetic datasets into ./data/
2. `bash ./scripts/download_data.sh` will download the remaining "real world" datasets into ./data/


Running Experiments
----------------



Code Overview
----------------



Results
-------

