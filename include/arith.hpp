// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
// 
//   http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

/* Current version as of 26 July 2019:

   Implementation of a 56-bit arithmetic encoder and decoder pair that
   carries out semi-static compression of an input array of (in the
   encoder) strictly positive uint32_t values, not including zero.
   Encoding process includes generation of a (very crude) byte-interp-coded
   prelude describing the set of symbol frequencies found in the source
   array. Decoder takes as input an array of uint8_t bytes representing a
   coded message (including embedded prelude), and regenerates original
   array supplied to encoder.

   Encoder and decoder assume that arrays are sufficiently-sized that
   encoded input and respectively decoded output can written written
   without overflow or resizing being required.  They do test the supplied
   limits and will cause program exit on array overrun, but only if the
   //myassert() macro is permitted to be active.

   Written by Alistair Moffat, July 2019, with special thanks to EK407,
   EK73, EK74, and EK408 :-)

*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


#define printint(msg, val) \
    fprintf(stderr, "%-18s = %10d\n", msg, (int)(val))
#define printdbl(msg, val) \
    fprintf(stderr, "%-18s = %10.3f\n", msg, (double)(val))

#define DU64(x) \
    printf("  line %3d: %5s = %016llx\n", __LINE__, #x, x)
#define DU32(x) \
    printf("  line %3d: %5s =         %08lx\n", __LINE__, #x, x)
#define DU08(x) \
    printf("  line %3d: %5s =               %02x\n", __LINE__, #x, x)
#define DI64(x) \
    printf("  line %3d: %5s =         %8ld\n", __LINE__, #x, x)
#define DI32(x) \
    printf("  line %3d: %5s =         %8d\n", __LINE__, #x, x)

size_t
byte_encode(uint8_t* obuff, size_t osize,
    uint64_t v, uint64_t max)
{
    int op = 0;
    max--; /* working here with zero-origin values */
    v--;
    while (max) {
        //myassert(op < osize);
        obuff[op++] = v & 255;
        v >>= 8;
        max >>= 8;
    }
    return op;
}

size_t
byte_decode(const uint8_t* ibuff, size_t isize,
    uint64_t max, uint64_t* v)
{
    size_t ip = 0;
    uint64_t newv = 0;
    int offset = 0;
    max--; /* working here with zero-origin values */
    while (max) {
        //myassert(ip < isize);
        newv += (((uint64_t)ibuff[ip++]) << offset);
        max >>= 8;
        offset += 8;
    }
    *v = newv + 1;
    return ip;
}

/* use the byte-aligned interp mechanism to code the entire integer buffer
*/
size_t interp_compress(
    uint8_t* obuff, /* output buffer */
    size_t osize, /* output buffer size */
    const uint32_t* ibuff, /* input buffer */
    size_t isize)
{ /* input buffer size */

    int i, p;
    uint64_t vp, vl, vr, v;
    size_t op = 0;

    std::vector<uint64_t> T(2 * isize - 1);

    for (i = 0; i < isize; i++) {
        T[i + isize - 1] = ibuff[i];
    }
    for (p = isize - 2; p >= 0; p--) {
        T[p] = T[2 * p + 1] + T[2 * p + 2] - 1;
    }
    op += byte_encode(obuff, osize - op, T[0], (1LL << 63));
    for (p = 0; p < isize - 1; p++) {
        vp = T[p];
        vl = T[2 * p + 1];
        vr = T[2 * p + 2];
        if (vl <= vr) {
            v = vr - vl + 1;
        } else {
            v = vl - vr;
        }
        op += byte_encode(obuff + op, osize - op, v, vp);
    }
    return op;
}

/* decode an entire buffer of byte codes to regenerate the initial
   sequence of 32-bit integers that was the original input
*/
size_t interp_decompress(
    uint32_t* obuff, /* output buffer */
    size_t osize, /* number symbols to be decoded */
    const uint8_t* ibuff, /* input buffer */
    size_t isize)
{ /* size of input buffer */
    int i, p;
    uint64_t vp, vl, vr, v;
    size_t ip = 0;

    std::vector<uint64_t> T(2 * osize - 1);
    ip += byte_decode(ibuff + ip, isize - ip, (1LL << 63), T.data());
    for (p = 0; p < osize - 1; p++) {
        vp = T[p];
        ip += byte_decode(ibuff + ip, isize - ip, vp, &v);
        if (((v + vp) & 1) == 0) {
            vl = 1 + ((vp - v) >> 1);
        } else {
            vl = (vp + v + 1) >> 1;
        }
        vr = vp - vl + 1;
        T[2 * p + 1] = vl;
        T[2 * p + 2] = vr;
    }
    for (i = 0; i < osize; i++) {
        obuff[i] = T[i + osize - 1];
    }
    return ip;
}

#define BBYTES 7
/* less than eight */
#define BBITS (BBYTES * 8)
/* multiple of 8, strictly less than 64 */
#define FULL ((1LL << BBITS) - 1)
#define FULLBYTE 255
#define PART ((1LL << (BBITS - 8)))
#define ZERO (0)
#define MINR ((1LL<<(BBITS-15)))
#define TBTS 31 /* at this stage, cannot be larger than 31 because of F */
#define PREL_RECURSE 1000

/* count through the input array, building a resizing array containing
   symbol frequency counts, including zero for symbols that don't appear
*/
std::vector<uint32_t> count_freqs(const uint32_t* ibuff, size_t isize, size_t* maxv)
{
    uint32_t newmax = 0;
    std::vector<uint32_t> F(1024,0);
    for (size_t i = 0; i < isize; i++) {
        uint32_t v = ibuff[i] + 1;
        if (v >= F.size()) {
            auto cur_size = F.size();
            F.resize(v+1);
            for(size_t j=cur_size;j<F.size();j++) F[j] = 0;
        }
        newmax = std::max(newmax,v);
        F[v]++;
    }
    *maxv = newmax;
	size_t total = 0;
	for (uint32_t i=1; i<=newmax; i++) {
		total += F[i];
	}

	/* now downsample the frequency array if required, to ensure that
	   total size is less than 2^28
	*/
	while (total > MINR) {
		total = 0;
		for (uint32_t i=1; i<=newmax; i++) {
			F[i] = (F[i]+1)>>1;
			total += F[i];
		}
	}
    /* return the array that got constructed */
    return F;
}

/* pick a suitable power of two, and adjust counts so that their total is a
   power of two, to avoid the division in the encoder and one of the two
   divisions in the decoder
*/
uint64_t
scale_counts(std::vector<uint32_t>& F, size_t maxv, int silent) {
	size_t power;
	double ratio;
	size_t maxsym;
	uint64_t total;
	int i;
	power = TBTS;
	total = 0;
	for (i=1; i<=maxv; i++) {
		/* shift from 1-origin back to 0-origin */
		F[i]--;
		total += F[i];
	}
	ratio = ((double)(1LL<<power))/total;

	/* do the adjustment, tracking the new total and also locating the
	   most probable symbol */
	maxsym = 1;
	total = 0;
	for (i=1; i<=maxv; i++) {
		/* these are still offset by 1 after the interp step */
		F[i] = F[i]*ratio;
		total += F[i];
		if (F[i]>F[maxsym]) {
			maxsym = i;
		}
	}
	/* allocate the leftover remainder as a bonus to the MFS */
	F[maxsym] += (1LL<<power)-total;
	return (1LL<<power);
}

/* encode a supplied array of ints to an output array of bytes
*/
size_t arith_compress(
    uint8_t* obuff, /* output buffer */
    size_t osize, /* output buffer size */
    const uint32_t* ibuff, /* input buffer */
    size_t isize)
{ /* input buffer size */

    /* returns the number of bytes written to the output buffer,
	   fails/exits if output buffer is too small, provided that
	   //myassertion() in enabled */

    /* state variables for encoding */
    uint64_t L = ZERO;
    uint64_t R = FULL;
    uint64_t low, high, total, scale;
    uint32_t v;
    uint8_t last_non_ff_byte = 0, byte;
    uint32_t num_ff_bytes = 0;

    int first = 1;
    size_t op = 0;
    size_t maxv, nunq=0;

    /* count the frequencies of the provided symbols */
    auto F = count_freqs(ibuff, isize, &maxv);

    /* adjust the counts to allow the interpolative encoder to work */
    for (size_t i = 0; i <= maxv; i++) {
        nunq += (F[i]>0);
        F[i]++;
    }

    /* now code the prelude into the output buffer */
    op += byte_encode(obuff + op, osize - op, maxv, (1LL << 31));
    op += byte_encode(obuff+op, osize-op, nunq, (1LL<<31));

	if (nunq<PREL_RECURSE) {
		/* small enough that inefficient approach is ok */
		op += interp_compress(obuff+op, osize-op, F.data(), maxv+1);
	} else {
		/* or else, ta-daa, recursive call for prelude... */
		op += arith_compress(obuff+op, osize-op, F.data(), maxv+1);
	}


	/* and now scale them up to get total to be a power of two */
	total = scale_counts(F, maxv, 0);

    /* bit ugly to have this next output line coming from here,
	   but will do for now */
    /* and turn the adjusted counts in F into cumulative array */
    F[0] = 0;
    for (size_t i = 1; i <= maxv; i++) {
        F[i] = F[i - 1] + F[i];
    }

    /* now encode the array of symbols using the fixed array F to
	   control probability estimation, one at a time */
    for (size_t i = 0; i < isize; i++) {
        v = ibuff[i] + 1;

        /* allocated probability range for this symbol */
        low = F[v - 1];
        high = F[v];

        /* this is the actual arithmetic coding step */
        scale = R>>TBTS;
        L += low * scale;
        if (high < total) {
            /* top symbol gets beneit of rounding gaps */
            R = (high - low) * scale;
        } else {
            R = R - low * scale;
        }

        /* now sort out the carry/renormalization process */
        if (L > FULL) {
            /* lower bound has overflowed, need first to push
			   a carry through the ff bytes and into the pending
			   non-ff byte */
            last_non_ff_byte += 1;
            L &= FULL;
            while (num_ff_bytes > 0) {
                obuff[op++] = last_non_ff_byte;
                num_ff_bytes--;
                last_non_ff_byte = ZERO;
            }
        }

        /* more normal type of renorm step */
        while (R < PART) {
            /* can output (or rather, save for later output)
			   a byte from the front of L */
            byte = (L >> (BBITS - 8));
            if (byte != FULLBYTE) {
                /* not ff, so can bring everything up to date */
                if (!first) {
                    obuff[op++] = last_non_ff_byte;
                }
                while (num_ff_bytes) {
                    obuff[op++] = FULLBYTE;
                    num_ff_bytes--;
                }
                last_non_ff_byte = byte;
                first = 0;
            } else {
                /* ff bytes just get counted */
                num_ff_bytes++;
            }
            L <<= 8;
            L &= FULL;
            R <<= 8;
        }
    }

    /* wind up: first flush all of the pending bytes */
    if (!first) {
        obuff[op++] = last_non_ff_byte;
    }
    while (num_ff_bytes) {
        obuff[op++] = FULLBYTE;
        num_ff_bytes--;
    }

    /* then send the final bytes from L, to be sure to be sure */
    for (int i = BBYTES - 1; i >= 0; i--) {
        obuff[op++] = (L >> ((8 * i))) & FULLBYTE;
    }

    /* tidy up, and done */
    return op;
}

/* decode the supplied array of bytes and regenerate the original array
   of strictly positive integers
*/
size_t arith_decompress(
    uint32_t* obuff, /* output buffer */
    size_t osize, /* number symbols to be decoded */
    const uint8_t* ibuff, /* input buffer */
    size_t isize)
{ /* size of input buffer */

    int i;
    size_t op, ip = 0;

    /* state variables for decoding */
    uint64_t R = FULL;
    uint64_t D;
    uint64_t low, high, total, scale;
    uint64_t target;
    uint64_t v = 0;
    size_t maxv;
    size_t nunq;

    /* fetch the prelude */
    ip += byte_decode(ibuff + ip, isize - ip, (1LL << 31), &target);
    maxv = target;
    ip += byte_decode(ibuff+ip, isize-ip, (1LL<<31), &target);
    nunq = target;

    std::vector<uint32_t> F(maxv+1,0);

	if (nunq<PREL_RECURSE) {
		/* small enough that inefficient approach is ok */
		ip += interp_decompress(F.data(), maxv+1, ibuff+ip, isize-ip);
	} else {
		/* or else, tadaa, recursive call for prelude... */
		ip += arith_decompress(F.data(), maxv+1, ibuff+ip, isize-ip);
	}


	/* adjust and scale the counts like the encoder did */
	total = scale_counts(F, maxv, 1);

    /* convert to cumulative sums */
    F[0] = 0;
    for (i = 1; i <= maxv; i++) {
        F[i] = F[i - 1] + F[i];
    }

    /* load up D */
    D = 0;
    for (i = 0; i < BBYTES; i++) {
        D <<= 8;
        D += ibuff[ip++];
    }

    /* and decode the required symbols one by one */
    for (op = 0; op < osize; op++) {
        /* decode target */
        scale = R>>TBTS;
        target = D / scale;

        /* beware of the rounding that might accrue at the top of the
		   range, and adjust downward if required */
        if (target>=total) target = total-1;
        /* binary search in F for target is a little faster */
        {
            int lo = 0, hi = maxv;
            /* elements F[lo..hi] inclusive being considered */
            while (lo < hi) {
                v = (1 + lo + hi) >> 1;
                if (F[v - 1] > target) {
                    hi = v;
                } else if (target >= F[v]) {
                    lo = v;
                } else {
                    break;
                }
            }
        }

        /* (could also take leading bits of target to index a table
		   and accelerate the search, leave that for another day)
		*/
        obuff[op] = v - 1;

        /* adjust, tracing the encoder, with D=V-L throughout */
        low = F[v - 1];
        high = F[v];
        D -= low * scale;
        if (high < total) {
            R = (high - low) * scale;
        } else {
            R = R - low * scale;
        }

        while (R < PART) {
            if(ip >= isize) {
                std::cerr << "I/O error" << std::endl;
                exit(EXIT_FAILURE);
            }
            /* range has shrunk, time to bring in another byte */
            R <<= 8;
            D <<= 8;
            D &= FULL;
            D += ibuff[ip++];
        }
    }
    return ip;
}
