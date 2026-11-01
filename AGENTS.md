# AGENTS.md — bkd

## Repo map

```
bkd.hh             The library: point, box, tree<D> (build, query, count).
test/test.cc       Queries and counts against brute force (1, 2, 3, 8 dimensions; leaf sizes 1-512); interval intersection.
fuzz/fuzz_tree.cc  libFuzzer: points and boxes from the input, checked against brute force.
CMakeLists.txt     INTERFACE target bkd::bkd; tests and the fuzz target when top-level.
docs/bkd.d2     README diagram; render: d2 --layout elk --sketch --theme 302 docs/bkd.d2 docs/bkd.svg
```

## Invariants

- A query reports every point inside the box exactly once, and nothing else.
- Every node's box bounds its points; a cell reported whole lies inside the query box.
- No recursion: build and query keep explicit stacks.

## Verification and benchmarks

The shared method is the catalog's
[PERFORMANCE.md](https://github.com/Kronuz/cpp-libraries/blob/main/docs/PERFORMANCE.md). Here the
oracle is brute force, from a fresh clone:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --test-dir build --output-on-failure
```

- **Test.** `bkd` (`test/test.cc`, seed 2026) builds trees of up to 5,000 uniform, clustered and
  duplicate-heavy points in 1, 2, 3 and 8 dimensions at leaf sizes 1, 7, 64 and 512, and checks 50
  boxes per tree: `query` emits exactly the ordinals a scan of every point finds, once each, and
  `count` agrees. It also checks interval intersection, an empty tree and a zero leaf size, and
  prints `bkd: ok`.
- **Fuzz.** `fuzz/fuzz_tree.cc` reads points, a leaf size and eight boxes from the input and
  aborts when a 2-D or 3-D tree disagrees with the scan. It needs a Clang with libFuzzer (on
  macOS, Homebrew's `llvm`) and builds with ASan and UBSan. No corpus or dictionary is committed;
  it starts from an empty directory, and a pass is a run that ends without an abort or a
  sanitizer report.

  ```sh
  cmake -S . -B build-fuzz -DBKD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++ && cmake --build build-fuzz
  mkdir -p corpus && build-fuzz/bkd_fuzz -max_total_time=60 corpus
  ```

- **CI.** `.github/workflows/ci.yml` calls the catalog's shared
  [`cmake.yml`](https://github.com/Kronuz/cpp-libraries/blob/main/.github/workflows/cmake.yml):
  Linux x86_64 (GCC, Clang) and arm64 (GCC), macOS arm64 and x86_64, and FreeBSD, at `-O2 -g` with
  asserts and the standard library's assertions on, plus ASan+UBSan and TSan jobs over the same
  tests (a built tree is read-only, and the tests use one thread). The ASan+UBSan job, on Linux:

  ```sh
  CC=clang CXX=clang++ cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug \
    "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS" \
    -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined
  cmake --build build-asan && ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan --output-on-failure
  ```

- **Not here.** No differential test, mutation script or benchmark, and the README makes no
  performance claim.
