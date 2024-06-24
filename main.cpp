#include <stdio.h>

#include "grep.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: grep PATTERN PATH\n"
                  << "Example: grep.out 123 ./grep.out";
        return EXIT_FAILURE;
    }
    std::string regex = argv[1];
    fs::path path = argv[2];

    if (!isValidRegex(regex) || regex.empty()) {
        std::cerr << "Invalid regex: '" << regex << "'" << std::endl;
        return EXIT_FAILURE;
    }
    if (!fs::exists(path)) {
        std::cerr << "Non existing path: " << path << std::endl;
        return EXIT_FAILURE;
    }

    FileRegexProcessor fp(std::thread::hardware_concurrency(), regex);

    FilesBuilder::Build(path, fp);

    while (!fp.IsFinished()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return EXIT_SUCCESS;
}
