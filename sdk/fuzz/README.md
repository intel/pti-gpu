# PTI fuzz testing

We are using [libFuzzer](https://llvm.org/docs/LibFuzzer.html) for fuzz
testing.

## Build

Build PTI for fuzzing targets.

(Using CMake [presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html))

```console
>> source <path_to_pti>/setvars.sh
>> cmake --preset fuzz
>> cd build
>> ninja -j $(nproc)
```

--or--

```console
>> source <path_to_pti>/setvars.sh
>> mkdir build
>> cd build
>> cmake -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/clang_toolchain.cmake \
    -DPTI_FUZZ=1 ..
>> make -j
```

## Run

```console
>> cd build
>> ctest --verbose fuzz-pti-view-lib
```
