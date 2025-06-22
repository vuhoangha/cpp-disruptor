<!-- DEBUG -->
```sh
rm -rf *
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --parallel
valgrind --tool=callgrind ./src/main
kcachegrind callgrind.out.28352
```

<!-- RELEASE -->
```sh
mkdir -p build-release
cd build-release
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF ..
cmake --build .
./src/main
```