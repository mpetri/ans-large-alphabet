cmake_minimum_required (VERSION 3.2.2)
project (ans-large-alphabet C CXX)

find_package(Boost COMPONENTS program_options filesystem regex REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -O3" )

include_directories(${CMAKE_SOURCE_DIR}/include/)
include_directories(${CMAKE_SOURCE_DIR}/third-party/FastPFor/headers/)
include_directories(${CMAKE_SOURCE_DIR}/third-party/streamvbyte/include/)
include_directories(${CMAKE_SOURCE_DIR}/third-party/FiniteStateEntropy/lib/)

add_library(FastPFor ${CMAKE_SOURCE_DIR}/third-party/FastPFor/src/bitpacking.cpp
                     ${CMAKE_SOURCE_DIR}/third-party/FastPFor/src/bitpackingaligned.cpp
                     ${CMAKE_SOURCE_DIR}/third-party/FastPFor/src/bitpackingunaligned.cpp
                     ${CMAKE_SOURCE_DIR}/third-party/FastPFor/src/simdunalignedbitpacking.cpp
                     ${CMAKE_SOURCE_DIR}/third-party/FastPFor/src/simdbitpacking.cpp)

add_library(streamvbyte ${CMAKE_SOURCE_DIR}/third-party/streamvbyte/src/streamvbyte_encode.c
                     ${CMAKE_SOURCE_DIR}/third-party/streamvbyte/src/streamvbyte_decode.c
                     ${CMAKE_SOURCE_DIR}/third-party/streamvbyte/src/streamvbyte_zigzag.c
                     ${CMAKE_SOURCE_DIR}/third-party/streamvbyte/src/streamvbytedelta_encode.c
                     ${CMAKE_SOURCE_DIR}/third-party/streamvbyte/src/streamvbytedelta_decode.c
                     ${CMAKE_SOURCE_DIR}/third-party/streamvbyte/src/streamvbyte_0124_encode.c
                     ${CMAKE_SOURCE_DIR}/third-party/streamvbyte/src/streamvbyte_0124_decode.c)

add_library(FiniteStateEntropy ${CMAKE_SOURCE_DIR}/third-party/FiniteStateEntropy/lib/debug.c
                     ${CMAKE_SOURCE_DIR}/third-party/FiniteStateEntropy/lib/entropy_common.c
                     ${CMAKE_SOURCE_DIR}/third-party/FiniteStateEntropy/lib/fse_compress.c
                     ${CMAKE_SOURCE_DIR}/third-party/FiniteStateEntropy/lib/fse_decompress.c
                     ${CMAKE_SOURCE_DIR}/third-party/FiniteStateEntropy/lib/hist.c
                     ${CMAKE_SOURCE_DIR}/third-party/FiniteStateEntropy/lib/huf_compress.c
                     ${CMAKE_SOURCE_DIR}/third-party/FiniteStateEntropy/lib/huf_decompress.c)


# OUR BINS
add_executable(benchmark.x src/benchmark.cpp)
target_link_libraries(benchmark.x FastPFor streamvbyte FiniteStateEntropy ${Boost_LIBRARIES})

add_executable(generate_inputs.x src/generate_inputs.cpp)
target_link_libraries(generate_inputs.x ${Boost_LIBRARIES})

add_executable(generate_bwtmtf.x src/generate_bwtmtf.cpp)
target_link_libraries(generate_bwtmtf.x ${Boost_LIBRARIES})

add_executable(shuff.x src/shuff.cpp)
target_link_libraries(shuff.x  ${Boost_LIBRARIES})

add_executable(rescale_freqs.x src/rescale_freqs.cpp)
target_link_libraries(rescale_freqs.x  ${Boost_LIBRARIES})

add_executable(lower_order_entropy.x src/lower_order_entropy.cpp)
target_link_libraries(lower_order_entropy.x  ${Boost_LIBRARIES})

add_executable(prelude.x src/prelude.cpp)
target_link_libraries(prelude.x  ${Boost_LIBRARIES})

add_executable(table_effectiveness.x src/table_effectiveness.cpp)
target_link_libraries(table_effectiveness.x FastPFor streamvbyte FiniteStateEntropy ${Boost_LIBRARIES})

add_executable(fold_effectiveness.x src/fold_effectiveness.cpp)
target_link_libraries(fold_effectiveness.x FastPFor streamvbyte FiniteStateEntropy ${Boost_LIBRARIES})

add_executable(bucket_skew.x src/bucket_skew.cpp)
target_link_libraries(bucket_skew.x FastPFor streamvbyte FiniteStateEntropy ${Boost_LIBRARIES})

add_executable(table_efficiency.x src/table_efficiency.cpp)
target_link_libraries(table_efficiency.x FastPFor streamvbyte FiniteStateEntropy ${Boost_LIBRARIES})

add_executable(pseudo_adaptive.x src/pseudo_adaptive.cpp)
target_link_libraries(pseudo_adaptive.x FastPFor streamvbyte FiniteStateEntropy ${Boost_LIBRARIES})
