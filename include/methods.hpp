#pragma once

#include "compositecodec.h"
#include "optpfor.h"
#include "variablebyte.h"
#include "streamvbyte.h"
#include "huf.h"
#include "fse.h"
#include "shuff.hpp"
#include "arith.hpp"

#include "ans_byte.hpp"
#include "ans_int.hpp"
#include "ans_msb.hpp"
#include "ans_fold.hpp"
#include "ans_reorder_fold.hpp"

#include "ans_sint.hpp"
#include "ans_smsb.hpp"

struct vbyte {
    static std::string name() { return "vbyte"; }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        static FastPForLib::VariableByte vb;
        auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
        size_t encoded_u32 = out_size_u8 / sizeof(uint32_t);
        vb.encodeArray(in_ptr,in_size_u32, out_ptr_u32, encoded_u32);
        return encoded_u32 * sizeof(uint32_t);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        static FastPForLib::VariableByte vb;
        size_t encoded_u32 = in_size_u8 / sizeof(uint32_t);
        auto in_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
        size_t out_len = out_size_u32;
        vb.decodeArray(in_u32, encoded_u32,out_ptr,out_len);
    }
};

template <uint32_t t_block_size = 128> struct optpfor {
    static_assert(
        t_block_size % 32 == 0, "op4 blocksize must be multiple of 32");
    static std::string name() { return "OptPFor"; }
    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        using op4_codec = FastPForLib::OPTPFor<t_block_size / 32>;
        using vb_codec = FastPForLib::VariableByte;
        static FastPForLib::CompositeCodec<op4_codec, vb_codec> op4c;
        auto out_u32 = reinterpret_cast<uint32_t*>(out_ptr);
        size_t encoded_u32 = out_size_u8 / sizeof(uint32_t);
        op4c.encodeArray(in_ptr,in_size_u32, out_u32, encoded_u32);
        return encoded_u32 * sizeof(uint32_t);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        using op4_codec = FastPForLib::OPTPFor<t_block_size / 32>;
        using vb_codec = FastPForLib::VariableByte;
        static FastPForLib::CompositeCodec<op4_codec, vb_codec> op4c;
        size_t encoded_u32 = in_size_u8 / sizeof(uint32_t);
        auto in_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
        size_t out_len = out_size_u32;
        op4c.decodeArray(in_u32, encoded_u32,out_ptr,out_len);
    }
};

struct streamvbyte {
    static std::string name() { return "streamvbyte"; }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        return streamvbyte_encode(in_ptr,in_size_u32,out_ptr);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        streamvbyte_decode(in_ptr,out_ptr,out_size_u32);
    }
};

struct huffzero {
    static std::string name() { return "huff0"; }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        auto in_u8 = reinterpret_cast<const uint8_t*>(in_ptr);
        auto in_size = in_size_u32 * sizeof(uint32_t);
        auto num_blocks = in_size / HUF_BLOCKSIZE_MAX;
        auto last_size = in_size % HUF_BLOCKSIZE_MAX;
        auto init_ptr = out_ptr;
        auto dst_capacity = out_size_u8;
        for(size_t i=0;i<num_blocks;i++) {
            auto compressed_size_u8 = HUF_compress(out_ptr+4,dst_capacity,in_u8,HUF_BLOCKSIZE_MAX);
            auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
            *out_ptr_u32 = compressed_size_u8;
            out_ptr += compressed_size_u8+4;
            dst_capacity -= (compressed_size_u8+4);
            in_u8 += HUF_BLOCKSIZE_MAX;
        }
        if(last_size) {
            auto compressed_size_u8 = HUF_compress(out_ptr+4,dst_capacity,in_u8,last_size);
            auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
            *out_ptr_u32 = compressed_size_u8;
            out_ptr += compressed_size_u8+4;
        }
        size_t bytes_written = out_ptr - init_ptr;
        return bytes_written;
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        auto out_ptr_u8 = reinterpret_cast<uint8_t*>(out_ptr);
        auto out_size = out_size_u32 * sizeof(uint32_t);
        auto num_blocks = out_size / HUF_BLOCKSIZE_MAX;
        auto last_size = out_size % HUF_BLOCKSIZE_MAX;
        auto dst_capacity = out_size_u32;
        for(size_t i=0;i<num_blocks;i++) {
            auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
            size_t compressed_size = *in_ptr_u32;
            HUF_decompress(out_ptr_u8,HUF_BLOCKSIZE_MAX,in_ptr+4,compressed_size);
            in_ptr += (compressed_size+4);
            out_ptr_u8 += HUF_BLOCKSIZE_MAX;
        }
        if(last_size) {
            auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
            size_t compressed_size = *in_ptr_u32;
            HUF_decompress(out_ptr_u8,last_size,in_ptr+4,compressed_size);
        }
    }
};

struct fse {
    static std::string name() { return "FSE"; }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        auto in_u8 = reinterpret_cast<const uint8_t*>(in_ptr);
        auto in_size = in_size_u32 * sizeof(uint32_t);
        return FSE_compress(out_ptr,out_size_u8,in_u8,in_size);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        auto out_ptr_u8 = reinterpret_cast<uint8_t*>(out_ptr);
        auto out_size = out_size_u32 * sizeof(uint32_t);
        FSE_decompress(out_ptr_u8,out_size,in_ptr,in_size_u8);
    }
};

struct vbytefse {
    static std::string name() { return "vbyteFSE"; }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        size_t vbyte_bytes = vbyte::encode(in_ptr,in_size_u32,buf,out_size_u8);
        auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
        *out_ptr_u32 = vbyte_bytes;
        auto stored_bytes = FSE_compress(out_ptr+sizeof(uint32_t),out_size_u8-sizeof(uint32_t),buf,vbyte_bytes);
        if(stored_bytes == 0) {
            *out_ptr_u32 = 0;
            memcpy(out_ptr+sizeof(uint32_t),buf,vbyte_bytes);
            return vbyte_bytes+sizeof(uint32_t);
        } else {
            return stored_bytes+sizeof(uint32_t);
        }
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
        size_t vbyte_bytes = *in_ptr_u32;
        in_ptr += +sizeof(uint32_t);
        if(vbyte_bytes == 0) {
            vbyte::decode(in_ptr,in_size_u8-sizeof(uint32_t),out_ptr,out_size_u32);
        } else {
            FSE_decompress(buf,vbyte_bytes,in_ptr,in_size_u8-sizeof(uint32_t));
            vbyte::decode(buf,vbyte_bytes,out_ptr,out_size_u32);
        }
    }
};

struct streamvbytefse {
    static std::string name() { return "streamvbytefse"; }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        size_t vbyte_bytes = streamvbyte::encode(in_ptr,in_size_u32,buf,out_size_u8);
        auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
        *out_ptr_u32 = vbyte_bytes;
        auto stored_bytes = FSE_compress(out_ptr+sizeof(uint32_t),out_size_u8-sizeof(uint32_t),buf,vbyte_bytes);
        if(stored_bytes == 0) {
            *out_ptr_u32 = 0;
            memcpy(out_ptr+sizeof(uint32_t),buf,vbyte_bytes);
            return vbyte_bytes+sizeof(uint32_t);
        } else {
            return stored_bytes+sizeof(uint32_t);
        }
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
        size_t vbyte_bytes = *in_ptr_u32;
        in_ptr += +sizeof(uint32_t);
        if(vbyte_bytes == 0) {
            streamvbyte::decode(in_ptr,in_size_u8-sizeof(uint32_t),out_ptr,out_size_u32);
        } else {
            FSE_decompress(buf,vbyte_bytes,in_ptr,in_size_u8-sizeof(uint32_t));
            streamvbyte::decode(buf,vbyte_bytes,out_ptr,out_size_u32);
        }
    }
};


struct vbytehuffzero {
    static std::string name() { return "vbytehuff0"; }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        auto init_ptr = out_ptr;
        size_t vbyte_bytes = vbyte::encode(in_ptr,in_size_u32,buf,out_size_u8);
        auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
        *out_ptr_u32 = vbyte_bytes;
        out_ptr += sizeof(uint32_t);

        auto in_u8 = reinterpret_cast<const uint8_t*>(buf);
        auto in_size = vbyte_bytes;
        auto num_blocks = in_size / HUF_BLOCKSIZE_MAX;
        auto last_size = in_size % HUF_BLOCKSIZE_MAX;
        auto dst_capacity = out_size_u8 - sizeof(uint32_t);
        for(size_t i=0;i<num_blocks;i++) {
            auto compressed_size_u8 = HUF_compress(out_ptr+4,dst_capacity,in_u8,HUF_BLOCKSIZE_MAX);
            if(compressed_size_u8 == 0) { // not compressible just copy
                auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
                *out_ptr_u32 = 0xFFFFFFFF;
                memcpy(out_ptr+4,in_u8,HUF_BLOCKSIZE_MAX);
                out_ptr += HUF_BLOCKSIZE_MAX+4;
            } else {
                auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
                *out_ptr_u32 = compressed_size_u8;
                out_ptr += compressed_size_u8+4;
                dst_capacity -= (compressed_size_u8+4);
            }
            in_u8 += HUF_BLOCKSIZE_MAX;
        }
        if(last_size) {
            auto compressed_size_u8 = HUF_compress(out_ptr+4,dst_capacity,in_u8,last_size);
            if(compressed_size_u8 == 0) { // not compressible just copy
                auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
                *out_ptr_u32 = 0xFFFFFFFF;
                memcpy(out_ptr+4,in_u8,HUF_BLOCKSIZE_MAX);
                out_ptr += HUF_BLOCKSIZE_MAX+4;
            } else {
                auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
                *out_ptr_u32 = compressed_size_u8;
                out_ptr += compressed_size_u8+4;
            }
        }
        size_t bytes_written = out_ptr - init_ptr;
        return bytes_written;
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
        size_t vbyte_bytes = *in_ptr_u32;
        in_ptr += +sizeof(uint32_t);

        auto out_ptr_u8 = reinterpret_cast<uint8_t*>(buf);
        auto out_size = vbyte_bytes;
        auto num_blocks = out_size / HUF_BLOCKSIZE_MAX;
        auto last_size = out_size % HUF_BLOCKSIZE_MAX;
        auto dst_capacity = out_size_u32;
        for(size_t i=0;i<num_blocks;i++) {
            auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
            size_t compressed_size = *in_ptr_u32;
            if(compressed_size == 0xFFFFFFFF) { // not compressible just copy
                memcpy(out_ptr_u8,in_ptr+4,HUF_BLOCKSIZE_MAX);
                in_ptr += HUF_BLOCKSIZE_MAX+4;
            } else {
                HUF_decompress(out_ptr_u8,HUF_BLOCKSIZE_MAX,in_ptr+4,compressed_size);
                in_ptr += (compressed_size+4);
            }
            out_ptr_u8 += HUF_BLOCKSIZE_MAX;
        }
        if(last_size) {
            auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
            size_t compressed_size = *in_ptr_u32;
            if(compressed_size == 0xFFFFFFFF) { // not compressible just copy
                memcpy(out_ptr_u8,in_ptr+4,last_size);
            } else {
                HUF_decompress(out_ptr_u8,last_size,in_ptr+4,compressed_size);
            }
        }
        vbyte::decode(buf,vbyte_bytes,out_ptr,out_size_u32);
    }
};


struct streamvbytehuffzero {
    static std::string name() { return "streamvbytehuff0"; }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        auto init_ptr = out_ptr;
        size_t vbyte_bytes = streamvbyte::encode(in_ptr,in_size_u32,buf,out_size_u8);
        auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
        *out_ptr_u32 = vbyte_bytes;
        out_ptr += sizeof(uint32_t);

        auto in_u8 = reinterpret_cast<const uint8_t*>(buf);
        auto in_size = vbyte_bytes;
        auto num_blocks = in_size / HUF_BLOCKSIZE_MAX;
        auto last_size = in_size % HUF_BLOCKSIZE_MAX;
        auto dst_capacity = out_size_u8 - sizeof(uint32_t);
        for(size_t i=0;i<num_blocks;i++) {
            auto compressed_size_u8 = HUF_compress(out_ptr+4,dst_capacity,in_u8,HUF_BLOCKSIZE_MAX);
            if(compressed_size_u8 == 0) { // not compressible just copy
                auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
                *out_ptr_u32 = 0xFFFFFFFF;
                memcpy(out_ptr+4,in_u8,HUF_BLOCKSIZE_MAX);
                out_ptr += HUF_BLOCKSIZE_MAX+4;
            } else {
                auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
                *out_ptr_u32 = compressed_size_u8;
                out_ptr += compressed_size_u8+4;
                dst_capacity -= (compressed_size_u8+4);
            }
            in_u8 += HUF_BLOCKSIZE_MAX;
        }
        if(last_size) {
            auto compressed_size_u8 = HUF_compress(out_ptr+4,dst_capacity,in_u8,last_size);
            if(compressed_size_u8 == 0) { // not compressible just copy
                auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
                *out_ptr_u32 = 0xFFFFFFFF;
                memcpy(out_ptr+4,in_u8,HUF_BLOCKSIZE_MAX);
                out_ptr += HUF_BLOCKSIZE_MAX+4;
            } else {
                auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
                *out_ptr_u32 = compressed_size_u8;
                out_ptr += compressed_size_u8+4;
            }
        }
        size_t bytes_written = out_ptr - init_ptr;
        return bytes_written;
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
        size_t vbyte_bytes = *in_ptr_u32;
        in_ptr += +sizeof(uint32_t);

        auto out_ptr_u8 = reinterpret_cast<uint8_t*>(buf);
        auto out_size = vbyte_bytes;
        auto num_blocks = out_size / HUF_BLOCKSIZE_MAX;
        auto last_size = out_size % HUF_BLOCKSIZE_MAX;
        auto dst_capacity = out_size_u32;
        for(size_t i=0;i<num_blocks;i++) {
            auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
            size_t compressed_size = *in_ptr_u32;
            if(compressed_size == 0xFFFFFFFF) { // not compressible just copy
                memcpy(out_ptr_u8,in_ptr+4,HUF_BLOCKSIZE_MAX);
                in_ptr += HUF_BLOCKSIZE_MAX+4;
            } else {
                HUF_decompress(out_ptr_u8,HUF_BLOCKSIZE_MAX,in_ptr+4,compressed_size);
                in_ptr += (compressed_size+4);
            }
            out_ptr_u8 += HUF_BLOCKSIZE_MAX;
        }
        if(last_size) {
            auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
            size_t compressed_size = *in_ptr_u32;
            if(compressed_size == 0xFFFFFFFF) { // not compressible just copy
                memcpy(out_ptr_u8,in_ptr+4,last_size);
            } else {
                HUF_decompress(out_ptr_u8,last_size,in_ptr+4,compressed_size);
            }
        }
        streamvbyte::decode(buf,vbyte_bytes,out_ptr,out_size_u32);
    }
};

struct streamvbyteANS {
    static std::string name() { return "streamvbyteANS"; }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        size_t vbyte_bytes = streamvbyte::encode(in_ptr,in_size_u32,buf,out_size_u8);
        auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
        *out_ptr_u32 = vbyte_bytes;
        auto stored_bytes = ans_byte_compress(out_ptr+sizeof(uint32_t),out_size_u8-sizeof(uint32_t),buf,vbyte_bytes);
        return stored_bytes+sizeof(uint32_t);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
        size_t vbyte_bytes = *in_ptr_u32;
        in_ptr += +sizeof(uint32_t);
        ans_byte_decompress(buf,vbyte_bytes,in_ptr,in_size_u8-sizeof(uint32_t));
        streamvbyte::decode(buf,vbyte_bytes,out_ptr,out_size_u32);
    }
};


struct vbyteANS {
    static std::string name() { return "vbyteANS"; }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        size_t vbyte_bytes = vbyte::encode(in_ptr,in_size_u32,buf,out_size_u8);
        auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_ptr);
        *out_ptr_u32 = vbyte_bytes;
        auto stored_bytes = ans_byte_compress(out_ptr+sizeof(uint32_t),out_size_u8-sizeof(uint32_t),buf,vbyte_bytes);
        return stored_bytes+sizeof(uint32_t);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_ptr);
        size_t vbyte_bytes = *in_ptr_u32;
        in_ptr += +sizeof(uint32_t);
        ans_byte_decompress(buf,vbyte_bytes,in_ptr,in_size_u8-sizeof(uint32_t));
        vbyte::decode(buf,vbyte_bytes,out_ptr,out_size_u32);
    }
};

struct ANSint {
    static std::string name() { return std::string("ANS"); }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        return ans_int_compress(out_ptr,out_size_u8,in_ptr,in_size_u32);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        ans_int_decompress(out_ptr,out_size_u32,in_ptr,in_size_u8);
    }
};

struct ANSmsb {
    static std::string name() { return "ANSmsb"; }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        return ans_msb_compress(out_ptr,out_size_u8,in_ptr,in_size_u32);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        ans_msb_decompress(out_ptr,out_size_u32,in_ptr,in_size_u8);
    }
};

struct shuff {
    static std::string name() { return "shuff"; }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        return shuff_compress(out_ptr,out_size_u8,in_ptr,in_size_u32);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        shuff_decompress(out_ptr,out_size_u32,in_ptr,in_size_u8);
    }
};

template<uint32_t fidelity>
struct ANSfold {
    static std::string name() { return std::string("ANSfold-") + std::to_string(fidelity); }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        return ans_fold_compress<fidelity>(out_ptr,out_size_u8,in_ptr,in_size_u32);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        ans_fold_decompress<fidelity>(out_ptr,out_size_u32,in_ptr,in_size_u8);
    }
};

template<uint32_t fidelity>
struct ANSrfold {
    static std::string name() { return std::string("ANSrfold-") + std::to_string(fidelity); }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        return ans_reorder_fold_compress<fidelity>(out_ptr,out_size_u8,in_ptr,in_size_u32);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        ans_reorder_fold_decompress<fidelity>(out_ptr,out_size_u32,in_ptr,in_size_u8);
    }
};

struct arith {
    static std::string name() { return std::string("arith"); }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        return arith_compress(out_ptr,out_size_u8,in_ptr,in_size_u32);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        arith_decompress(out_ptr,out_size_u32,in_ptr,in_size_u8);
    }
};

template<uint32_t H_approx>
struct ANSsint {
    static std::string name() { return std::string("ANSsint-") + std::to_string(H_approx); }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        return ans_sint_compress<H_approx>(out_ptr,out_size_u8,in_ptr,in_size_u32);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        ans_sint_decompress(out_ptr,out_size_u32,in_ptr,in_size_u8);
    }
};

template<uint32_t H_approx>
struct ANSsmsb {
    static std::string name() { return std::string("ANSsmsb-") + std::to_string(H_approx); }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        return ans_smsb_compress<H_approx>(out_ptr,out_size_u8,in_ptr,in_size_u32);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        ans_smsb_decompress(out_ptr,out_size_u32,in_ptr,in_size_u8);
    }
};


struct entropy_only {
    static std::string name() { return std::string("entropy"); }

    static size_t encode(const uint32_t* in_ptr,size_t in_size_u32,uint8_t* out_ptr,size_t out_size_u8,uint8_t* buf = NULL)
    {
        auto [input_entropy,sigma] = compute_entropy(in_ptr,in_size_u32);
        return ceil((input_entropy*in_size_u32)/8.0);
    }
    static void decode(const uint8_t* in_ptr,size_t in_size_u8,uint32_t* out_ptr,size_t out_size_u32,uint8_t* buf = NULL)
    {
        std::cerr << "don't do this!!" << std::endl;
        exit(EXIT_FAILURE);
    }
};