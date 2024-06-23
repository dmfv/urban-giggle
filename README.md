# urban-giggle

grep-like utility.
tested my multithreading skills

utility tested on linux (ubuntu) and windows
# dependencies:
* GTest

Based on some features of C++20 but could be easily rewrited on lower standards
For easier testing and running project uses only 1 CMakeLists file
For compilation use next:

mkdir build
cd build
cmake ..
cmake --build .

# To run tests:
make tests or ctest

Or you could always run tests manually as executable file

# Utility usage:
This utility super primitive and supports only one type of arguments
./MyGrep PATTERN PATH

You could compare results of this utility and regular grep/pgrep
in test folder (tests/test_folder_2)

# Classical grep unility equivalents:
This MyGrep arguments could be equivalent to this grep arguments:
MyGrep works with all files by default (doesn't skip binaries or others files)

MyGrep 123 tests
grep -ra tests

MyGrep 123 tests/test_folder_1/100.txt
grep -raH tests/test_folder_1/100.txt