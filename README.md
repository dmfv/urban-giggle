# Urban Giggle
Urban Giggle is a grep-like utility demonstrating multithreading skills. Tested on Linux (Ubuntu) and Windows.

# Features
* Written with using C++20 features (can be adapted to lower standards)
* Multithreading support for faster searches
* Command-line interface similar to the classic grep command
* Testing with GTest
* Build system using CMake

#Requirements
* C++20 compatible compiler
* CMake
* GTest

For compilation use next:

```bash
mkdir build
```
```bash
cd build
```
```bash
cmake ..
```
```bash
cmake --build .
```

# To run tests:
```bash
make tests
```
or 
```bash
ctest
```

Or you could always run tests manually as executable file

# Utility usage:
This utility super primitive and supports only one type of arguments
```bash
./MyGrep PATTERN PATH
```

You could compare results of this utility and regular grep/pgrep
in test folder (tests/test_folder_2)

# Classical grep unility equivalents:
This MyGrep arguments could be equivalent to this grep arguments:
MyGrep works with all files by default (doesn't skip binaries or others files)

```bash
MyGrep 123 tests
grep -ra tests
```

```bash
MyGrep 123 tests/test_folder_1/100.txt
grep -raH tests/test_folder_1/100.txt
```
