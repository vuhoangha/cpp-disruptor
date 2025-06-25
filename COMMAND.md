<!-- DEBUG -->
```sh
rm -rf *
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --parallel
./src/main
```

<!-- RELEASE -->
```sh
rm -rf *
mkdir -p build-release
cd build-release
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF ..
cmake --build .
./src/main
```

<!-- Perf with VALGRIND -->
```sh
rm -rf callgrind.out.*
valgrind --tool=callgrind ./src/main
kcachegrind callgrind.out
```


<!-- Perf with PERF & HOTSPOT -->
```sh
rm -rf perf.data
perf record --call-graph dwarf ./src/main
hotspot perf.data
```