#pragma once

#ifdef RECORD_STATS
#include "stats.hpp"
#endif

#define SHUFF_LOG2_L 6
#define SHUFF_L 63

#ifdef SHUFF_MAX_SYMBOL_NUMBER
#define SHUFF_LOG2_MAX_SYMBOL SHUFF_LOG2_MAX_SYMBOL_NUMBER
#define SHUFF_MAX_SYMBOL SHUFF_MAX_SYMBOL_NUMBER
#else
#define SHUFF_LOG2_MAX_SYMBOL 27 /* must be < sizeof(ulong)*8 - SHUFF_LOG2_L   \
                                  */
#define SHUFF_MAX_SYMBOL (1 << SHUFF_LOG2_MAX_SYMBOL)
#endif

#define SHUFF_LUT_BITS 8
#define SHUFF_LUT_SIZE (1 << SHUFF_LUT_BITS)
#define SHUFF_MAX_IT 0x00ffffffffffffffULL; // ulong without top

#define SHUFF_MAX_ULONG 0xffffffffffffffffULL // maximum value for a ulong

#define SHUFF_MAX(a, b) ((a) < (b) ? (b) : (a))

const int64_t SHUFF_BUFF_BITS = sizeof(uint64_t) << 3;

/* Canonical coding arrays */
uint64_t shuff_min_code[SHUFF_L];
uint64_t shuff_lj_base[SHUFF_L];
uint64_t shuff_offset[SHUFF_L];
uint64_t* shuff_lut[SHUFF_LUT_SIZE]; /* canonical decode array */

#define SHUFF_CHECK_SYMBOL_RANGE(s)                                            \
    if (((s) > SHUFF_MAX_SYMBOL)) {                                            \
        fprintf(stderr, "Symbol %u is out of range.\n", s);                    \
        exit(-1);                                                              \
    }

void* shuff_allocate(size_t bytes)
{
    void* ptr = malloc(bytes);
    if (ptr == NULL) {
        fprintf(stderr, "malloc error!\n");
        exit(-1);
    }
    return ptr;
}

struct bit_io_t {
    size_t bytes_written = 0;
    uint8_t* init_out_u8;
    uint64_t* out_u64;
    const uint64_t* in_u64;
    uint64_t buff_btg;
};

/******************************************************************************
** Routines for outputing bits
******************************************************************************/
inline void SHUFF_START_OUTPUT(bit_io_t* bio, uint8_t* out_u8)
{
    bio->init_out_u8 = out_u8;
    bio->out_u64 = (uint64_t*)out_u8;
    bio->buff_btg = SHUFF_BUFF_BITS;
}

inline size_t SHUFF_BYTES_WRITTEN(bit_io_t* bio)
{
    size_t full_bytes = ((uint8_t*)bio->out_u64) - bio->init_out_u8;
    size_t partial = (64 - SHUFF_BUFF_BITS) / 8;
    return full_bytes + partial;
}

inline void SHUFF_OUTPUT_NEXT(bit_io_t* bio)
{
    bio->out_u64++;
}

inline void SHUFF_OUTPUT_BIT(bit_io_t* bio, int64_t b)
{
    *bio->out_u64 <<= 1;
    if (b)
        *bio->out_u64 |= 1;
    bio->buff_btg--;
    if (bio->buff_btg == 0) {
        SHUFF_OUTPUT_NEXT(bio);
        *bio->out_u64 = 0;
        bio->buff_btg = SHUFF_BUFF_BITS;
    }
}

inline void SHUFF_OUTPUT_ULONG(bit_io_t* bio, uint64_t n, char len)
{
    if (len < bio->buff_btg) {
        *bio->out_u64 <<= len;
        *bio->out_u64 |= n;
        bio->buff_btg -= len;
    } else {
        *bio->out_u64 <<= bio->buff_btg;
        *bio->out_u64 |= (n) >> (len - bio->buff_btg);
        SHUFF_OUTPUT_NEXT(bio);
        *bio->out_u64 = n;
        bio->buff_btg = SHUFF_BUFF_BITS - (len - bio->buff_btg);
    }
}

//
// Output n as a unary code 0->1 1->01 2->001 3->0001 etc
//
inline void SHUFF_OUTPUT_UNARY_CODE(bit_io_t* bio, int64_t n)
{
    for (; n > 0; n--)
        SHUFF_OUTPUT_BIT(bio, 0);
    SHUFF_OUTPUT_BIT(bio, 1);
}

/*
**
*/
inline size_t SHUFF_FINISH_OUTPUT(bit_io_t* bio)
{
    if (bio->buff_btg == SHUFF_BUFF_BITS) {
    } else {
        *bio->out_u64 <<= bio->buff_btg;
    }
    return ((uint8_t*)bio->out_u64) - bio->init_out_u8;
} // flush_output_stream()

/******************************************************************************
** Routines for inputting bits
******************************************************************************/

inline int64_t SHUFF_START_INPUT(bit_io_t* bio, const uint8_t*& f)
{
    bio->in_u64 = (const uint64_t*)f;
    bio->buff_btg = SHUFF_BUFF_BITS;
}

//
// If we are at the end then fill the buffer.
//       Set last_buff, buff and buff_btg.
// else buff++, btg=BUFF_BITS
//
inline void SHUFF_INPUT_NEXT(bit_io_t* bio)
{
    bio->in_u64++;
    bio->buff_btg = SHUFF_BUFF_BITS;
}

//
// Interpret the next len bits of the input as a ULONG and return the result
//
inline uint64_t SHUFF_INPUT_ULONG(bit_io_t* bio, int64_t len)
{
    if (len == 0)
        return 0;

    uint64_t n;

    if (bio->buff_btg == SHUFF_BUFF_BITS)
        n = (*bio->in_u64) >> (SHUFF_BUFF_BITS - len);
    else
        n = ((*bio->in_u64) << (SHUFF_BUFF_BITS - bio->buff_btg))
            >> (SHUFF_BUFF_BITS - len);

    if (len < bio->buff_btg)
        bio->buff_btg -= len;
    else {
        len -= bio->buff_btg;
        SHUFF_INPUT_NEXT(bio);
        if (len > 0) {
            n |= (*bio->in_u64) >> (SHUFF_BUFF_BITS - len);
            bio->buff_btg -= len;
        }
    }

    if (bio->buff_btg == 0)
        SHUFF_INPUT_NEXT(bio);

    return n;
}

inline uint64_t SHUFF_INPUT_BIT(bit_io_t* bio)
{
    bio->buff_btg--;
    uint64_t bit = (*bio->in_u64 >> bio->buff_btg) & 1;
    if (bio->buff_btg == 0)
        SHUFF_INPUT_NEXT(bio);
    return bit;
}

//
// Read 0 bits until a 1 bit is encountered, 1->0 01->1 001->2 0001->3
// ASSUMES: that a unary code is no longer than BUFF_BITS
//
inline uint64_t SHUFF_INPUT_UNARY_CODE(bit_io_t* bio)
{
    uint64_t n;
    n = 0;
    while (!SHUFF_INPUT_BIT(bio))
        n++;
    return n;
}

/***************************************************************************/

typedef struct stack_elem_type {
    int64_t lo, hi;
} stack;

#define SHUFF_CHECK_STACK_SIZE(n)                                              \
    if (ss < n) {                                                              \
        s = (stack*)realloc(s, sizeof(stack) * (n));                           \
        if (s == NULL) {                                                       \
            fprintf(stderr, "Out of memory for stack\n");                      \
            exit(-1);                                                          \
        }                                                                      \
        ss = n;                                                                \
    }

#define SHUFF_PUSH(l, h)                                                       \
    do {                                                                       \
        s[stack_pointer].lo = l;                                               \
        s[stack_pointer].hi = h;                                               \
        stack_pointer++;                                                       \
    } while (0)

#define SHUFF_POP(l, h)                                                        \
    do {                                                                       \
        l = s[stack_pointer - 1].lo;                                           \
        h = s[stack_pointer - 1].hi;                                           \
        stack_pointer--;                                                       \
    } while (0)

#define SHUFF_STACK_NOT_EMPTY (stack_pointer > 0)

/***************************************************************************/

inline uint64_t SHUFF_ceil_log2(uint64_t x)
{
    uint64_t _B_x = x - 1;
    uint64_t v = 0;
    for (; _B_x; _B_x >>= 1, (v)++)
        ;
    return v;
}

/*************************************************************************/

#define SHUFF_CEILLOG_2(x, v)                                                  \
    do {                                                                       \
        uint64_t _B_x = (x)-1;                                                 \
        (v) = 0;                                                               \
        for (; _B_x; _B_x >>= 1, (v)++)                                        \
            ;                                                                  \
    } while (0)

#define SHUFF_BINARY_ENCODE(bio, x, b)                                         \
    do {                                                                       \
        int64_t _B_x = (x);                                                    \
        int64_t _B_b = (b);                                                    \
        int64_t _B_nbits, _B_logofb, _B_thresh;                                \
        SHUFF_CEILLOG_2(_B_b, _B_logofb);                                      \
        _B_thresh = (1 << _B_logofb) - _B_b;                                   \
        if (--_B_x < _B_thresh)                                                \
            _B_nbits = _B_logofb - 1;                                          \
        else {                                                                 \
            _B_nbits = _B_logofb;                                              \
            _B_x += _B_thresh;                                                 \
        }                                                                      \
        SHUFF_OUTPUT_ULONG(bio, _B_x, _B_nbits);                               \
    } while (0)

#ifndef SHUFF_DECODE_ADD
#define SHUFF_DECODE_ADD(bio, b) (b) += (b) + SHUFF_INPUT_BIT(bio)
#endif

#define SHUFF_BINARY_DECODE(bio, x, b)                                         \
    do {                                                                       \
        int64_t _B_x = 0;                                                      \
        int64_t _B_b = (b);                                                    \
        int64_t _B_logofb, _B_thresh;                                          \
        if (_B_b != 1) {                                                       \
            SHUFF_CEILLOG_2(_B_b, _B_logofb);                                  \
            _B_thresh = (1 << _B_logofb) - _B_b;                               \
            _B_logofb--;                                                       \
            _B_x = SHUFF_INPUT_ULONG(bio, _B_logofb);                          \
            if (_B_x >= _B_thresh) {                                           \
                SHUFF_DECODE_ADD(bio, _B_x);                                   \
                _B_x -= _B_thresh;                                             \
            }                                                                  \
            (x) = _B_x + 1;                                                    \
        } else                                                                 \
            (x) = 1;                                                           \
    } while (0)

/***************************************************************************/

/*
** INPUT:        Array of integers to code A[0..n].
** RETURNS:      none.
** SIDE EFFECTS: Outputs bits that is interpolative encoding of A[1..n-1].
**               Sets A[0]=0 and A[n] = MAX_SYMBOL
*/
void shuff_interp_encode(bit_io_t* bio, uint64_t* A, uint64_t n)
{
    int64_t lo, hi, mid, range;
    static stack* s = NULL;
    static uint64_t ss = 0;
    uint64_t stack_pointer = 0;

    A[0] = 0;
    A[n] = SHUFF_MAX_SYMBOL;

    SHUFF_CHECK_STACK_SIZE(SHUFF_ceil_log2(n) + 1);

    SHUFF_PUSH(0, n);
    while (SHUFF_STACK_NOT_EMPTY) {
        SHUFF_POP(lo, hi);
        range = A[hi] - A[lo] - (hi - lo - 1);
        mid = lo + ((hi - lo) >> 1);
        SHUFF_BINARY_ENCODE(bio, A[mid] - (A[lo] + (mid - lo - 1)), range);
        if ((hi - mid > 1) && (A[hi] - A[mid] > (uint64_t)(hi - mid)))
            SHUFF_PUSH(mid, hi);
        if ((mid - lo > 1) && (A[mid] - A[lo] > (uint64_t)(mid - lo)))
            SHUFF_PUSH(lo, mid);
    }
}

/*
** INPUT:        A[0..n-1] ready to be filled with decoded numbers
**               A[n] must be a valid reference.
**
** SIDE EFFECTS: A[0..n-1] overwritten.
*/
void shuff_interp_decode(bit_io_t* bio, uint64_t A[], uint64_t n)
{
    int64_t lo, hi, mid, range, j;
    static stack* s = NULL;
    static uint64_t ss = 0;
    uint64_t stack_pointer = 0;

    A[0] = 0;
    A[n] = SHUFF_MAX_SYMBOL;

    SHUFF_CHECK_STACK_SIZE(SHUFF_ceil_log2(n) + 1);

    SHUFF_PUSH(0, n);
    while (SHUFF_STACK_NOT_EMPTY) {
        SHUFF_POP(lo, hi);
        range = A[hi] - A[lo] - (hi - lo - 1);
        mid = lo + ((hi - lo) >> 1);
        SHUFF_BINARY_DECODE(bio, A[mid], range);
        A[mid] += A[lo] + (mid - lo - 1);

        if (A[hi] - A[mid] == (uint64_t)(hi - mid)) // fill in the gaps of 1
            for (j = mid + 1; j < hi; j++)
                A[j] = A[j - 1] + 1;
        else if (hi - mid > 1)
            SHUFF_PUSH(mid, hi);

        if (A[mid] - A[lo] == (uint64_t)(mid - lo)) // fill in the gaps of 1
            for (j = lo + 1; j < mid; j++)
                A[j] = A[j - 1] + 1;
        else if (mid - lo > 1)
            SHUFF_PUSH(lo, mid);
    }
}

/*
** Fill freq[] with freqs
*/
uint64_t shuff_one_pass_freq_count(const uint32_t* input_u32,
    uint64_t input_size, uint64_t* freq, uint64_t* syms, uint64_t ms)
{
    uint64_t n;
    uint64_t* up;
    const uint32_t* cup;

    /* clear all elements up to max_symbol = ms */
    for (up = freq; up <= freq + ms; up++)
        *up = 0;

    n = 0;
    for (cup = input_u32; cup < input_u32 + input_size; cup++) {
        if (freq[*cup+1] == 0)
            syms[n++] = *cup+1;
        freq[*cup+1]++;
    }
    freq[0] = 1;
    syms[n++] = 0;

    return n;
}

/*
Adapted by aht to allow a level of indirection.
Tue Sep 16 09:58:43 EST 1997

        The method for calculating codelengths in function
        calculate_minimum_redundancy is described in

        @inproceedings{mk95:wads,
                author = "A. Moffat and J. Katajainen",
                title = "In-place calculation of minimum-redundancy codes",
                booktitle = "Proc. Workshop on Algorithms and Data Structures",
                address = "Kingston University, Canada",
                publisher = "LNCS 955, Springer-Verlag",
                Month = aug,
                year = 1995,
                editor = "S.G. Akl and F. Dehne and J.-R. Sack",
                pages = "393-402",
        }

        The abstract of that paper may be fetched from
        http://www.cs.mu.oz.au/~alistair/abstracts/wads95.html
        A revised version is currently being prepared.

        Written by
                Alistair Moffat, alistair@cs.mu.oz.au,
                Jyrki Katajainen, jyrki@diku.dk
        November 1996.
*/

/*** Function to calculate in-place a minimum-redundancy code
     Parameters:
        freq[0..M]    array of n symbol frequencies, unsorted
        syms[0..n-1]  array of n pointers into freq[], sorted increasing freq
        n             number of symbols
*/
void shuff_calculate_minimum_redundancy(
    uint64_t freq[], uint64_t syms[], int64_t n)
{
    int64_t root; /* next root node to be used */
    int64_t leaf; /* next leaf to be used */
    int64_t next; /* next value to be assigned */
    int64_t avbl; /* number of available nodes */
    int64_t used; /* number of internal nodes */
    uint64_t dpth; /* current depth of leaves */

    /* check for pathological cases */
    if (n == 0) {
        return;
    }
    if (n == 1) {
        freq[syms[0]] = 0;
        return;
    }

    /* first pass, left to right, setting parent pointers */
    freq[syms[0]] += freq[syms[1]];
    root = 0;
    leaf = 2;
    for (next = 1; next < n - 1; next++) {
        /* select first item for a pairing */
        if (leaf >= n || freq[syms[root]] < freq[syms[leaf]]) {
            freq[syms[next]] = freq[syms[root]];
            freq[syms[root++]] = next;
        } else
            freq[syms[next]] = freq[syms[leaf++]];

        /* add on the second item */
        if (leaf >= n || (root < next && freq[syms[root]] < freq[syms[leaf]])) {
            freq[syms[next]] += freq[syms[root]];
            freq[syms[root++]] = next;
        } else
            freq[syms[next]] += freq[syms[leaf++]];
    }

    /* second pass, right to left, setting internal depths */
    freq[syms[n - 2]] = 0;
    for (next = n - 3; next >= 0; next--)
        freq[syms[next]] = freq[syms[freq[syms[next]]]] + 1;

    /* third pass, right to left, setting leaf depths */
    avbl = 1;
    used = dpth = 0;
    root = n - 2;
    next = n - 1;
    while (avbl > 0) {
        while (root >= 0 && freq[syms[root]] == dpth) {
            used++;
            root--;
        }
        while (avbl > used) {
            freq[syms[next--]] = dpth;
            avbl--;
        }
        avbl = 2 * used;
        dpth++;
        used = 0;
    }
}

#define shuff_swapcode(parmi, parmj, n)                                        \
    {                                                                          \
        int64_t i = (n) / es;                                                  \
        uint64_t* pi = (uint64_t*)(parmi);                                     \
        uint64_t* pj = (uint64_t*)(parmj);                                     \
        do {                                                                   \
            uint64_t t;                                                        \
            t = *pi;                                                           \
            *pi++ = *pj;                                                       \
            *pj++ = t;                                                         \
        } while (--i > 0);                                                     \
    }

#define shuff_swap(a, b) shuff_swapcode(a, b, es)
#define vecshuff_swap(a, b, n)                                                 \
    if ((n) > 0)                                                               \
    shuff_swapcode(a, b, n)

int64_t shuff_cmp(uint64_t* a, uint64_t* b, uint64_t freq[])
{
    return freq[*a] - freq[*b];
}

uint64_t* shuff_med3(uint64_t* a, uint64_t* b, uint64_t* c, uint64_t freq[])
{
    return shuff_cmp(a, b, freq) < 0
        ? (shuff_cmp(b, c, freq) < 0 ? b : (shuff_cmp(a, c, freq) < 0 ? c : a))
        : (shuff_cmp(b, c, freq) > 0 ? b : (shuff_cmp(a, c, freq) < 0 ? a : c));
}

#define shuff_min(a, b) (a) < (b) ? a : b

/*
** Sort sums using freq[syms[i]] as the key for syms[i]
*/
void shuff_indirect_sort(
    uint64_t* freq, uint64_t* syms, uint64_t* a, uint64_t n)
{
    uint64_t *pa, *pb, *pc, *pd, *pl, *pm, *pn;
    int64_t d, r;
    const int64_t es = 1;

    if (n < 7) {
        for (pm = a + es; pm < a + n * es; pm += es)
            for (pl = pm; pl > a && shuff_cmp(pl - es, pl, freq) > 0; pl -= es)
                shuff_swap(pl, pl - es);
        return;
    }
    pm = a + (n / 2) * es;
    if (n > 7) {
        pl = a;
        pn = a + (n - 1) * es;
        if (n > 40) {
            d = (n / 8) * es;
            pl = shuff_med3(pl, pl + d, pl + 2 * d, freq);
            pm = shuff_med3(pm - d, pm, pm + d, freq);
            pn = shuff_med3(pn - 2 * d, pn - d, pn, freq);
        }
        pm = shuff_med3(pl, pm, pn, freq);
    }
    shuff_swap(a, pm);
    pa = pb = a + es;

    pc = pd = a + (n - 1) * es;
    for (;;) {
        while (pb <= pc && (r = shuff_cmp(pb, a, freq)) <= 0) {
            if (r == 0) {
                shuff_swap(pa, pb);
                pa += es;
            }
            pb += es;
        }
        while (pb <= pc && (r = shuff_cmp(pc, a, freq)) >= 0) {
            if (r == 0) {
                shuff_swap(pc, pd);
                pd -= es;
            }
            pc -= es;
        }
        if (pb > pc)
            break;
        shuff_swap(pb, pc);
        pb += es;
        pc -= es;
    }
    pn = a + n * es;
    r = shuff_min(pa - a, pb - pa);
    vecshuff_swap(a, pb - r, r);
    r = shuff_min(pd - pc, pn - pd - es);
    vecshuff_swap(pb, pn - r, r);
    if ((r = pb - pa) > es)
        shuff_indirect_sort(freq, syms, a, r / es);
    if ((r = pd - pc) > es)
        shuff_indirect_sort(freq, syms, pn - r, r / es);
}

/*
** Build lj_base[] and offset from the codelens in A[0..n-1]
** A[] need not be sorted.
**
** Return cw_lens[] a freq count of codeword lengths.
*/
void shuff_build_canonical_arrays(uint64_t* cw_lens, uint64_t max_cw_length)
{
    uint64_t* q;
    uint64_t* p;

    // build offset
    q = shuff_offset;
    *q = 0;
    for (p = cw_lens + 1; p < cw_lens + max_cw_length; p++, q++)
        *(q + 1) = *q + *p;

    // generate the min_code array
    // min_code[i] = (min_code[i+1] + cw_lens[i+2]) >>1
    q = shuff_min_code + max_cw_length - 1;
    *q = 0;
    for (q--, p = cw_lens + max_cw_length; q >= shuff_min_code; q--, p--)
        *q = (*(q + 1) + *p) >> 1;

    // generate the lj_base array
    q = shuff_lj_base;
    uint64_t* pp = shuff_min_code;
    int64_t left_shift = (sizeof(uint64_t) << 3) - 1;
    for (p = cw_lens + 1; q < shuff_lj_base + max_cw_length;
         p++, q++, pp++, left_shift--)
        if (*p == 0)
            *q = *(q - 1);
        else
            *q = (*pp) << left_shift;
    for (p = cw_lens + 1, q = shuff_lj_base; *p == 0; p++, q++)
        *q = SHUFF_MAX_ULONG;

}

/*
** INPUT: syms[0..n-1] lists symbol numbers
**        freq[i] contains the codeword length of symbol i
**        cw_lens[1..max_cw_length] is the number of codewords of length i
**
** OUTPUT: None
**
** SIDE EFFECTS: syms[0..max_symbol] is overwritten with canonical code mapping.
**               cw_lens[] is destroyed.
*/
void shuff_generate_mapping(uint64_t* cw_lens, uint64_t* syms,
    uint64_t* freq, uint64_t max_cw_length, uint64_t n)
{
    int64_t i;

    for (i = 1; i <= (int)max_cw_length; i++)
        cw_lens[i] += cw_lens[i - 1];

    for (i = n - 1; i >= 0; i--) {
        syms[syms[i]] = cw_lens[freq[syms[i]] - 1]++;
    }
}

int shuff_pcmp(const void* a, const void* b)
{
    return *((uint64_t*)a) - *((uint64_t*)b);
}

void shuff_build_codes(
    bit_io_t* bio, uint64_t* syms, uint64_t* freq, uint64_t n)
{
    uint64_t i;
    const uint64_t* p;
    uint64_t max_codeword_length; //, min_codeword_length;
    uint64_t cw_lens[SHUFF_L + 1];

    shuff_indirect_sort(freq, syms, syms, n);

    shuff_calculate_minimum_redundancy(freq, syms, n);

    // calculcate max_codeword_length and set cw_lens[]
    for (i = 0; i <= SHUFF_L; i++)
        cw_lens[i] = 0;
    // min_codeword_length = max_codeword_length = freq[syms[0]];
    max_codeword_length = 0;
    for (p = syms; p < syms + n; p++) {
        if (freq[*p] > max_codeword_length)
            max_codeword_length = freq[*p];
        cw_lens[freq[*p]]++;
    }

    shuff_build_canonical_arrays(cw_lens, max_codeword_length);

    SHUFF_OUTPUT_ULONG(bio, n, SHUFF_LOG2_MAX_SYMBOL);
    SHUFF_OUTPUT_ULONG(bio, max_codeword_length, SHUFF_LOG2_L);

    qsort(syms, n, sizeof(uint64_t), shuff_pcmp);

    for (p = syms; p < syms + n; p++) {
        SHUFF_OUTPUT_UNARY_CODE(bio, max_codeword_length - freq[*p]);
    }

    shuff_interp_encode(bio, syms, n);

    shuff_generate_mapping(cw_lens, syms, freq, max_codeword_length, n);
}

/*
** Canonical encode.  cwlens[] contains codeword lens, mapping[] contains
** ordinal symbol mapping.
*/
inline uint64_t shuff_output(
    bit_io_t* bio, uint64_t i, uint64_t* mapping, uint64_t* cwlens)
{
    uint64_t sym_num = mapping[i]; // ordinal symbol number
    uint64_t len = cwlens[i];
    uint64_t cw = shuff_min_code[len - 1] + (sym_num - shuff_offset[len - 1]);
    SHUFF_OUTPUT_ULONG(bio, cw, len);
    return len;
}

/*
** count the freqs, build the codes, write the codes, "...and I am spent."
*/

inline size_t shuff_compress(uint8_t* out_u8, size_t out_size_u8,
    const uint32_t* input_u32, size_t input_size)
{
#ifdef RECORD_STATS
    auto start_compress = std::chrono::high_resolution_clock::now();
#endif

    bit_io_t bio;
    SHUFF_START_OUTPUT(&bio, out_u8);

    const uint32_t* up;
    uint64_t n;
    uint64_t max_symbol, temp;

    uint64_t* freqs
        = (uint64_t*)shuff_allocate(sizeof(uint64_t) * SHUFF_MAX_SYMBOL + 2);
    uint64_t* syms
        = (uint64_t*)shuff_allocate(sizeof(uint64_t) * SHUFF_MAX_SYMBOL + 2);

    /* find max_symbol and check range*/
    max_symbol = 0;
    for (up = input_u32; up < input_u32 + input_size; up++) {
        if (*up+1 > max_symbol)
            max_symbol = *up+1;
        SHUFF_CHECK_SYMBOL_RANGE(*up+1);
    }

    n = shuff_one_pass_freq_count(
        input_u32, input_size, freqs, syms, max_symbol);

    shuff_build_codes(&bio, syms, freqs, n);


#ifdef RECORD_STATS
    auto stop_prelude = std::chrono::high_resolution_clock::now();
    get_stats().prelude_bytes = SHUFF_BYTES_WRITTEN(&bio);
    get_stats().prelude_time_ns = (stop_prelude - start_compress).count();
#endif

    for (up = input_u32; up < input_u32 + input_size; up++)
        shuff_output(&bio, *up+1, syms, freqs);

    free(freqs);
    free(syms);

#ifdef RECORD_STATS
    auto stop_compress = std::chrono::high_resolution_clock::now();
    get_stats().encode_bytes = SHUFF_BYTES_WRITTEN(&bio) - get_stats().prelude_bytes;
    get_stats().encode_time_ns = (stop_compress - stop_prelude).count();
#endif

    return SHUFF_FINISH_OUTPUT(&bio);
}

void shuff_build_lut(uint64_t max_cw_len)
{
    uint64_t max, min; // range of left justified "i"
    int64_t i, j = max_cw_len - 1; // pointer into lj

    for (i = 0; i < SHUFF_LUT_SIZE; i++) {
        min = i << ((sizeof(uint64_t) << 3) - SHUFF_LUT_BITS);
        max = min | SHUFF_MAX_IT;

        while ((j >= 0) && (max > shuff_lj_base[j]))
            j--;

        // we know max is in range of lj[j], so check min
        if (min >= shuff_lj_base[j + 1])
            shuff_lut[i] = shuff_lj_base + j + 1;
        else
            shuff_lut[i] = NULL; //-(j+1);
    }
}

void shuff_decompress(
    uint32_t* out_u32, size_t to_decode, const uint8_t* in_u8, size_t cSrcSize)
{
    bit_io_t bio;
    SHUFF_START_INPUT(&bio, in_u8);
    uint64_t* mapping
        = (uint64_t*)shuff_allocate(sizeof(uint64_t) * SHUFF_MAX_SYMBOL);
    uint64_t cw_lens[SHUFF_L + 1];

    uint64_t n = SHUFF_INPUT_ULONG(&bio, SHUFF_LOG2_MAX_SYMBOL);
    int64_t* lens = (int64_t*)shuff_allocate(sizeof(int64_t) * n + 1);

    uint64_t max_cw_len = SHUFF_INPUT_ULONG(&bio, SHUFF_LOG2_L);

    for (int i = 0; i <= (int)max_cw_len; i++)
        cw_lens[i] = 0;
    for (int64_t* p = lens; p < lens + n; p++) {
        *p = max_cw_len - SHUFF_INPUT_UNARY_CODE(&bio);
        cw_lens[*p]++;
    }

    uint64_t min_cw_len = 0;
    for (min_cw_len = 0; cw_lens[min_cw_len] == 0; min_cw_len++)
        ;

    shuff_build_canonical_arrays(cw_lens, max_cw_len);

    shuff_interp_decode(&bio, mapping, n);

    for (int i = 1; i <= (int64_t)max_cw_len; i++)
        cw_lens[i] += cw_lens[i - 1];

    for (int64_t* p = lens + n - 1; p >= lens; p--)
        *p = cw_lens[*p - 1]++;

    uint64_t t, from, S;
    int64_t start = 0;
    lens[n] = 1; // sentinel
    while (start < n) {
        from = start;
        S = mapping[start];

        while (lens[from] >= 0) {
            int i = lens[from];
            lens[from] = -1;
            t = mapping[i];
            mapping[i] = S;
            S = t;
            from = i;
        }

        while (lens[start] == -1)
            start++; // find next start (if any)
    }
    free(lens);

    shuff_build_lut(max_cw_len);

    uint64_t code = 0;
    uint64_t bits_needed = sizeof(uint64_t) << 3;
    uint64_t currcode;
    uint64_t currlen = sizeof(uint64_t) << 3;
    uint64_t* lj;
    uint64_t* start_linear_search
        = shuff_lj_base + SHUFF_MAX(SHUFF_LUT_BITS, min_cw_len) - 1;

    while (to_decode != 0) {
        code |= SHUFF_INPUT_ULONG(&bio, bits_needed);

        lj = shuff_lut[code >> ((sizeof(uint64_t) << 3) - SHUFF_LUT_BITS)];
        if (lj == NULL)
            for (lj = start_linear_search; code < *lj; lj++)
                ;
        currlen = lj - shuff_lj_base + 1;

        // calculate symbol number
        currcode = code >> ((sizeof(uint64_t) << 3) - currlen);
        currcode -= shuff_min_code[currlen - 1];
        currcode += shuff_offset[currlen - 1];

        // subtract the one added in encoding
        *out_u32++ = mapping[currcode]-1; // we add 1 to everything we encode

        code <<= currlen;
        bits_needed = currlen;
        to_decode--;
    }

    free(mapping);
}
