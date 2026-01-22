# Scheme Switching

Scheme Switching is an experimental compiler toolchain that targets multiple Fully Homomorphic Encryption (FHE) schemes (TFHE, CKKS, BGV, and BinFHE). The repository bundles:

- an OpenFHE-backed runtime (`scheme_switching` library) plus a handful of examples under `examples/`,
- a prototype MLIR front-end that lowers MLIR modules into C++ (see `src/frontend/compiler.cpp`),
- helper scripts to exercise the Polygeist/MLIR-based workflow.

## Repository layout

- `examples/` - runnable samples that exercise the operations implemented in `scheme_switching`.
- `src/` - MLIR front-end and lowering pipeline. Enable with `-DBUILD_MLIR_COMPILER=ON`.
- `scripts/` - helper shell scripts (`build_chess.sh`, `polygeist.sh`, `compile.sh`, `run.sh`) that demonstrate the MLIR -> C++ path.
- `test/` - harness used while developing the OpenFHE kernels.

## Requirements

- CMake 3.16+ and Ninja or GNU Make.
- A C++17 toolchain (Clang >= 12 or GCC >= 10) with OpenMP if you want to run the parallel experiments.
- Python 3 (used by some MLIR/LLVM utilities).
- [OpenFHE](https://github.com/openfheorg/openfhe-development) installed with its CMake package (either shared or static builds work).
- Optional: LLVM/MLIR (for the compiler target and MLIR tooling), Polygeist's `cgeist` if you plan to reuse the helper scripts.

### Installing OpenFHE

```
git clone https://github.com/openfheorg/openfhe-development.git
cmake -S openfhe-development -B build/openfhe \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/openfhe \
  -DBUILD_SHARED=ON
cmake --build build/openfhe --target install -j
```

Set `OpenFHE_DIR` to the directory that contains `OpenFHEConfig.cmake` (for the example above it is `/opt/openfhe/lib/cmake/OpenFHE`).

### Installing LLVM / MLIR

```
git clone https://github.com/llvm/llvm-project.git
cmake -S llvm-project/llvm -B build/llvm \
  -G Ninja \
  -DLLVM_ENABLE_PROJECTS="mlir;clang" \
  -DLLVM_TARGETS_TO_BUILD="host" \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_INCLUDE_EXAMPLES=OFF \
  -DLLVM_INCLUDE_TESTS=OFF \
  -DMLIR_ENABLE_BINDINGS_PYTHON=OFF
cmake --build build/llvm --target mlir-opt mlir-translate
cmake --install build/llvm --prefix /opt/llvm
```

Expose the package configuration files to CMake:

- `MLIR_DIR=/opt/llvm/lib/cmake/mlir`
- `LLVM_DIR=/opt/llvm/lib/cmake/llvm`

If you want to follow the Polygeist-based path (as documented in `scripts/compile.sh`), also build Polygeist so that `cgeist`, `mlir-opt`, and `mlir-translate` are on your `PATH`.

## Configuring and building

Use the top-level CMake project to build the OpenFHE components and, optionally, the MLIR compiler. If OpenFHE is installed under a standard prefix (for example `/usr/local` or `/opt/openfhe`), a plain configure works:

```
cmake -S . -B build
cmake --build build
```

If CMake cannot locate OpenFHE, provide the package directory explicitly:

```
cmake -S . -B build \
  -DOpenFHE_DIR=/opt/openfhe/lib/cmake/OpenFHE
cmake --build build
```

To enable the MLIR compiler, add the MLIR/LLVM package locations if they are not in a standard prefix:

```
cmake -S . -B build \
  -DOpenFHE_DIR=/opt/openfhe/lib/cmake/OpenFHE \
  -DBUILD_MLIR_COMPILER=ON \
  -DMLIR_DIR=/opt/llvm/lib/cmake/mlir \
  -DLLVM_DIR=/opt/llvm/lib/cmake/llvm
cmake --build build
```

Useful cache options:

- `-DBUILD_EXAMPLES=OFF` stops building the executables under `examples/`.
- `-DBUILD_STATIC=ON` links everything against the static OpenFHE libraries. Pass this together with `-DBUILD_SHARED=OFF` when configuring OpenFHE.
- `-DBUILD_MLIR_COMPILER=ON` enables the MLIR toolchain under `src/`. Leave it `OFF` if you only want the OpenFHE examples.

The build produces the `scheme_switching` static library along with the `main`, `example`, `ir`, and `experiments` executables in the chosen build directory.

## Scripted workflow (step-by-step)

The four scripts under `scripts/` implement a minimal pipeline from C++ → MLIR → generated C++ → run.

1) Build the MLIR compiler (`chess`):
```
./scripts/build_chess.sh clang
```

2) Generate MLIR from a simple input under `examples/input/`:
```
./scripts/polygeist.sh compare_lt
```
This reads `examples/input/compare_lt.cpp` and writes `examples/mlir/compare_lt.mlir`.

3) Lower MLIR to generated C++:
```
./scripts/compile.sh compare_lt
```
This reads `examples/mlir/compare_lt.mlir` and writes `examples/output/compare_lt.cpp`. (No extra MLIR files are emitted.)

4) Compile and run the generated C++:
```
./scripts/run.sh compare_lt secure
```
You can also pass `debug` to use insecure parameters.

Notes:
- `polygeist.sh` requires `cgeist` and `mlir-opt` in `PATH` (or via `POLYGEIST_ROOT`).
- `compile.sh` requires `mlir-opt` and a built `chess` binary.
- `run.sh` compiles the output C++ on the fly and accepts an optional mode string.

## Running an example

```
cmake --build build --target experiments
./build/experiments
```

`examples/experiments.cpp` sets up a BinFHE context, encrypts random vectors, and evaluates a parallel minimum-index search. You can also run `./build/main` for a larger CKKS/TFHE demonstration or `./build/ir` to inspect the IR-focused sample.

## Building only the MLIR compiler

If you prefer to keep the MLIR toolchain in a standalone build tree, configure the top-level project with `BUILD_MLIR_COMPILER` enabled:

```
cmake -S . -B build/compiler \
  -DBUILD_MLIR_COMPILER=ON \
  -DMLIR_DIR=/opt/llvm/lib/cmake/mlir \
  -DLLVM_DIR=/opt/llvm/lib/cmake/llvm
cmake --build build/compiler --target chess
```

The `scripts/compile.sh` helper shows one way of producing MLIR with `cgeist`, optimizing it with `mlir-opt`, translating to C++, and stitching the generated sources into the OpenFHE runtime. The `chess` binary is the MLIR-to-OpenFHE C++ compiler.
