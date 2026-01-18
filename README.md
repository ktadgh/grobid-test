# grobid-test
Building
```bash
rm -rf build
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.2/macos"
cmake --build . --parallel 8
./grobid_test
```