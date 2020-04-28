Large-Alphabet Semi-Static Entropy Coding Via Asymmetric Numeral Systems
======================

This repository is the code released in conjunction of with the following paper:

`
Alistair Moffat, Matthias Petri: Large-Alphabet Semi-Static Entropy Coding Via Asymmetric Numeral Systems. ACM Transactions on Information Systems, doi: 10.1145/3397175
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


1. `bash ./scripts/download_data.sh` will download the "real world" datasets into ./data/
2. `bash ./scripts/generate_data.sh` will generate the synthetic datasets into ./data/ and setup the directory structure to run all experiments


Running Experiments
----------------

Each experiment in the paper can be reproduced using a self-contained binary. To generate Table 9 in the paper
we run:

```
./build/table_effectiveness.x -t -i ./data/space/
```

which produces:

```
huff0 &
17.2864  &
    4.1148  &
        9.1648  &
            11.2015  &
                16.6952  &
                    15.8428  &
                        28.9523  \\
FSE &
17.2491  &
    0.7367  &
        8.5364  &
            10.5462  &
                16.6186  &
                    15.7553  &
                        28.9405  \\
vbyte &
15.7499  &
    8.0000  &
        8.2141  &
            10.9859  &
                15.2995  &
                    14.3414  &
                        31.6357  \\
vbyte+huff0 &
14.0040  &
    1.1125  &
        5.1454  &
            8.7429  &
                13.8829  &
                    13.0518  &
                        29.3547  \\
vbyte+FSE &
13.9387  &
    0.5234  &
        5.1147  &
            8.7600  &
                13.8549  &
                    13.0309  &
                        29.2314  \\
...
```

The latex table from the paper (requiring some additional reformatting to get the exact).


The main efficiency results table (Table 10) can be reproduced similarily:

```
./build/table_efficiency.x -t -i ./data/speed/
```

which produces:

```
\method{\vbyte}  &
	1287581427.7761  &  1275265333.6182  &
		1124443361.1807  &  1113121210.1284  &
				205508460.2778  &   212470992.1057  &
					301014107.7998  &   300215782.7958  \\

\method{\vbyte+huff0}  &
	510864371.2260  &   667794304.1286  &
		458460057.4240  &   557119911.1247  &
			138274224.6237  &   144482479.1039  &
				178295869.4397  &   184607891.3527  \\

\method{\vbyte+FSE}  &
	359587723.6133  &   444027800.3319  &
		338439584.5626  &   414147209.0230  &
			114633629.8415  &   132227626.1772  &
				142760372.5347  &   166934064.7120  \\
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
| `fold_effectiveness.cpp` | Used to generate Figure 11 which allows changing the fidelity (f) for `ans_fold` and `ans_rfold` |