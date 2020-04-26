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

Each experiment in the paper can be reproduced using a self-contained binary. To generate Table 9 in the paper
we run:

```
./build/table_effectiveness.x -t -i ./data/space/
```

which produces:

```
huff0  &
4.1142  &
    17.2938  &
        16.7040  \\ 
FSE  &
0.7246  &
    17.2717  &
        16.6425  \\ 
vbyte  &
8.0000  &
    15.7453  &
        15.3139  \\ 
OptPFor  &
1.1059  &
    12.2515  &
        16.6003  \\ 
shuff  &
1.1104  &
    12.1171  &
        14.5248  \\ 
arith  &
0.5182  &
    12.1559  &
        14.9079  \\ 
vbyteFSE  &
0.5182  &
    13.9537  &
        13.8778  \\ 
vbytehuff0  &
1.1115  &
    14.0030  &
        13.8888  \\ 
vbyteANS  &
0.5280  &
    13.9251  &
        13.8560  \\ 
ANS  &
0.5182  &
    12.1350  &
        15.7338  \\ 
ANSmsb  &
0.5182  &
    12.0094  &
        13.4717  \\ 
entropy  &
0.5153  &
    11.9693  &
        12.0703  \\ 
```

The latex table from the paper (requiring some additional reformatting to get the exact).


The main efficiency results table (Table 10) can be reproduced similarily:

```
./build/table_efficiency.x -t -i ./data/speed/
```

which produces:

```
\method{vbyte}  &
1636393388.9707  &  1087121953.3407  &
     159013606.7943  &   130903074.1278  \\ 

\method{vbytehuff0}  &
 394124393.5411  &   496583505.4823  &
      90783067.5054  &    95368795.9022  \\ 

\method{vbyteFSE}  &
 266113859.4759  &   342498595.7558  &
      79023959.2742  &    89971829.8201  \\ 
...
```



Code Overview
----------------

The repository contains all the code necessary to reproduce the results in the paper. The table below provides an overview of the different files and what they implement:

| File | Description |
| ---  | ---- |
| `shuff.hpp` | A version of `https://github.com/turpinandrew/shuff` which implements "On the Implementation of Minimum-Redundancy Prefix Codes", IEEE Transactions on Communications, 45(10):1200-1207, October 1997, and "Housekeeping for Prefix Coding", IEEE Transactions on Communications, 48(4):622-628, April 2000. |
| `arith.hpp` | Implementation of a 56-bit arithmetic encoder and decoder pair that carries out semi-static compression of an input array of (in the encoder) strictly positive uint32_t values, not including zero. |
| `ans_fold.hpp` | The "ans_fold" technique described in the paper |
| `ans_msb.hpp` | The "ans_fold" technique was generalized from a previous paper which was called `ans_msb` which is equivalent to `ans_fold_1` |
| `ans_int.hpp` | A large alphabet implementation of regular ANS coding. Called "ANS" in the paper |
| `ans_reorder_fold.hpp` | The "ANSfold-X-r" technique which reorders the most frequent symbols to the front of the alphabet and stores the mapping in the prelude |
| `methods.hpp` | Interfaces to all the different methods including the external library calls to the `streamvbyte`, `FiniteStateEntropy` and `FastPfor` libraries for fast `vbyte`, `huff0`, `FSE` and `OpfPFor` implementations |
| `generate_*.cpp` | Generate different datasets used in the paper |
| `interp.hpp` | A version of interpolative coding: `Alistair Moffat, Lang Stuiver: Binary Interpolative Coding for Effective Index Compression. Inf. Retr. 3(1): 25-47 (2000)` used for prelude compression. | 
| `ans_util.hpp` | Various ANS utility function shared across different ANS implementations in this repository | 
| `pseudo_adaptive.cpp` | A block based ANS coder to used to create Figure 13 in the paper |
| `ans_sint.hpp` | A version of the ANS coder in `ans_int.hpp` which supports different entropy approximation ratios used to create Figure 12 |
| `ans_smsb.hpp` | A version of the ANS coder in `ans_fold.hpp` which supports different entropy approximation ratios used to create Figure 12 |