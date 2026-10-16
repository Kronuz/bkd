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

## Verify a change

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --test-dir build
cmake -S . -B build-fuzz -DBKD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++ && cmake --build build-fuzz && build-fuzz/bkd_fuzz -max_total_time=60
```
